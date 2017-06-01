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
        , edges_x(context, N+2, N+3, N+3, Scene::edges_format())
        , edges_y(context, N+3, N+2, N+3, Scene::edges_format())
        , edges_z(context, N+3, N+3, N+2, Scene::edges_format())
#warning "TODO: this vbo and cl_vbo are rather ugly"
        , vbo()
        , num_vertices(0)
        , cl_vbo()
        , ibo()
        , cl_ibo()
        , lock()
        , coord(coord)
        , lod(lod) {
}

} // namespace vm
