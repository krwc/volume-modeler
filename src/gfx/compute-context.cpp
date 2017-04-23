#include "compute-context.h"
#include <iostream>

using namespace std;

namespace vm {

ComputeContext::ComputeContext(MakeSharedEnabler)
    : context(move(compute::opengl_create_shared_context()))
    , queue(context, context.get_device()) {
    cerr << "Initialized OpenCL context with" << endl
         << "* device       : " << context.get_device().name() << endl
         << "* vendor       : " << context.get_device().vendor() << endl
         << "* profile      : " << context.get_device().profile() << endl
         << "* memory (MB)  : " << (context.get_device().global_memory_size() / double(1 << 20))
         << endl;
}

}
