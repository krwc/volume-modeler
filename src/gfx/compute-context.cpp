#include "compute-context.h"
#include <iostream>

namespace compute = boost::compute;
using namespace std;

namespace vm {

ComputeContext::ComputeContext(MakeSharedEnabler)
    : device(compute::system::default_device())
    , context(device)
    , queue(context, device) {
    cerr << "Initialized OpenCL context with" << endl
         << "* device       : " << device.name() << endl
         << "* vendor       : " << device.vendor() << endl
         << "* profile      : " << device.profile() << endl
         << "* memory (MB)  : " << (device.global_memory_size() / double(1 << 20))
         << endl;
}

}
