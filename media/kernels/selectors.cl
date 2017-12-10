#include "config/config.h"

#include "media/kernels/utils.h"

kernel void select_active_edges(global uint *out_selection,
                                read_only image3d_t samples) {
    const int x = get_global_id(0);
    const int y = get_global_id(1);
    const int z = get_global_id(2);
    /* Ignore edges from boundary layers, as they are handled in a
     * crack-patching phase. */
    if (x < 1 || y < 1 || z < 1) {
        return;
    }
    if (x >= VOXEL_GRID_DIM || y >= VOXEL_GRID_DIM || z >= VOXEL_GRID_DIM) {
        return;
    }
    /**
     * Short explanation of what is going on (and why):
     *  1. This kernel is ran once per sample.
     *
     *  2. Each kernel invocation examines three minimal grid-edges originated
     *     at the sample point it process, and writes a triplet (u,v,w) into
     *     out_selection array, where u,v,w are either 1 or 0 depending on
     *     edge being active or not.
     *
     *  3. The results written by this kernel are then used to count active
     *     edges (via scan), and this is why such layout is used.
     *
     * Oh, and by the way, the `out_selection` array is (N+1)^3 (even though the
     * number of edges is a bit smaller) because it is easier to waste some
     * memory than to constantly verify that we don't write out of bounds.
     */
    const uint index = 3 * (x + SAMPLE_GRID_DIM * (y + SAMPLE_GRID_DIM * z));

    float s0 = sample_at(samples, x, y, z);
    float3 s1 = (float3)(sample_at(samples, x + 1, y, z),
                         sample_at(samples, x, y + 1, z),
                         sample_at(samples, x, y, z + 1));
    if (x < VOXEL_GRID_DIM) {
        out_selection[index + 0] = !!active_edge(s0, s1.x);
    }
    if (y < VOXEL_GRID_DIM) {
        out_selection[index + 1] = !!active_edge(s0, s1.y);
    }
    if (z < VOXEL_GRID_DIM) {
        out_selection[index + 2] = !!active_edge(s0, s1.z);
    }
}
