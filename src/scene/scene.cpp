#include "config.h"

#include "scene.h"

#include "utils/compute-interop.h"
#include "utils/log.h"
#include "utils/persistence.h"

#include <glm/gtx/norm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>

#include <cstring>

using namespace std;
using namespace glm;
namespace vm {

#define CHUNK_WORLD_SIZE (VM_CHUNK_SIZE * VM_VOXEL_SIZE)

namespace {
size_t coord_hash(const ivec3 &coord) {
    enum { HASH_COEFF = 1024 };
    return coord.x + HASH_COEFF * (coord.y + HASH_COEFF * coord.z);
}

size_t chunk_hash(const shared_ptr<Chunk> &chunk) {
    return coord_hash(chunk->get_coord());
}
} // namespace

void Scene::init_kernels() {
    auto program =
            compute::program::create_with_source_file("kernels/samplers.cl",
                                                      m_compute_ctx->context);
    program.build();
    m_sdf_samplers[Brush::Id::Cube] = program.create_kernel("sample_cube");
    m_sdf_samplers[Brush::Id::Ball] = program.create_kernel("sample_ball");
    m_initializer = program.create_kernel("initialize");
    m_downsampler = program.create_kernel("downsample");
}

void Scene::init_persisted_chunks() {
    for (const ivec3 &coord : m_archive.get_chunk_coords()) {
        auto chunk = m_chunks.emplace(coord_hash(coord),
                                      make_shared<Chunk>(coord,
                                                         m_compute_ctx->context,
                                                         m_volume_format))
                             .first->second;
        m_archive.restore(chunk);
    }
}

void Scene::get_covered_region(const AABB &aabb,
                               ivec3 &out_min,
                               ivec3 &out_max) {
    const vec3 min = aabb.min + float(0.5 * CHUNK_WORLD_SIZE);
    const vec3 max = aabb.max - float(0.5 * CHUNK_WORLD_SIZE);
    out_min = ivec3(INT_MAX, INT_MAX, INT_MAX);
    out_max = ivec3(INT_MIN, INT_MIN, INT_MIN);

    for (size_t i = 0; i < 3; ++i) {
        out_min[i] = std::min(out_min[i], (int) floor(min[i] / CHUNK_WORLD_SIZE));
        out_max[i] = std::max(out_max[i], (int) ceil(max[i] / CHUNK_WORLD_SIZE));
    }
}

Scene::Scene(const shared_ptr<ComputeContext> &compute_ctx,
             const shared_ptr<Camera> &camera,
             const string &scene_directory)
        : m_compute_ctx(compute_ctx),
          m_camera(camera),
          m_chunks(),
          m_volume_format(compute::image_format::rg,
                          compute::image_format::float16),
          m_archive(scene_directory,
                    compute_ctx,
                    m_volume_format) {
    init_kernels();
    init_persisted_chunks();
}

vec3 Scene::get_chunk_origin(const ivec3 &coord) {
    return vec3(CHUNK_WORLD_SIZE * dvec3(coord));
}

void Scene::init_chunk(const shared_ptr<Chunk> &chunk) {
    const size_t N = VM_CHUNK_SIZE + VM_CHUNK_BORDER;
    // Initialize the volume using isovalue = 1e5 and material index -1,
    // which is an air representation
    m_initializer.set_arg(0, chunk->get_volume());
    m_initializer.set_arg(1, vec4(1e5, -1, 0, 0));
    lock_guard<mutex> queue_lock(m_compute_ctx->queue_mutex);
    m_compute_ctx->queue.enqueue_nd_range_kernel(
            m_initializer, 3, nullptr, compute::dim(N, N, N).data(), nullptr);
}

void Scene::sample(const Brush &brush, Operation op) {
    ivec3 region_min;
    ivec3 region_max;
    const size_t N = VM_CHUNK_SIZE + VM_CHUNK_BORDER;
    get_covered_region(brush.get_aabb(), region_min, region_max);

    compute::kernel &sampler = m_sdf_samplers.at(brush.id());
    sampler.set_arg(2, static_cast<int>(op));
    sampler.set_arg(3, brush.material());
    sampler.set_arg(5, brush.get_origin());
    sampler.set_arg(6, 0.5f * brush.get_scale());
    sampler.set_arg(7, brush.get_rotation());

    for (int z = region_min.z; z <= region_max.z; ++z) {
    for (int y = region_min.y; y <= region_max.y; ++y) {
    for (int x = region_min.x; x <= region_max.x; ++x) {
        const ivec3 coord{ x, y, z };
        if (m_chunks.find(coord_hash(coord)) == m_chunks.end()) {
            auto chunk = make_shared<Chunk>(coord, m_compute_ctx->context,
                                            m_volume_format);
            m_chunks.emplace(chunk_hash(chunk), chunk);
            init_chunk(chunk);
            LOG(trace) << "Created chunk (" << x << ',' << y << ',' << z
                       << "), number of chunks: " << m_chunks.size();
        }
        auto chunk = m_chunks[coord_hash(coord)];
        lock_guard<mutex> lock(chunk->get_mutex());

        // Fuck standards. OpenCL < 2.0 standard does not allow images to be
        // read-write, but it works anyway on every device I tested against,
        // so... why bother?
        sampler.set_arg(0, chunk->get_volume());
        sampler.set_arg(1, chunk->get_volume());
        sampler.set_arg(4, get_chunk_origin(coord));
        lock_guard<mutex> queue_lock(m_compute_ctx->queue_mutex);
        m_compute_ctx->queue.enqueue_nd_range_kernel(
                sampler, 3, nullptr, compute::dim(N, N, N).data(), nullptr);
        m_compute_ctx->queue.flush();
        // Queue this modified chunk to be persisted on the next occassion
        m_archive.persist_later(chunk);
    }}}
    lock_guard<mutex> queue_lock(m_compute_ctx->queue_mutex);
    m_compute_ctx->queue.finish();
}

void Scene::add(const Brush &brush) {
    sample(brush, Operation::Add);
}

void Scene::sub(const Brush &brush) {
    sample(brush, Operation::Sub);
}

vector<const Chunk *> Scene::get_chunks_to_render() const {
    vector<const Chunk *> chunks(m_chunks.size());
    size_t index = 0;
    for (const auto &chunk : m_chunks) {
        chunks[index++] = chunk.second.get();
    }

    vec3 camera_origin = m_camera->get_origin();
    auto chunk_comparator = [camera_origin](const Chunk *&lhs,
                                            const Chunk *&rhs) {
        return distance2(camera_origin, get_chunk_origin(lhs->get_coord()))
               < distance2(camera_origin, get_chunk_origin(rhs->get_coord()));
    };

    sort(chunks.begin(), chunks.end(), chunk_comparator);
    /* TODO: frustum culling */
    return chunks;
}

} // namespace vm
