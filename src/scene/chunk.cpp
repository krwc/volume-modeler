#include <config.h>

#include "chunk.h"
#include "scene.h"

using namespace glm;
namespace vm {

static const size_t N = VM_CHUNK_SIZE;

Chunk::Chunk(const ivec3 &coord,
             const compute::context &context,
             int lod)
        : samples(context, N+3, N+3, N+3, Scene::samples_format())
        , edges_x(context, N+3, N+3, N+3, Scene::edges_format())
        , edges_y(context, N+3, N+3, N+3, Scene::edges_format())
        , edges_z(context, N+3, N+3, N+3, Scene::edges_format())
        , vertices(context, N+2, N+2, N+2, Scene::vertices_format())
        , lock()
        , coord(coord)
        , lod(lod) {
}

} // namespace vm
