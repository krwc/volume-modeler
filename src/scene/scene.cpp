#include "config.h"
#include "scene.h"

#include "utils/persistence.h"

#include <glm/gtx/norm.hpp>

#include <algorithm>

#include <cstring>

namespace vm {
using namespace std;
using namespace glm;

#define CHUNK_WORLD_SIZE (VM_CHUNK_SIZE * VM_VOXEL_SIZE)

Scene::Scene()
    : m_camera()
    , m_chunks()
    , m_sampler() {
    m_sampler.define("VM_VOXEL_SIZE", to_string(VM_VOXEL_SIZE));
    m_sampler.define("VM_CHUNK_SIZE", to_string(VM_CHUNK_SIZE));
    m_sampler.define("VM_CHUNK_BORDER", to_string(VM_CHUNK_BORDER));
    m_sampler.set_shader_from_file(GL_COMPUTE_SHADER, "shaders/sampler.glsl");
    m_sampler.compile();
    m_sampler.link();
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

void Scene::sample(const Brush &brush, Operation op) {
    ivec3 region_min, region_max;
    get_affected_region(region_min, region_max, brush.get_aabb());

    auto bb = brush.get_aabb();
    fprintf(stderr, "AABBmin (%.3f, %.3f, %.3f), AABBmax (%.3f, %.3f, %.3f)\n",
            bb.min.x, bb.min.y, bb.min.z, bb.max.x, bb.max.y, bb.max.z);
    fprintf(stderr, "Affected region (%d,%d,%d)x(%d,%d,%d)\n",
            region_min.x, region_min.y, region_min.z,
            region_max.x, region_max.y, region_max.z);

    glUseProgram(m_sampler.id());
    m_sampler.set_constant("brush", brush.id());
    m_sampler.set_constant("brush_origin", brush.get_origin());
    m_sampler.set_constant("brush_scale", 0.5f * brush.get_scale());
    m_sampler.set_constant("brush_rotation", brush.get_rotation());
    m_sampler.set_constant("operation", (int) op);

    for (int z = region_min.z; z <= region_max.z; ++z) {
    for (int y = region_min.y; y <= region_max.y; ++y) {
    for (int x = region_min.x; x <= region_max.x; ++x) {
        size_t current = get_coord_hash(x, y, z);
        auto &&node = m_chunks[current];

        if (!node.volume) {
            node.coord = ivec3(x, y, z);
            node.origin = get_chunk_origin(x, y, z);
            node.volume = make_unique<Texture3d>(
                    TextureDesc3d{ VM_CHUNK_SIZE + VM_CHUNK_BORDER,
                                   VM_CHUNK_SIZE + VM_CHUNK_BORDER,
                                   VM_CHUNK_SIZE + VM_CHUNK_BORDER, GL_R16F });
            node.volume->set_parameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            node.volume->set_parameter(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            node.volume->set_parameter(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
            node.volume->set_parameter(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
            node.volume->set_parameter(GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);
            node.volume->clear(1e5, 1e5, 1e5, 1e5);
        }
        m_sampler.set_constant("chunk_origin", get_chunk_origin(x, y, z));
        glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT);
        glBindImageTexture(0, node.volume->id(), 0, GL_TRUE, 0, GL_READ_WRITE,
                           GL_R16F);
        glDispatchCompute(VM_CHUNK_SIZE + VM_CHUNK_BORDER,
                          VM_CHUNK_SIZE + VM_CHUNK_BORDER,
                          VM_CHUNK_SIZE + VM_CHUNK_BORDER);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    }}}
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

    chunk.volume->read(buffer.data(), sizeof(uint16_t) * buffer.size(), GL_RED,
                       GL_SHORT);

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

    chunk.volume = make_unique<Texture3d>(TextureDesc3d {
        N, N, N, GL_R16F
    });
    chunk.volume->set_parameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    chunk.volume->set_parameter(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    chunk.volume->set_parameter(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    chunk.volume->set_parameter(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    chunk.volume->set_parameter(GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    chunk.volume->fill(buffer.data(), GL_RED, GL_SHORT);

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
