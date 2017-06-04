#ifndef VM_COMPUTE_UTILS_H
#define VM_COMPUTE_UTILS_H

#include "compute/context.h"

namespace vm {

// Boost.Compute does not have templated overload that allows to run nd-range
// kernel with automatic local work distribution, but it would be nice to have
// one.
template <size_t N>
compute::event enqueue_auto_distributed_nd_range_kernel(
        compute::command_queue &queue,
        const compute::kernel &kernel,
        const compute::extents<N> &global_work_size,
        const compute::wait_list &events = compute::wait_list()) {
    return queue.enqueue_nd_range_kernel(
            kernel, N, nullptr, global_work_size.data(), nullptr, events);
}

} // namespace vm

#endif
