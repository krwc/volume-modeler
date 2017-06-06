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

static inline compute::event
enqueue_read_image3d_async(compute::command_queue &queue,
                           const compute::image3d &image,
                           void *hostptr) {
    compute::event event;
    cl_int retval = clEnqueueReadImage(queue.get(),
                                       image.get(),
                                       CL_FALSE,
                                       compute::dim(0, 0, 0).data(),
                                       image.size().data(),
                                       0,
                                       0,
                                       hostptr,
                                       0,
                                       nullptr,
                                       &event.get());
    assert(retval == CL_SUCCESS);
    return event;
}

static inline compute::event
enqueue_write_image3d(compute::command_queue &queue,
                      compute::image3d &image,
                      const void *hostptr) {
    return queue.enqueue_write_image<3>(
            image, compute::dim(0, 0, 0), image.size(), hostptr);
}

} // namespace vm

#endif
