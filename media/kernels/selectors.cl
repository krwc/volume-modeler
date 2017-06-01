#cmakedefine VM_CHUNK_SIZE @VM_CHUNK_SIZE@
#cmakedefine VM_VOXEL_SIZE @VM_VOXEL_SIZE@

#include "media/kernels/utils.h"

kernel void clear(global uint *out_buffer, uint size) {
    const uint index = get_global_id(0);
    if (index < size) {
        out_buffer[index] = 0;
    }
}

kernel void select_active_edges(global uint *out_selection,
                                read_only image3d_t samples) {
    const int x = get_global_id(0);
    const int y = get_global_id(1);
    const int z = get_global_id(2);
    if (x < 1 || y < 1 || z < 1) {
        return;
    }
    if (x > VM_CHUNK_SIZE + 1 || y > VM_CHUNK_SIZE + 1 || z > VM_CHUNK_SIZE + 1) {
        return;
    }
    const uint offset =
            3 * (x + (VM_CHUNK_SIZE + 3) * (y + (VM_CHUNK_SIZE + 3) * z));

    float s0 = sample_at(samples, x, y, z);
    float3 s1 = (float3)(sample_at(samples, x + 1, y, z),
                         sample_at(samples, x, y + 1, z),
                         sample_at(samples, x, y, z + 1));
    if (x < VM_CHUNK_SIZE + 1) {
        out_selection[offset + 0] = active_edge(s0, s1.x) ? 1 : 0;
    }
    if (y < VM_CHUNK_SIZE + 1) {
        out_selection[offset + 1] = active_edge(s0, s1.y) ? 1 : 0;
    }
    if (z < VM_CHUNK_SIZE + 1) {
        out_selection[offset + 2] = active_edge(s0, s1.z) ? 1 : 0;
    }
}
