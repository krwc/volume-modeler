#include <config.h>

#include "mesher.h"

#include "compute/interop.h"
#include "compute/utils.h"

#include "scene/chunk.h"
#include "scene/scene.h"

#include "utils/log.h"

using namespace std;
namespace vm {
namespace dc {

void Mesher::init_buffers() {
    // Actually this is a bit too much than it needs to be, because the
    // regular grid of N voxels has 3 * N*(N+1)*(N+1) edges.
    //
    // Allocating a bigger buffer makes compute kernels easier to write
    // though.
    const size_t num_edges = 3 * SAMPLE_3D_GRID_SIZE;
    m_edge_mask = compute::vector<uint32_t>(num_edges, m_compute_ctx->context);
    m_scanned_edges =
            compute::vector<uint32_t>(num_edges, m_compute_ctx->context);
    compute::fill(m_scanned_edges.begin(),
                  m_scanned_edges.end(),
                  0,
                  m_compute_ctx->queue);
    compute::fill(
            m_edge_mask.begin(), m_edge_mask.end(), 0, m_compute_ctx->queue);

    m_voxel_mask = compute::vector<uint32_t>(VOXEL_3D_GRID_SIZE,
                                             m_compute_ctx->context);
    m_scanned_voxels = compute::vector<uint32_t>(VOXEL_3D_GRID_SIZE,
                                                 m_compute_ctx->context);

    static const float num_vertex_components = 3;
    m_voxel_vertices =
            compute::vector<float>(num_vertex_components * VOXEL_3D_GRID_SIZE,
                                   m_compute_ctx->context);
}

void Mesher::init_kernels() {
    {
        auto program = compute::program::create_with_source_file(
                "media/kernels/selectors.cl", m_compute_ctx->context);
        program.build();

        m_select_active_edges = program.create_kernel("select_active_edges");
    }

    {
        auto program = compute::program::create_with_source_file(
                "media/kernels/qef.cl", m_compute_ctx->context);
        program.build();
        m_solve_qef = program.create_kernel("solve_qef");
    }

    {
        auto program = compute::program::create_with_source_file(
                "media/kernels/contour.cl", m_compute_ctx->context);
        program.build();
        m_copy_vertices = program.create_kernel("copy_vertices");
        m_make_indices = program.create_kernel("make_indices");
    }
}

Mesher::Mesher(const shared_ptr<ComputeContext> &compute_ctx)
        : m_compute_ctx(compute_ctx)
        , m_edges_scan(compute_ctx->context)
        , m_voxels_scan(compute_ctx->context)
        , m_vbo(compute_ctx->context, 1)
        , m_ibo(compute_ctx->context, 1) {
    init_buffers();
    init_kernels();
}

compute::event Mesher::enqueue_select_edges(Chunk &chunk,
                                            compute::command_queue &queue,
                                            const compute::wait_list &events) {
    // Mark active edges
    m_select_active_edges.set_arg(0, m_edge_mask);
    m_select_active_edges.set_arg(1, chunk.samples);

    auto event = enqueue_auto_distributed_nd_range_kernel<3>(
            queue,
            m_select_active_edges,
            compute::dim(VOXEL_GRID_DIM, VOXEL_GRID_DIM, VOXEL_GRID_DIM),
            events);

    // Count them
    return m_edges_scan.inclusive_scan(
            m_edge_mask, m_scanned_edges, queue, event);
}

compute::event Mesher::enqueue_solve_qef(Chunk &chunk,
                                         compute::command_queue &queue,
                                         const compute::wait_list &events) {
    m_solve_qef.set_arg(0, chunk.samples);
    m_solve_qef.set_arg(1, chunk.edges_x);
    m_solve_qef.set_arg(2, chunk.edges_y);
    m_solve_qef.set_arg(3, chunk.edges_z);
    m_solve_qef.set_arg(4, Scene::get_chunk_origin(chunk.coord));
    m_solve_qef.set_arg(5, m_voxel_vertices);
    m_solve_qef.set_arg(6, m_voxel_mask);

    auto event = enqueue_auto_distributed_nd_range_kernel<3>(
            queue,
            m_solve_qef,
            compute::dim(VOXEL_GRID_DIM, VOXEL_GRID_DIM, VOXEL_GRID_DIM),
            events);

    // Count active voxels
    return m_voxels_scan.inclusive_scan(
            m_voxel_mask, m_scanned_voxels, queue, event);
}

namespace {
constexpr inline size_t required_vbo_size(size_t num_active_voxels) {
    return sizeof(glm::vec3) * num_active_voxels;
}

constexpr inline size_t required_ibo_size(size_t num_active_edges) {
    return 6u * sizeof(unsigned) * num_active_edges;
}
} // namespace

void Mesher::realloc_vbo_if_necessary(Chunk &chunk, size_t num_voxels) {
    const size_t new_size = required_vbo_size(num_voxels);

    if (chunk.vbo.size() < new_size) {
        chunk.vbo = move(Buffer(BufferDesc{
                GL_ARRAY_BUFFER, GL_DYNAMIC_DRAW, nullptr, new_size }));
    }
    if (m_vbo.size() < new_size) {
        m_vbo = move(compute::buffer(m_compute_ctx->context, new_size));
   }
}

void Mesher::realloc_ibo_if_necessary(Chunk &chunk, size_t num_edges) {
    const size_t new_size = required_ibo_size(num_edges);

    if (chunk.ibo.size() < new_size) {
        chunk.ibo = move(Buffer(BufferDesc{
                GL_ELEMENT_ARRAY_BUFFER, GL_DYNAMIC_DRAW, nullptr, new_size }));
    }
    if (m_ibo.size() < new_size) {
        m_ibo = move(compute::buffer(m_compute_ctx->context, new_size));
    }
}

void Mesher::enqueue_contour(Chunk &chunk,
                             compute::command_queue &queue,
                             const compute::wait_list &events) {
    // Unfortunately we have to wait for these, because otherwise we won't
    // be able to access m_scanned_voxels and m_scanned_edges safely.
    events.wait();
    uint32_t num_voxels = m_scanned_voxels.back();
    uint32_t num_edges = m_scanned_edges.back();
    chunk.num_indices = 6 * num_edges;
    if (!num_voxels || !num_edges) {
        // Nothing to do.
        return;
    }
    realloc_vbo_if_necessary(chunk, num_voxels);
    realloc_ibo_if_necessary(chunk, num_edges);

    m_copy_vertices.set_arg(0, m_vbo);
    m_copy_vertices.set_arg(1, m_voxel_vertices);
    m_copy_vertices.set_arg(2, m_voxel_mask);
    m_copy_vertices.set_arg(3, m_scanned_voxels);
    auto copy_event = enqueue_auto_distributed_nd_range_kernel<1>(
            queue, m_copy_vertices, compute::dim(VOXEL_3D_GRID_SIZE), events);

    m_make_indices.set_arg(0, m_ibo);
    m_make_indices.set_arg(1, m_edge_mask);
    m_make_indices.set_arg(2, m_scanned_edges);
    m_make_indices.set_arg(3, m_scanned_voxels);
    m_make_indices.set_arg(4, chunk.samples);
    auto indices_event = enqueue_auto_distributed_nd_range_kernel<1>(
            queue,
            m_make_indices,
            compute::dim(3 * SAMPLE_3D_GRID_SIZE),
            events);

    // TODO: Perhaps this belongs to vm::Buffer implementation?
    struct MappedBuffer {
        GLuint id;
        void *raw_ptr;

        MappedBuffer(GLuint id, GLenum access)
                : id(id), raw_ptr(glMapNamedBuffer(id, access)) {
            if (!raw_ptr) {
                throw runtime_error("could not map a buffer");
            }
        }

        ~MappedBuffer() {
            if (raw_ptr) {
                glUnmapNamedBuffer(id);
            }
        }
    };
    MappedBuffer mapped_vbo(chunk.vbo.id(), GL_WRITE_ONLY);
    auto vbo_copy_event =
            queue.enqueue_read_buffer(m_vbo,
                                      0,
                                      required_vbo_size(num_voxels),
                                      mapped_vbo.raw_ptr,
                                      { copy_event, indices_event });

    MappedBuffer mapped_ibo(chunk.ibo.id(), GL_WRITE_ONLY);
    auto ibo_copy_event =
            queue.enqueue_read_buffer(m_ibo,
                                      0,
                                      required_ibo_size(num_edges),
                                      mapped_ibo.raw_ptr,
                                      { copy_event, indices_event });
    chunk.num_vertices = num_voxels;
    // Again, we MUST wait or otherwise buffer will unmap itself, and we'd
    // land in the undefined-behavior world.
    vbo_copy_event.wait();
    ibo_copy_event.wait();
}

void Mesher::contour(Chunk &chunk,
                     compute::command_queue &queue,
                     const compute::wait_list &events) {
    auto select_edges_event = enqueue_select_edges(chunk, queue, events);
    auto solve_qef_event = enqueue_solve_qef(chunk, queue);
    enqueue_contour(chunk, queue, { select_edges_event, solve_qef_event });
}

} // namespace dc
} // namespace vm
