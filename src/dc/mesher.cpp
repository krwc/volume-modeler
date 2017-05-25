#include <config.h>

#include "mesher.h"

using namespace std;
namespace vm {
namespace dc {

Mesher::Mesher(const shared_ptr<ComputeContext> &compute_ctx)
        : m_compute_ctx(compute_ctx),
          m_select_active_edges(),
          m_copy_selected(),
          m_solve_qef(),
          m_scan(),
          m_active_edges(),
          m_contour(),
          m_unordered_queue(compute::command_queue(
                  compute_ctx->context,
                  compute_ctx->context.get_device(),
                  compute::command_queue::enable_out_of_order_execution)) {

#define _N (VM_CHUNK_SIZE + 3)
    for (size_t i = 0; i < 3; ++i) {
        m_scan[i] = move(Scan(compute_ctx->queue, _N * _N * _N));
    }
#undef _N

#define _N (VM_CHUNK_SIZE + 2)
    for (size_t i = 0; i < 3; ++i) {
        m_active_edges[i] =
                compute::vector<uint32_t>(_N * _N * _N, compute_ctx->context);
    }
#undef _N
    {
        auto program = compute::program::create_with_source_file(
                "media/kernels/selectors.cl", compute_ctx->context);
        ostringstream os;
        os << " -w";
        os << " -cl-mad-enable";
        os << " -cl-single-precision-constant";
        os << " -cl-fast-relaxed-math";
        program.build(os.str());

        m_select_active_edges = program.create_kernel("select_active_edges");
        m_copy_selected = program.create_kernel("copy_selected");
    }

    {
        auto program = compute::program::create_with_source_file(
                "media/kernels/qef.cl", compute_ctx->context);
        program.build("-w");
        m_solve_qef = program.create_kernel("solve_qef");
    }

    {
        auto program = compute::program::create_with_source_file(
                "media/kernels/contour.cl", compute_ctx->context);
        program.build("-w");
        m_contour = program.create_kernel("contour");
    }
}

void Mesher::enqueue_get_active_edges(size_t axis) {
    return;
}

void Mesher::enqueue_solve_qef() {
}

void Mesher::enqueue_contour() {
}

void Mesher::contour(Chunk &chunk) {
    for (size_t axis = 0; axis < 3; ++axis) {
        enqueue_get_active_edges(axis);
    }
    enqueue_solve_qef();
    m_unordered_queue.flush();
    m_unordered_queue.finish();

    enqueue_contour();
    m_compute_ctx->queue.flush();
    m_compute_ctx->queue.finish();
}

} // namespace dc
} // namespace vm
