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
        , m_mesher_queue(compute_ctx->make_queue()) {
    init_buffers();
    init_kernels();
}

void Mesher::enqueue_select_edges(Chunk &chunk) {
    // Mark active edges
    m_select_active_edges.set_arg(0, m_edge_mask);
    m_select_active_edges.set_arg(1, chunk.samples);

    auto event = enqueue_auto_distributed_nd_range_kernel<3>(
            m_mesher_queue,
            m_select_active_edges,
            compute::dim(VOXEL_GRID_DIM, VOXEL_GRID_DIM, VOXEL_GRID_DIM));

    // Count them
    m_edges_scan.inclusive_scan(
            m_edge_mask, m_scanned_edges, m_mesher_queue, event);
}

void Mesher::enqueue_solve_qef(Chunk &chunk) {
    m_solve_qef.set_arg(0, chunk.samples);
    m_solve_qef.set_arg(1, chunk.edges_x);
    m_solve_qef.set_arg(2, chunk.edges_y);
    m_solve_qef.set_arg(3, chunk.edges_z);
    m_solve_qef.set_arg(4, Scene::get_chunk_origin(chunk.coord));
    m_solve_qef.set_arg(5, m_voxel_vertices);
    m_solve_qef.set_arg(6, m_voxel_mask);

    auto event = enqueue_auto_distributed_nd_range_kernel<3>(
            m_mesher_queue,
            m_solve_qef,
            compute::dim(VOXEL_GRID_DIM, VOXEL_GRID_DIM, VOXEL_GRID_DIM));

    // Count active voxels
    m_voxels_scan.inclusive_scan(
            m_voxel_mask, m_scanned_voxels, m_mesher_queue, event);
}

void Mesher::realloc_vbo_if_necessary(Chunk &chunk, size_t num_voxels) {
    static const size_t vertex_size = sizeof(glm::vec3);

    if (chunk.vbo.size() < vertex_size * num_voxels) {
        chunk.vbo = move(Buffer(BufferDesc{ GL_ARRAY_BUFFER,
                                            GL_DYNAMIC_DRAW,
                                            nullptr,
                                            vertex_size * num_voxels }));
#if defined(WITH_CONTEXT_SHARING)
        chunk.cl_vbo =
                compute::opengl_buffer(m_compute_ctx->context, chunk.vbo.id());
#else
        chunk.cl_vbo = compute::buffer(m_compute_ctx->context,
                                       vertex_size * num_voxels);
#endif // WITH_CONTEXT_SHARING
    }
}

void Mesher::realloc_ibo_if_necessary(Chunk &chunk, size_t num_edges) {
    const size_t num_indices = 6 * num_edges;

    if (chunk.ibo.size() < sizeof(unsigned) * num_indices) {
        chunk.ibo = move(
                Buffer(BufferDesc{ GL_ELEMENT_ARRAY_BUFFER,
                                   GL_DYNAMIC_DRAW,
                                   nullptr,
                                   sizeof(unsigned) * num_indices }));
#if defined(WITH_CONTEXT_SHARING)
        chunk.cl_ibo =
                compute::opengl_buffer(m_compute_ctx->context, chunk.ibo.id());
#else
        chunk.cl_ibo = compute::buffer(m_compute_ctx->context,
                                       sizeof(unsigned) * num_indices);
#endif // WITH_CONTEXT_SHARING
    }
}

void Mesher::enqueue_contour(Chunk &chunk) {
    uint32_t num_voxels = m_scanned_voxels.back();
    uint32_t num_edges = m_scanned_edges.back();
    chunk.num_indices = 6 * num_edges;
    if (!num_voxels || !num_edges) {
        // Nothing to do.
        return;
    }
    realloc_vbo_if_necessary(chunk, num_voxels);
    realloc_ibo_if_necessary(chunk, num_edges);

#if defined(WITH_CONTEXT_SHARING)
    // Ensure we don't have any race with acquire commands.
    glFinish();

    const cl_mem buffers[] = {
        chunk.cl_vbo.get(),
        chunk.cl_ibo.get()
    };
    opengl_enqueue_acquire_gl_objects(2, buffers, m_compute_ctx->queue);
#endif // WITH_CONTEXT_SHARING

    m_copy_vertices.set_arg(0, chunk.cl_vbo);
    m_copy_vertices.set_arg(1, m_voxel_vertices);
    m_copy_vertices.set_arg(2, m_voxel_mask);
    m_copy_vertices.set_arg(3, m_scanned_voxels);
    enqueue_auto_distributed_nd_range_kernel<1>(m_compute_ctx->queue,
                                                m_copy_vertices,
                                                compute::dim(
                                                        VOXEL_3D_GRID_SIZE));


    m_make_indices.set_arg(0, chunk.cl_ibo);
    m_make_indices.set_arg(1, m_edge_mask);
    m_make_indices.set_arg(2, m_scanned_edges);
    m_make_indices.set_arg(3, m_scanned_voxels);
    m_make_indices.set_arg(4, chunk.samples);
    enqueue_auto_distributed_nd_range_kernel<1>(
            m_compute_ctx->queue,
            m_make_indices,
            compute::dim(3 * SAMPLE_3D_GRID_SIZE)).wait();

#if defined(WITH_CONTEXT_SHARING)
    opengl_enqueue_release_gl_objects(2, buffers, m_compute_ctx->queue);
#else
    m_compute_ctx->queue.finish();
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
    MappedBuffer mapped_ibo(chunk.ibo.id(), GL_WRITE_ONLY);
    m_compute_ctx->queue.enqueue_read_buffer(chunk.cl_vbo,
                                             0,
                                             chunk.cl_vbo.size(),
                                             mapped_vbo.raw_ptr);
    m_compute_ctx->queue.enqueue_read_buffer(chunk.cl_ibo,
                                             0,
                                             chunk.cl_ibo.size(),
                                             mapped_ibo.raw_ptr);
    m_compute_ctx->queue.finish();
#endif // WITH_CONTEXT_SHARING
    chunk.num_vertices = num_voxels;
}

void Mesher::contour(Chunk &chunk) {
    enqueue_select_edges(chunk);
    enqueue_solve_qef(chunk);
    m_mesher_queue.finish();
    enqueue_contour(chunk);
}

} // namespace dc
} // namespace vm
