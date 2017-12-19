#ifndef VM_DC_MESHER_H
#define VM_DC_MESHER_H
#include <memory>

#include "compute/context.h"
#include "compute/scan.h"

namespace vm {
class Chunk;

namespace dc {

class Mesher {
private:
    std::shared_ptr<ComputeContext> m_compute_ctx;
    compute::kernel m_select_active_edges;
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
    compute::command_queue m_mesher_queue;

    void init_buffers();
    void init_kernels();
    void realloc_vbo_if_necessary(Chunk &chunk, size_t num_voxels);
    void realloc_ibo_if_necessary(Chunk &chunk, size_t num_edges);

    compute::event enqueue_select_edges(
            Chunk &chunk,
            compute::command_queue &queue,
            const compute::wait_list &events = compute::wait_list());

    compute::event
    enqueue_solve_qef(Chunk &chunk,
                      compute::command_queue &queue,
                      const compute::wait_list &events = compute::wait_list());

    void
    enqueue_contour(Chunk &chunk,
                    compute::command_queue &queue,
                    const compute::wait_list &events = compute::wait_list());

public:
    Mesher(const std::shared_ptr<ComputeContext> &compute_ctx);
    Mesher(const Mesher &) = delete;
    Mesher &operator=(const Mesher &) = delete;

    /**
     * Enqueues contouring on the specified @p queue and waits till it finishes.
     * @param chunk Chunk to contour.
     * @param queue Queue to use.
     * @param events Events to wait for before contouring.
     */
    void contour(Chunk &chunk,
                 compute::command_queue &queue,
                 const compute::wait_list &events = compute::wait_list());
};

} // namespace dc
} // namespace vm

#endif /* VM_DC_MESHER_H */
