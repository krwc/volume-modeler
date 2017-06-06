#include "config.h"

#include "scene.h"

#include "compute/interop.h"

#include "utils/log.h"
#include "utils/persistence.h"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/norm.hpp>

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
    return coord_hash(chunk->coord);
}
} // namespace

void Scene::init_persisted_chunks() {
    for (const ivec3 &coord : m_archive.get_chunk_coords()) {
        auto chunk = make_shared<Chunk>(coord, m_compute_ctx->context);
        m_chunks.emplace(chunk_hash(chunk), chunk);
        m_archive.restore(chunk);
        m_mesher.contour(*chunk);
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
        out_min[i] =
                std::min(out_min[i], (int) floor(min[i] / CHUNK_WORLD_SIZE));
        out_max[i] =
                std::max(out_max[i], (int) ceil(max[i] / CHUNK_WORLD_SIZE));
    }
}

compute::image_format Scene::samples_format() {
    return compute::image_format(compute::image_format::r,
                                 compute::image_format::float16);
}

compute::image_format Scene::edges_format() {
    return compute::image_format(compute::image_format::rgba,
                                 compute::image_format::float16);
}

Scene::Scene(const shared_ptr<ComputeContext> &compute_ctx,
             const shared_ptr<Camera> &camera,
             const string &scene_directory)
        : m_compute_ctx(compute_ctx)
        , m_camera(camera)
        , m_chunks()
        , m_archive(scene_directory, compute_ctx)
        , m_sampler(compute_ctx)
        , m_mesher(compute_ctx) {
    init_persisted_chunks();
}

vec3 Scene::get_chunk_origin(const ivec3 &coord) {
    return vec3(CHUNK_WORLD_SIZE * dvec3(coord));
}

void Scene::init_chunk(const shared_ptr<Chunk> &chunk) {
    lock_guard<mutex> queue_lock(m_compute_ctx->queue_mutex);
    const compute::float4_ fill_color(1e3, 1e3, 1e3, 1e3);
    m_compute_ctx->queue.enqueue_fill_image<3>(chunk->samples,
                                               &fill_color,
                                               compute::dim(0, 0, 0),
                                               chunk->samples.size());
}

void Scene::sample(const Brush &brush, dc::Sampler::Operation operation) {
    ivec3 region_min;
    ivec3 region_max;
    get_covered_region(brush.get_aabb(), region_min, region_max);

    for (int z = region_min.z; z <= region_max.z; ++z) {
        for (int y = region_min.y; y <= region_max.y; ++y) {
            for (int x = region_min.x; x <= region_max.x; ++x) {
                const ivec3 coord{ x, y, z };
                if (m_chunks.find(coord_hash(coord)) == m_chunks.end()) {
                    auto chunk =
                            make_shared<Chunk>(coord, m_compute_ctx->context);
                    m_chunks.emplace(chunk_hash(chunk), chunk);
                    init_chunk(chunk);
                    LOG(trace) << "Created chunk (" << x << ',' << y << ',' << z
                               << "), number of chunks: " << m_chunks.size();
                }
                auto chunk = m_chunks[coord_hash(coord)];
                {
                    lock_guard<mutex> chunk_lock(chunk->mutex);
                    lock_guard<mutex> queue_lock(m_compute_ctx->queue_mutex);
                    m_sampler.sample(*chunk, brush, operation);
                    m_mesher.contour(*chunk);
                }

                // Queue this modified chunk to be persisted on the next
                // occassion
                m_archive.persist_later(chunk);
            }
        }
    }
    lock_guard<mutex> queue_lock(m_compute_ctx->queue_mutex);
    m_compute_ctx->queue.finish();
}

void Scene::add(const Brush &brush) {
    sample(brush, dc::Sampler::Operation::Add);
}

void Scene::sub(const Brush &brush) {
    sample(brush, dc::Sampler::Operation::Sub);
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
        return distance2(camera_origin, get_chunk_origin(lhs->coord))
               < distance2(camera_origin, get_chunk_origin(rhs->coord));
    };

    sort(chunks.begin(), chunks.end(), chunk_comparator);
    /* TODO: frustum culling */
    return chunks;
}

} // namespace vm
