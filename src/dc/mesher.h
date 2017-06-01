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
    compute::kernel m_clear;
    compute::kernel m_solve_qef;
    Scan m_edges_scan;
    Scan m_voxels_scan;
    /* Binary vector for each edge that tells whether an edge is active */
    compute::vector<uint32_t> m_edge_mask;
    /* Binary vector for each voxel that tells whether a voxel is active */
    compute::vector<uint32_t> m_voxel_mask;
    /* Prefixsums of active edges / voxels */
    compute::vector<uint32_t> m_scanned_edges;
    compute::vector<uint32_t> m_scanned_voxels;
    /* Vertices solved by the QEF */
    compute::vector<float> m_voxel_vertices;

    /* Finally, some geometry generator */
    compute::kernel m_copy_vertices;
    compute::kernel m_make_indices;

    /* A queue where active-edges and qef will be computed */
    compute::command_queue m_unordered_queue;

    void enqueue_get_active_edges(Chunk &chunk);
    void enqueue_solve_qef(Chunk &chunk);
    void enqueue_contour(Chunk &chunk);

public:
    Mesher(const std::shared_ptr<ComputeContext> &compute_ctx);

    void contour(Chunk &chunk);
};

} // namespace dc
} // namespace vm

#endif /* VM_DC_MESHER_H */
