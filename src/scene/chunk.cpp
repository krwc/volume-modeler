#include <config.h>

#include "chunk.h"
#include "scene.h"

using namespace glm;
namespace vm {

Chunk::Chunk(const ivec3 &coord, const compute::context &context, int lod)
        : samples(context,
                  SAMPLE_GRID_DIM,
                  SAMPLE_GRID_DIM,
                  SAMPLE_GRID_DIM,
                  Scene::samples_format())
        , edges_x(context,
                  EDGE_X_GRID_DIM(AXIS_X),
                  EDGE_X_GRID_DIM(AXIS_Y),
                  EDGE_X_GRID_DIM(AXIS_Z),
                  Scene::edges_format())
        , edges_y(context,
                  EDGE_Y_GRID_DIM(AXIS_X),
                  EDGE_Y_GRID_DIM(AXIS_Y),
                  EDGE_Y_GRID_DIM(AXIS_Z),
                  Scene::edges_format())
        , edges_z(context,
                  EDGE_Z_GRID_DIM(AXIS_X),
                  EDGE_Z_GRID_DIM(AXIS_Y),
                  EDGE_Z_GRID_DIM(AXIS_Z),
                  Scene::edges_format())
        , vbo()
        , num_vertices(0)
        , ibo()
        , num_indices(0)
        , mutex()
        , coord(coord)
        , lod(lod) {
}

} // namespace vm
