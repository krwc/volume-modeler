#ifndef VM_SCENE_CHUNK_H
#define VM_SCENE_CHUNK_H
#include <glm/glm.hpp>
#include <mutex>

#include <compute/context.h>

#include <gfx/buffer.h>

namespace vm {

struct Chunk {
    compute::image3d samples;
    compute::image3d edges_x;
    compute::image3d edges_y;
    compute::image3d edges_z;

    Buffer vbo;
    size_t num_vertices;

    Buffer ibo;
    size_t num_indices;

    std::mutex mutex;
    glm::ivec3 coord;
    int lod;

    /**
     * Constructs an empty chunk of the scene, with no volumetric data
     * associated - it may be loaded later on.
     */
    Chunk(const glm::ivec3 &coord,
          const compute::context &context,
          int lod = 0);
};

} // namespace vm

#endif /* VM_SCENE_CHUNK_H */
