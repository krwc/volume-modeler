#include <config.h>

#include <chrono>
#include <sstream>

#include "sampler.h"

#include "scene/brush.h"
#include "scene/chunk.h"
#include "scene/scene.h"

#include "compute/interop.h"
#include "compute/utils.h"

#include "utils/log.h"

using namespace std;
using namespace glm;
namespace vm {
namespace dc {
namespace {
vector<pair<string, Brush::Id>> supported_brushes() {
    return { { "BRUSH_BALL", Brush::Id::Ball },
             { "BRUSH_CUBE", Brush::Id::Cube } };
}
} // namespace

Sampler::Sampler(const shared_ptr<ComputeContext> &compute_ctx) {
    for (const auto &supported_brush : supported_brushes()) {
        auto program = compute::program::create_with_source_file(
                "media/kernels/samplers.cl", compute_ctx->context);
        program.build("-D" + supported_brush.first);

        m_sdf_samplers.at(static_cast<size_t>(supported_brush.second)).sampler =
                program.create_kernel("sample");
        m_sdf_samplers.at(static_cast<size_t>(supported_brush.second)).updater =
                program.create_kernel("update_edges");
    }
}

compute::wait_list Sampler::sample(Chunk &chunk,
                                   const Brush &brush,
                                   Operation operation,
                                   compute::command_queue &queue) {
    compute::wait_list events{};
    events.reserve(4);
    compute::kernel &sampler =
            m_sdf_samplers.at(static_cast<size_t>(brush.id())).sampler;
    const vec3 chunk_origin = Scene::get_chunk_origin(chunk.coord);
    const vec3 brush_scale = 0.5f * brush.get_scale();
    const vec3 brush_origin = brush.get_origin();
    const mat3 brush_rotation = brush.get_rotation();

    sampler.set_arg(0, chunk.samples);
    sampler.set_arg(1, chunk.samples);
    sampler.set_arg(2, static_cast<cl_int>(operation));
    sampler.set_arg(3, chunk_origin);
    sampler.set_arg(4, brush_origin);
    sampler.set_arg(5, brush_scale);
    sampler.set_arg(6, brush_rotation);

    compute::kernel &updater =
            m_sdf_samplers.at(static_cast<size_t>(brush.id())).updater;

    events.insert(enqueue_auto_distributed_nd_range_kernel<3>(
            queue,
            sampler,
            compute::dim(SAMPLE_GRID_DIM, SAMPLE_GRID_DIM, SAMPLE_GRID_DIM)));

    updater.set_arg(2, chunk_origin);
    updater.set_arg(3, brush_origin);
    updater.set_arg(4, brush_scale);
    updater.set_arg(5, brush_rotation);

    // TODO: Either this should be run in a single kernel or at unordered queue
    for (int axis = 0; axis < 3; ++axis) {
        updater.set_arg(0, (&chunk.edges_x)[axis]);
        updater.set_arg(1, static_cast<cl_int>(axis));
        events.insert(enqueue_auto_distributed_nd_range_kernel<3>(
                queue,
                updater,
                compute::dim(
                        SAMPLE_GRID_DIM, SAMPLE_GRID_DIM, SAMPLE_GRID_DIM)));
    }
    return events;
}

} // namespace dc
} // namespace vm
