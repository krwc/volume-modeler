#ifndef VM_DC_MESHER_H
#define VM_DC_MESHER_H
#include <memory>

#include "compute/context.h"
#include "compute/scan.h"

namespace vm {
class Chunk;

namespace dc {

class Mesher {
    std::shared_ptr<ComputeContext> m_compute_ctx;
    compute::kernel m_select_active_edges;
    compute::kernel m_copy_selected;
    compute::kernel m_solve_qef;
    /* Instance per one task that can be run in parallel */
    Scan m_scan[3];
    /* Indices of active edges for each axis */
    compute::vector<uint32_t> m_active_edges[3];
    /* Vertices solved by the QEF */
    compute::vector<cl_float3> m_voxel_vertices;

    /* Finally, some geometry generator */
    compute::kernel m_contour;

    /* A queue where active-edges and qef will be computed */
    compute::command_queue m_unordered_queue;

    void enqueue_get_active_edges(size_t axis);
    void enqueue_solve_qef();
    void enqueue_contour();

public:
    Mesher(const std::shared_ptr<ComputeContext> &compute_ctx);

    void contour(Chunk &chunk);
};

} // namespace dc
} // namespace vm

#endif /* VM_DC_MESHER_H */
