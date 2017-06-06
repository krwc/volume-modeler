#include "scan.h"

#include "utils/log.h"

#include <sstream>
#include <stdexcept>
using namespace std;

namespace vm {

static const char *scan_source = R"(
void swap_ints(int *a, int *b) {
    int temp = *a;
    *a = *b;
    *b = temp;
}

kernel void local_scan(global uint *input,
                       global uint *output,
                       global uint *next,
                       uint size) {
    local uint temp[2][BLK_SIZE];
    const int l_tid = get_local_id(0);
    const int g_tid = get_global_id(0);

    int po = 0;
    int pi = 1;

    if (g_tid < size) {
        temp[po][l_tid] = input[g_tid];
    } else {
        temp[po][l_tid] = 0;
    }
    barrier(CLK_LOCAL_MEM_FENCE);

    for (uint offset = 1; offset < BLK_SIZE; offset *= 2) {
        swap_ints(&po, &pi);
        if (l_tid >= offset) {
            temp[po][l_tid] = temp[pi][l_tid] + temp[pi][l_tid - offset];
        } else {
            temp[po][l_tid] = temp[pi][l_tid];
        }
        barrier(CLK_LOCAL_MEM_FENCE);
    }

    if (g_tid < size) {
        output[g_tid] = temp[po][l_tid];
    }

    if (l_tid == 0 && next) {
        next[get_group_id(0)] = temp[po][BLK_SIZE - 1];
    }
}

kernel void fixup_scan(global uint *output,
                       global const uint *next,
                       uint size) {
    const uint index = BLK_SIZE + get_global_id(0);
    local uint value;
    value = next[get_group_id(0)];
    barrier(CLK_LOCAL_MEM_FENCE);
    if (index < size) {
        output[index] += value;
    }
}

)";

namespace {
static size_t align_to_block_size(size_t input, size_t block_size) {
    if (input % block_size) {
        return input + (block_size - input % block_size);
    }
    return input;
}
} // namespace

Scan::Scan()
        : m_input_size(0)
        , m_aligned_size(0)
        , m_phases()
        , m_local_inclusive_scan()
        , m_fixup_scan() {}

Scan::Scan(compute::command_queue &queue, size_t input_size)
        : m_input_size(input_size)
        , m_aligned_size(align_to_block_size(input_size, Scan::BLOCK_SIZE))
        , m_phases()
        , m_local_inclusive_scan()
        , m_fixup_scan() {
    {
        auto program =
                compute::program::create_with_source(scan_source,
                                                     queue.get_context());
        std::ostringstream inclusive_opts;
        inclusive_opts << " -DBLK_SIZE=" << Scan::BLOCK_SIZE;
        program.build(inclusive_opts.str());
        m_local_inclusive_scan = program.create_kernel("local_scan");
        m_fixup_scan = program.create_kernel("fixup_scan");
    }

    size_t num_phases = 0;
    {
        size_t size = m_input_size;
        while (size > Scan::BLOCK_SIZE) {
            ++num_phases;
            size /= Scan::BLOCK_SIZE;

            /* Each array must be aligned to be a multiple of a block */
            const size_t array_size =
                    align_to_block_size(size, Scan::BLOCK_SIZE);
            m_phases.emplace_back(
                    compute::vector<uint32_t>(array_size, 0, queue));
            LOG(trace) << "Allocated buffer for " << array_size
                       << " elements on phase " << num_phases << endl;
        }
    }
}

compute::event Scan::inclusive_scan(compute::vector<uint32_t> &input,
                                    compute::vector<uint32_t> &output,
                                    compute::command_queue &queue,
                                    const compute::wait_list &events) {
    assert(input.size() == m_input_size);
    assert(output.size() >= m_input_size);

    m_local_inclusive_scan.set_arg(0, input);
    m_local_inclusive_scan.set_arg(1, output);
    if (m_phases.size()) {
        m_local_inclusive_scan.set_arg(2, m_phases[0]);
    } else {
        m_local_inclusive_scan.set_arg(2, NULL);
    }
    m_local_inclusive_scan.set_arg(3, static_cast<cl_uint>(m_input_size));
    compute::event event;
    event = queue.enqueue_1d_range_kernel(m_local_inclusive_scan,
                                          0,
                                          m_aligned_size,
                                          Scan::BLOCK_SIZE,
                                          events);

    const size_t num_fixup_phases = m_phases.size();
    for (size_t j = 1; j <= num_fixup_phases; ++j) {
        m_local_inclusive_scan.set_arg(0, m_phases[j - 1]);
        m_local_inclusive_scan.set_arg(1, m_phases[j - 1]);
        if (j < num_fixup_phases) {
            m_local_inclusive_scan.set_arg(2, m_phases[j]);
        } else {
            m_local_inclusive_scan.set_arg(2, NULL);
        }
        m_local_inclusive_scan.set_arg(
                3, static_cast<cl_uint>(m_phases[j - 1].size()));
        event = queue.enqueue_1d_range_kernel(m_local_inclusive_scan,
                                              0,
                                              m_phases[j - 1].size(),
                                              Scan::BLOCK_SIZE,
                                              event);
    }

    if (num_fixup_phases) {
        for (size_t j = num_fixup_phases - 1; j >= 1; --j) {
            m_fixup_scan.set_arg(0, m_phases[j - 1]);
            m_fixup_scan.set_arg(1, m_phases[j]);
            m_fixup_scan.set_arg(2,
                                 static_cast<cl_uint>(m_phases[j - 1].size()));
            event = queue.enqueue_1d_range_kernel(m_fixup_scan,
                                                  0,
                                                  m_phases[j - 1].size(),
                                                  Scan::BLOCK_SIZE,
                                                  event);
        }
        m_fixup_scan.set_arg(0, output);
        m_fixup_scan.set_arg(1, m_phases[0]);
        m_fixup_scan.set_arg(2, static_cast<cl_uint>(output.size()));
        event = queue.enqueue_1d_range_kernel(m_fixup_scan,
                                              0,
                                              m_aligned_size - Scan::BLOCK_SIZE,
                                              Scan::BLOCK_SIZE,
                                              event);
    }
    return event;
}

} // namespace vm
