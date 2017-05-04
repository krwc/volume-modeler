#include "compute-context.h"
#include "utils/log.h"

using namespace std;

namespace vm {

ComputeContext::ComputeContext(MakeSharedEnabler)
    : context(move(compute::opengl_create_shared_context()))
    , queue(context, context.get_device()) {
    LOG(info) << "Initialized OpenCL context";
    LOG(info) << "device      : " << context.get_device().name();
    LOG(info) << "vendor      : " << context.get_device().vendor();
    LOG(info) << "profile     : " << context.get_device().profile();
    LOG(info) << "memory (MB) : " << (context.get_device().global_memory_size() / double(1 << 20));
}

}
