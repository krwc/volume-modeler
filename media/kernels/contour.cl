#include "config/config.h"

#include "media/kernels/utils.h"

kernel void copy_vertices(global float *out_vbo,
                          global const float *voxel_vertices,
                          global const uint *voxel_mask,
                          global const uint *scanned_voxels) {
    const uint tid = get_global_id(0);
    if (tid >= VOXEL_3D_GRID_SIZE) {
        return;
    }
    const uint mask = voxel_mask[tid];
    if (!mask) {
        return;
    }
    const uint index = 3 * (scanned_voxels[tid] - 1);
    for (uint i = 0; i < 3; ++i) {
        out_vbo[index + i] = voxel_vertices[3 * tid + i];
    }
}

int voxel_index(int x, int y, int z) {
    return x + VOXEL_GRID_DIM * (y + VOXEL_GRID_DIM * z);
}

constant uint triangles[2][6] = {
    { 0, 1, 2, 0, 2, 3 },
    { 0, 2, 1, 0, 3, 2 }
};

kernel void make_indices(global uint *out_ibo,
                         global const uint *edge_mask,
                         global const uint *scanned_edges,
                         global const uint *scanned_voxels,
                         read_only image3d_t samples) {

    const uint tid = get_global_id(0);
    if (tid >= 3 * SAMPLE_3D_GRID_SIZE) {
        return;
    }

    if (!edge_mask[tid]) {
        return;
    }
    const uint axis = tid % 3;
    const uint offset = (tid - axis) / 3;
    int e0x = offset % SAMPLE_GRID_DIM;
    int e0y = ((offset - e0x) / SAMPLE_GRID_DIM) % SAMPLE_GRID_DIM;
    int e0z = ((offset - e0x) / SAMPLE_GRID_DIM - e0y) / SAMPLE_GRID_DIM;

    int cells[4];
    cells[0] = voxel_index(e0x, e0y, e0z);

    switch (axis) {
    case 0:
        cells[1] = voxel_index(e0x, e0y - 1, e0z);
        cells[2] = voxel_index(e0x, e0y - 1, e0z - 1);
        cells[3] = voxel_index(e0x, e0y, e0z - 1);
        break;
    case 1:
        cells[1] = voxel_index(e0x, e0y, e0z - 1);
        cells[2] = voxel_index(e0x - 1, e0y, e0z - 1);
        cells[3] = voxel_index(e0x - 1, e0y, e0z);
        break;
    case 2:
        cells[1] = voxel_index(e0x - 1, e0y, e0z);
        cells[2] = voxel_index(e0x - 1, e0y - 1, e0z);
        cells[3] = voxel_index(e0x, e0y - 1, e0z);
        break;
    }

    float value = sample_at(samples, e0x, e0y, e0z);
    int triangulation = value <= 0 ? 0 : 1;
    const uint index = 6 * (scanned_edges[tid] - 1);
    for (uint i = 0; i < 6; ++i) {
        out_ibo[index + i] = scanned_voxels[cells[triangles[triangulation][i]]] - 1;
    }
}

