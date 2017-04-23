#include "config.h"
#include "scene.h"

#include "utils/compute-interop.h"
#include "utils/persistence.h"

#include <glm/gtx/norm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>

#include <cstring>

using namespace std;
using namespace glm;
namespace vm {

#define CHUNK_WORLD_SIZE (VM_CHUNK_SIZE * VM_VOXEL_SIZE)

Scene::Scene(shared_ptr<ComputeContext> compute_ctx)
    : m_camera()
    , m_compute_ctx(compute_ctx)
    , m_chunks()
    , m_volume_format(compute::image_format::r, compute::image_format::float16) {
    auto program =
            compute::program::create_with_source_file("kernels/samplers.cl",
                                                      m_compute_ctx->context);
    program.build();
    m_sphere_sampler = program.create_kernel("sample_sphere");
    m_cube_sampler = program.create_kernel("sample_cube");
    m_initializer = program.create_kernel("initialize");
}

vector<const Scene::Chunk *> Scene::get_chunks_to_render() const {
    vector<const Scene::Chunk *> chunks(m_chunks.size());
    size_t index = 0;
    for (const auto &chunk : m_chunks) {
        chunks[index++] = &chunk.second;
    }

    vec3 camera_origin = m_camera->get_origin();
    auto chunk_comparator = [camera_origin](const Scene::Chunk *&lhs,
                                            const Scene::Chunk *&rhs) {
        return distance2(camera_origin, lhs->origin)
               < distance2(camera_origin, rhs->origin);
    };

    sort(chunks.begin(), chunks.end(), chunk_comparator);
    /* TODO: frustum culling */
    return chunks;
}

void Scene::get_affected_region(ivec3 &out_min,
                                ivec3 &out_max,
                                const AABB &aabb) const {
    const vec3 min = aabb.min + float(0.5 * CHUNK_WORLD_SIZE);
    const vec3 max = aabb.max - float(0.5 * CHUNK_WORLD_SIZE);
    out_min = ivec3(INT_MAX, INT_MAX, INT_MAX);
    out_max = ivec3(INT_MIN, INT_MIN, INT_MIN);

    for (size_t i = 0; i < 3; ++i) {
        out_min[i] = std::min(out_min[i], (int) floor(min[i] / CHUNK_WORLD_SIZE));
        out_max[i] = std::max(out_max[i], (int) ceil(max[i] / CHUNK_WORLD_SIZE));
    }
}

size_t Scene::get_coord_hash(int x, int y, int z) {
    enum { HASH_COEFF = 1024 };
    return x + HASH_COEFF * (y + HASH_COEFF * z);
}

Scene::Chunk *Scene::get_chunk_ptr(int x, int y, int z) {
    auto node = m_chunks.find(get_coord_hash(x, y, z));
    if (node != m_chunks.end()) {
        return &node->second;
    }
    return nullptr;
}

vec3 Scene::get_chunk_origin(int x, int y, int z) const {
    return vec3(CHUNK_WORLD_SIZE * dvec3(x, y, z));
}

compute::kernel &Scene::get_sampler(int brush_id) {
    switch (brush_id) {
    case 0: return m_sphere_sampler;
    case 1: return m_cube_sampler;
    default:
        throw invalid_argument("No sampler for given brush_id="
                               + to_string(brush_id));
    }
}


void Scene::sample(const Brush &brush, Operation op) {
    ivec3 region_min, region_max;
    get_affected_region(region_min, region_max, brush.get_aabb());

    auto bb = brush.get_aabb();
    fprintf(stderr, "AABBmin (%.3f, %.3f, %.3f), AABBmax (%.3f, %.3f, %.3f)\n",
            bb.min.x, bb.min.y, bb.min.z, bb.max.x, bb.max.y, bb.max.z);
    fprintf(stderr, "Affected region (%d,%d,%d)x(%d,%d,%d)\n",
            region_min.x, region_min.y, region_min.z,
            region_max.x, region_max.y, region_max.z);

    compute::kernel &sampler = get_sampler(brush.id());
    sampler.set_arg(2, static_cast<int>(op));
    sampler.set_arg(4, brush.get_origin());
    sampler.set_arg(5, 0.5f * brush.get_scale());
    sampler.set_arg(6, brush.get_rotation());

    for (int z = region_min.z; z <= region_max.z; ++z) {
    for (int y = region_min.y; y <= region_max.y; ++y) {
    for (int x = region_min.x; x <= region_max.x; ++x) {
        size_t current = get_coord_hash(x, y, z);
        auto &&node = m_chunks[current];

        if (!node.volume) {
            node.volume = move(compute::image3d(
                    m_compute_ctx->context,
                    VM_CHUNK_SIZE + VM_CHUNK_BORDER,
                    VM_CHUNK_SIZE + VM_CHUNK_BORDER,
                    VM_CHUNK_SIZE + VM_CHUNK_BORDER,
                    m_volume_format));
            node.origin = get_chunk_origin(x, y, z);
            node.coord = ivec3(x, y, z);

            m_initializer.set_arg(0, *node.volume);
            m_initializer.set_arg(1, vec4(1e5, 1e5, 1e5, 1e5));
            m_compute_ctx->queue.enqueue_nd_range_kernel(
                    m_initializer, 3, nullptr,
                    compute::dim(VM_CHUNK_SIZE + VM_CHUNK_BORDER,
                                 VM_CHUNK_SIZE + VM_CHUNK_BORDER,
                                 VM_CHUNK_SIZE + VM_CHUNK_BORDER)
                            .data(),
                    nullptr);
            fprintf(stderr, "Created node (%d, %d, %d), number of nodes: %zu\n",
                    x, y, z, m_chunks.size());
        }
        // Fuck standards. OpenCL < 2.0 standard does not allow images to be read-write,
        // but it works anyway on every device I tested against, so... why bother?
        sampler.set_arg(0, *node.volume);
        sampler.set_arg(1, *node.volume);
        sampler.set_arg(3, node.origin);
        m_compute_ctx->queue.enqueue_nd_range_kernel(
                sampler, 3, nullptr,
                compute::dim(VM_CHUNK_SIZE + VM_CHUNK_BORDER,
                             VM_CHUNK_SIZE + VM_CHUNK_BORDER,
                             VM_CHUNK_SIZE + VM_CHUNK_BORDER)
                        .data(),
                nullptr);
        m_compute_ctx->queue.flush();
    }}}
    m_compute_ctx->queue.finish();
}

void Scene::add(const Brush &brush) {
    sample(brush, Operation::Add);
}

void Scene::sub(const Brush &brush) {
    sample(brush, Operation::Sub);
}

ostream &operator<<(ostream &out, const Scene::Chunk &chunk) {
    const size_t N = VM_CHUNK_SIZE + VM_CHUNK_BORDER;
    vector<int16_t> buffer(N*N*N);
#warning "FIXME: persistence"
    for (size_t i = 0; i < buffer.size(); ++i) {
        out <= buffer[i];
    }

    out <= chunk.coord;
    out <= chunk.origin;

    return out;
}

istream &operator>>(istream &in, Scene::Chunk &chunk) {
    const size_t N = VM_CHUNK_SIZE + VM_CHUNK_BORDER;
    vector<int16_t> buffer(N*N*N);

    for (size_t i = 0; i < buffer.size(); ++i) {
        in >= buffer[i];
    }
#warning "FIXME: persistence"
    in >= chunk.coord;
    in >= chunk.origin;

    return in;
}

istream &operator>>(istream &in, Scene &scene) {
    scene.m_chunks.clear();
    int chunk_size;
    int chunk_border;
    double voxel_size;

    in >= chunk_size
       >= chunk_border
       >= voxel_size;

    if (chunk_size != VM_CHUNK_SIZE
            || chunk_border != VM_CHUNK_BORDER
            || voxel_size != VM_VOXEL_SIZE) {
        throw invalid_argument("Mismatched voxel format");
    }

    size_t num_chunks;
    in >= num_chunks;
    in >> *scene.m_camera.get();

    for (size_t i = 0; i < num_chunks; ++i) {
        size_t chunk_hash;
        in >= chunk_hash;
        in >> scene.m_chunks[chunk_hash];
    }

    return in;
}

ostream &operator<<(ostream &out, const Scene &scene) {
    out <= VM_CHUNK_SIZE
        <= VM_CHUNK_BORDER
        <= VM_VOXEL_SIZE;

    out <= scene.m_chunks.size();
    out << *scene.m_camera.get();

    for (const auto &it : scene.m_chunks) {
        out <= it.first;
        out << it.second;
    }
    return out;
}

} // namespace vm
