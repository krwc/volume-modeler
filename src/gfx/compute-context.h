#ifndef VM_GFX_COMPUTE_CONTEXT_H
#define VM_GFX_COMPUTE_CONTEXT_H

#include <boost/compute/core.hpp>
#include <memory>

namespace vm {

class ComputeContext {
    struct MakeSharedEnabler {};
    friend std::shared_ptr<ComputeContext> make_compute_context();

public:
    boost::compute::device device;
    boost::compute::context context;
    boost::compute::command_queue queue;

    ComputeContext(MakeSharedEnabler);
};

inline std::shared_ptr<ComputeContext> make_compute_context() {
    return std::make_shared<ComputeContext>(
            ComputeContext::MakeSharedEnabler{});
}

} // namespace vm

#endif // VM_GFX_COMPUTE_CONTEXT
