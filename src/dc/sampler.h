#ifndef VM_DC_SAMPLER_H
#define VM_DC_SAMPLER_H
#include "compute/context.h"

#include <array>

namespace vm {
class Chunk;
class Brush;

namespace dc {

class Sampler {
    std::shared_ptr<ComputeContext> m_compute_ctx;
    struct SDFSampler {
        /* Volume sampler */
        compute::kernel sampler;
        /* Edge updater */
        compute::kernel updater;
    };
    std::array<SDFSampler, 2> m_sdf_samplers;

public:
    enum class Operation {
        Add = 0,
        Sub = 1
    };

    /**
     * Initializes brush sampler.
     *
     * @param compute_ctx   Compute context to perform sampling operations on.
     */
    Sampler(const std::shared_ptr<ComputeContext> &compute_ctx);

    /**
     * Samples the @p brush over specified @p chunk, and performs any operations
     * needed to get chunk ready to be meshed by the @ref dc::Mesher
     *
     * @param chunk     Chunk to operate on.
     * @param brush     Brush to sample.
     * @param operation Type of the operation to perform.
     */
    void sample(const std::shared_ptr<Chunk> &chunk,
                const Brush &brush,
                Operation operation);
};

} // namespace dc
} // namespace vm

#endif /* VM_DC_SAMPLER_H */
