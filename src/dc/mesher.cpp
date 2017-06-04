#include <config.h>

#include "mesher.h"

#include "compute/interop.h"

#include "scene/scene.h"
#include "scene/chunk.h"

#include "utils/log.h"

using namespace std;
namespace vm {
namespace dc {

static const size_t N = VM_CHUNK_SIZE;

void Mesher::init_buffers() {
    // Actually this is a bit too much than it needs to be, because the
    // regular grid of (N+2) voxels has 3 * (N+2)*(N+3)*(N+3) edges.
    //
    // Allocating a bigger buffer makes compute kernel easier to write
    // though.
    const size_t num_edges = 3 * ((N + 3) * (N + 3) * (N + 3));
    m_edges_scan = move(Scan(m_compute_ctx->queue, num_edges));
    m_edge_mask =
            compute::vector<uint32_t>(num_edges, m_compute_ctx->context);
    m_scanned_edges =
            compute::vector<uint32_t>(num_edges, m_compute_ctx->context);
    compute::fill(m_scanned_edges.begin(), m_scanned_edges.end(), 0,
                  m_compute_ctx->queue);
    compute::fill(m_edge_mask.begin(), m_edge_mask.end(), 0,
                  m_compute_ctx->queue);

    const size_t num_voxels = (N + 2) * (N + 2) * (N + 2);
    m_voxels_scan = move(Scan(m_compute_ctx->queue, num_voxels));
    m_voxel_mask =
            compute::vector<uint32_t>(num_voxels, m_compute_ctx->context);
    m_scanned_voxels =
            compute::vector<uint32_t>(num_voxels, m_compute_ctx->context);

    m_voxel_vertices =
            compute::vector<float>(3 * num_voxels, m_compute_ctx->context);
}

void Mesher::init_kernels() {
    {
        auto program = compute::program::create_with_source_file(
                "media/kernels/selectors.cl", m_compute_ctx->context);
        program.build();

        m_select_active_edges = program.create_kernel("select_active_edges");
        m_clear = program.create_kernel("clear");
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
        : m_compute_ctx(compute_ctx),
          m_unordered_queue(compute::command_queue(
                  compute_ctx->context,
                  compute_ctx->context.get_device(),
                  compute::command_queue::enable_out_of_order_execution)) {
    init_buffers();
    init_kernels();
}

void Mesher::enqueue_select_edges(Chunk &chunk) {
    // Mark active edges
    m_select_active_edges.set_arg(0, m_edge_mask);
    m_select_active_edges.set_arg(1, chunk.samples);
    const size_t size[3] = { N + 3, N + 3, N + 3};
    auto event = m_unordered_queue.enqueue_nd_range_kernel(
            m_select_active_edges, 3, nullptr, size, nullptr);
    // Count them
    m_edges_scan.inclusive_scan(m_edge_mask, m_scanned_edges, m_unordered_queue,
                                event);
}

void Mesher::enqueue_solve_qef(Chunk &chunk) {
    m_solve_qef.set_arg(0, chunk.samples);
    m_solve_qef.set_arg(1, chunk.edges_x);
    m_solve_qef.set_arg(2, chunk.edges_y);
    m_solve_qef.set_arg(3, chunk.edges_z);
    m_solve_qef.set_arg(4, Scene::get_chunk_origin(chunk.coord));
    m_solve_qef.set_arg(5, m_voxel_vertices);
    m_solve_qef.set_arg(6, m_voxel_mask);

    static const size_t size[3] = { VM_CHUNK_SIZE + 2,
                                    VM_CHUNK_SIZE + 2,
                                    VM_CHUNK_SIZE + 2 };
    auto event =
            m_unordered_queue.enqueue_nd_range_kernel(m_solve_qef, 3, nullptr,
                                                      size, nullptr);
    // Count active voxels
    m_voxels_scan.inclusive_scan(m_voxel_mask, m_scanned_voxels,
                                 m_unordered_queue, event);
}

namespace {
size_t align(size_t value, size_t alignment = 4096) {
    if (value % alignment) {
        return value + (alignment - value % alignment);
    }
    return value;
}

void realloc_vbo_if_necessary(std::shared_ptr<ComputeContext> &ctx,
                              Chunk &chunk,
                              uint32_t num_voxels) {
    const size_t vertex_size = sizeof(glm::vec3);
    if (chunk.vbo.size() < vertex_size * num_voxels) {
        chunk.vbo = move(Buffer(BufferDesc{
            GL_ARRAY_BUFFER,
            GL_DYNAMIC_DRAW,
            nullptr,
            align(vertex_size * num_voxels)
        }));
        fprintf(stderr, "Allocated vbo of size %zu\n", chunk.vbo.size());
        chunk.cl_vbo = compute::opengl_buffer(ctx->context, chunk.vbo.id());
    }
}

void realloc_ibo_if_necessary(std::shared_ptr<ComputeContext> &ctx,
                              Chunk &chunk,
                              size_t num_edges) {
    if (chunk.ibo.size() < 6 * sizeof(unsigned) * num_edges) {
        chunk.ibo = move(Buffer(BufferDesc{
            GL_ELEMENT_ARRAY_BUFFER,
            GL_DYNAMIC_DRAW,
            nullptr,
            align(6 * sizeof(unsigned) * num_edges)
        }));
        fprintf(stderr, "Allocated ibo of size %zu\n", chunk.ibo.size());
        chunk.cl_ibo = compute::opengl_buffer(ctx->context, chunk.ibo.id());
    }
}
} // namespace

void Mesher::enqueue_contour(Chunk &chunk) {
    uint32_t num_voxels = m_scanned_voxels.back();
    uint32_t num_edges = m_scanned_edges.back();
    realloc_vbo_if_necessary(m_compute_ctx, chunk, num_voxels);
    realloc_ibo_if_necessary(m_compute_ctx, chunk, num_edges);
    chunk.num_indices = 6 * num_edges;

    // Ensure we don't have any race with acquire commands.
    glFinish();

    // TODO: Why acquiring ibo and vbo together causes deadlocks?!
    clEnqueueAcquireGLObjects(m_compute_ctx->queue.get(), 1,
                              &chunk.cl_vbo.get(), 0, nullptr, nullptr);
    m_copy_vertices.set_arg(0, chunk.cl_vbo);
    m_copy_vertices.set_arg(1, m_voxel_vertices);
    m_copy_vertices.set_arg(2, m_voxel_mask);
    m_copy_vertices.set_arg(3, m_scanned_voxels);
    m_compute_ctx->queue.enqueue_1d_range_kernel(
            m_copy_vertices, 0, (N + 2) * (N + 2) * (N + 2), 0);

    clEnqueueReleaseGLObjects(m_compute_ctx->queue.get(), 1,
                              &chunk.cl_vbo.get(), 0, nullptr, nullptr);

    clEnqueueAcquireGLObjects(m_compute_ctx->queue.get(), 1,
                              &chunk.cl_ibo.get(), 0, nullptr, nullptr);
    // TODO: Why this is necessary? I mean, the queue is ordered after all
    m_compute_ctx->queue.flush();
    m_compute_ctx->queue.finish();

    m_make_indices.set_arg(0, chunk.cl_ibo);
    m_make_indices.set_arg(1, m_edge_mask);
    m_make_indices.set_arg(2, m_scanned_edges);
    m_make_indices.set_arg(3, m_scanned_voxels);
    m_make_indices.set_arg(4, chunk.samples);

    m_compute_ctx->queue.enqueue_1d_range_kernel(
            m_make_indices, 0, 3 * (N + 3) * (N + 3) * (N + 3), 0);

    clEnqueueReleaseGLObjects(m_compute_ctx->queue.get(), 1,
                              &chunk.cl_ibo.get(), 0, nullptr, nullptr);

    m_compute_ctx->queue.flush();
    m_compute_ctx->queue.finish();
    chunk.num_vertices = num_voxels;
}

void Mesher::contour(Chunk &chunk) {
    enqueue_select_edges(chunk);
    enqueue_solve_qef(chunk);
    m_unordered_queue.finish();
#if 0
    static size_t counter = 0;
    if (counter == 10000) {
        uint32_t active_edges = m_scanned_edges.back();
        uint32_t active_voxels = m_scanned_voxels.back();
        fprintf(stderr, "Active edges: %d; active voxels %d\n", active_edges, active_voxels);
        counter = 0;
    }
    ++counter;
#endif
    enqueue_contour(chunk);
}

} // namespace dc
} // namespace vm
