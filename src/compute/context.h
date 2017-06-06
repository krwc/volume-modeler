#ifndef VM_GFX_COMPUTE_CONTEXT_H
#define VM_GFX_COMPUTE_CONTEXT_H

#undef BOOST_COMPUTE_USE_OFFLINE_CACHE
#define BOOST_COMPUTE_DEBUG_KERNEL_COMPILATION

#include <CL/cl.h>
#include <GL/glew.h>
/* Unfortunately CL/cl.h defines this, and boost::compute relies on that, while
 * OpenCL 2.0 is not available */
#undef CL_VERSION_2_0

#include <boost/compute/container/vector.hpp>
#include <boost/compute/core.hpp>
#include <boost/compute/image.hpp>
#include <boost/compute/interop/opengl.hpp>
#include <boost/compute/utility/dim.hpp>
#include <memory>
#include <mutex>

namespace compute = boost::compute;
namespace vm {

class ComputeContext {
    struct MakeSharedEnabler {};
    friend std::shared_ptr<ComputeContext> make_gl_shared_compute_context();
    friend std::shared_ptr<ComputeContext> make_compute_context();

public:
    compute::context context;
    compute::command_queue queue;
    std::mutex queue_mutex;

    ComputeContext(MakeSharedEnabler, bool gl_shared);

    /** Creates an out of order queue using initialized context and device */
    compute::command_queue make_out_of_order_queue() const;
};

inline std::shared_ptr<ComputeContext> make_gl_shared_compute_context() {
    return std::make_shared<ComputeContext>(ComputeContext::MakeSharedEnabler{},
                                            true);
}

inline std::shared_ptr<ComputeContext> make_compute_context() {
    return std::make_shared<ComputeContext>(ComputeContext::MakeSharedEnabler{},
                                            false);
}

} // namespace vm

#endif // VM_GFX_COMPUTE_CONTEXT
