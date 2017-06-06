#include "compute/context.h"
#include "utils/log.h"

using namespace std;

namespace vm {

ComputeContext::ComputeContext(MakeSharedEnabler, bool gl_shared) {
    if (gl_shared) {
        context = move(compute::opengl_create_shared_context());
    } else {
        context = move(compute::context(compute::system::default_device()));
    }
    queue = move(compute::command_queue(context, context.get_device()));
    LOG(info) << "Initialized OpenCL context";
    LOG(info) << "device      : " << context.get_device().name();
    LOG(info) << "vendor      : " << context.get_device().vendor();
    LOG(info) << "profile     : " << context.get_device().profile();
    LOG(info) << "memory (MB) : "
              << (context.get_device().global_memory_size() / double(1 << 20));
}

compute::command_queue ComputeContext::make_out_of_order_queue() const {
    return compute::command_queue(
            context,
            context.get_device(),
            compute::command_queue::enable_out_of_order_execution);
}
}
