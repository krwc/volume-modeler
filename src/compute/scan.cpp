#include "scan.h"

#include "utils/log.h"

#include <stdexcept>
#include <sstream>
using namespace std;

namespace vm {
namespace compute {

static const char *scan_source = R"(
void swap_ints(int *a, int *b) {
    int temp = *a;
    *a = *b;
    *b = temp;
}

kernel void local_scan(global uint *input,
                       global uint *output,
                       global uint *next) {
    local uint temp[2][BLK_SIZE];
    const int l_tid = get_local_id(0);
    const int g_tid = get_global_id(0);

    int po = 0;
    int pi = 1;

    temp[po][l_tid] = input[g_tid];
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

    output[g_tid] = temp[po][l_tid];

    if (l_tid == 0 && next) {
        next[get_group_id(0)] = temp[po][BLK_SIZE - 1];
    }
}

kernel void fixup_scan(global uint *output,
                       global const uint *next) {
    local uint value;
    value = next[get_group_id(0)];
    barrier(CLK_LOCAL_MEM_FENCE);
    output[BLK_SIZE + get_global_id(0)] += value;
}
)";

Scan::Scan(::compute::command_queue &queue,
           size_t input_size)
        : m_input_size(input_size)
        , m_phases()
        , m_local_inclusive_scan()
        , m_fixup_scan() {
    if (input_size % Scan::BLOCK_SIZE != 0) {
        throw invalid_argument("input_size must be a Scan::BLOCK_SIZE multiple");
    }

    {
        auto program = ::compute::program::create_with_source(scan_source,
                                                              queue.get_context());
        std::ostringstream inclusive_opts;
        inclusive_opts << " -DBLK_SIZE=" << Scan::BLOCK_SIZE;
        program.build(inclusive_opts.str());
        m_local_inclusive_scan = program.create_kernel("local_scan");
        m_fixup_scan = program.create_kernel("fixup_scan");
    }

    size_t num_phases = 0;
    {
        size_t size = input_size;
        while (size >= Scan::BLOCK_SIZE) {
            ++num_phases;
            size /= Scan::BLOCK_SIZE;

            /* Each array must be aligned to be a multiple of a block */
            const size_t array_size =
                    size + (Scan::BLOCK_SIZE - size % Scan::BLOCK_SIZE);
            m_phases.emplace_back(
                    ::compute::vector<uint32_t>(array_size, 0, queue));
            LOG(trace) << "Allocated buffer for " << array_size
                       << " elements on phase " << num_phases << endl;
        }
    }
}

::compute::event Scan::inclusive_scan(::compute::vector<uint32_t> &input,
                                      ::compute::vector<uint32_t> &output,
                                      ::compute::command_queue &queue) {
    assert(input.size() == m_input_size);
    assert(output.size() >= m_input_size);

    m_local_inclusive_scan.set_arg(0, input);
    m_local_inclusive_scan.set_arg(1, output);
    if (m_phases.size()) {
        m_local_inclusive_scan.set_arg(2, m_phases[0]);
    } else {
        m_local_inclusive_scan.set_arg(2, NULL);
    }
    queue.enqueue_1d_range_kernel(m_local_inclusive_scan, 0, m_input_size,
                                  Scan::BLOCK_SIZE);

    ::compute::event event;
    const size_t num_fixup_phases = m_phases.size();
    for (size_t j = 1; j <= num_fixup_phases; ++j) {
        m_local_inclusive_scan.set_arg(0, m_phases[j - 1]);
        m_local_inclusive_scan.set_arg(1, m_phases[j - 1]);
        if (j < num_fixup_phases) {
            m_local_inclusive_scan.set_arg(2, m_phases[j]);
        } else {
            m_local_inclusive_scan.set_arg(2, NULL);
        }
        event = queue.enqueue_1d_range_kernel(
                m_local_inclusive_scan, 0, m_phases[j - 1].size(), Scan::BLOCK_SIZE);
    }

    if (num_fixup_phases) {
        for (size_t j = num_fixup_phases - 1; j >= 1; --j) {
            m_fixup_scan.set_arg(0, m_phases[j - 1]);
            m_fixup_scan.set_arg(1, m_phases[j]);
            queue.enqueue_1d_range_kernel(
                    m_fixup_scan, 0, m_phases[j - 1].size(), Scan::BLOCK_SIZE);
        }
        m_fixup_scan.set_arg(0, output);
        m_fixup_scan.set_arg(1, m_phases[0]);
        event = queue.enqueue_1d_range_kernel(m_fixup_scan, 0,
                                              m_input_size - Scan::BLOCK_SIZE,
                                              Scan::BLOCK_SIZE);
    }
    return event;
}

} // namespace compute
} // namespace vm
