#ifndef VM_COMPUTE_SCAN_H
#define VM_COMPUTE_SCAN_H
#include "compute/context.h"

namespace vm {
namespace compute {

class Scan {
    static const constexpr size_t BLOCK_SIZE = 1024;

    size_t m_input_size;
    size_t m_aligned_size;
    /**
     * Arrays keeping temporaries generated during scan on number of elements
     * that exceed BLOCK_SIZE (which is 1024 actually).
     */
    std::vector<::compute::vector<uint32_t>> m_phases;
    ::compute::kernel m_local_inclusive_scan;
    ::compute::kernel m_fixup_scan;

public:
    Scan(::compute::command_queue &queue,
         size_t input_size);

    /**
     * Performs an inclusive prefix-sum on the specified input. NOTE: one shall
     * likely wait for the returned event to complete.
     *
     * @param input     Input vector to scan through.
     * @param output    Output where scanned prefix-sum will be written.
     * @param queue     Compute queue on which this task will be placed.
     */
    ::compute::event inclusive_scan(::compute::vector<uint32_t> &input,
                                    ::compute::vector<uint32_t> &output,
                                    ::compute::command_queue &queue);
};

} // namespace compute
} // namespace vm

#endif /* VM_COMPUTE_SCAN_H */
