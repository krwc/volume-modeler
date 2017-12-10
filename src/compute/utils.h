#ifndef VM_COMPUTE_UTILS_H
#define VM_COMPUTE_UTILS_H

#include "compute/context.h"

#include <stdexcept>

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
    (void) retval;
    return event;
}

static inline compute::event
enqueue_write_image3d(compute::command_queue &queue,
                      compute::image3d &image,
                      const void *hostptr) {
    return queue.enqueue_write_image<3>(
            image, compute::dim(0, 0, 0), image.size(), hostptr);
}

static inline size_t
image_format_size(const compute::image_format &format) {
    auto num_channels = [&]() {
        switch (format.get_format_ptr()->image_channel_order) {
        case CL_R:
        case CL_A:
        case CL_INTENSITY:
        case CL_LUMINANCE:
            return 1;
        case CL_RG:
        case CL_RA:
            return 2;
        case CL_RGB:
            return 3;
        case CL_RGBA:
        case CL_ARGB:
        case CL_BGRA:
            return 4;
        default:
            throw std::invalid_argument("unsupported channel ordering");
        }
    };
    auto channel_size = [&]() {
        switch (format.get_format_ptr()->image_channel_data_type) {
        case CL_SIGNED_INT8:
        case CL_UNSIGNED_INT8:
            return 1;
        case CL_SIGNED_INT16:
        case CL_UNSIGNED_INT16:
        case CL_HALF_FLOAT:
            return 2;
        default:
            throw std::invalid_argument("unsupported channel format");
        }
    };
    return num_channels() * channel_size();
}

} // namespace vm

#endif
