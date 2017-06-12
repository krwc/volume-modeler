#cmakedefine VM_CHUNK_SIZE @VM_CHUNK_SIZE@
#cmakedefine VM_VOXEL_SIZE @VM_VOXEL_SIZE@

#include "media/kernels/utils.h"
#include "media/kernels/qef_solver.h"

/* NOTE: The numbers are actually bitmasks */
constant int edge_vertex[12][2] = {
    // x, y, x, y
    { 0, 4 },       // (0,0,0) <-> (1,0,0)
    { 4, 6 },       // (1,0,0) <-> (1,1,0)
    { 2, 6 },       // (0,1,0) <-> (1,1,0)
    { 0, 2 },       // (0,0,0) <-> (0,1,0)
    // x, y, x, y
    { 1, 5 },       // (0,0,1) <-> (1,0,1)
    { 5, 7 },       // (1,0,1) <-> (1,1,1)
    { 3, 7 },       // (0,1,1) <-> (1,1,1)
    { 1, 3 },       // (0,0,1) <-> (0,1,1)
    // z, z, z, z
    { 0, 1 },       // (0,0,0) <-> (0,0,1)
    { 4, 5 },       // (1,0,0) <-> (1,0,1)
    { 6, 7 },       // (1,1,0) <-> (1,1,1)
    { 2, 3 }        // (0,1,0) <-> (0,1,1)
};

constant int edge_axis[12] = {
    0, 1, 0, 1,
    0, 1, 0, 1,
    2, 2, 2, 2
};

constant int edge_offset[12][3] = {
    { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 },
    { 0, 0, 0 }, { 0, 0, 1 }, { 1, 0, 1 },
    { 0, 1, 1 }, { 0, 0, 1 }, { 0, 0, 0 },
    { 1, 0, 0 }, { 1, 1, 0 }, { 0, 1, 0 }
};

int3 edge_vertex_offset(int edge, int endpoint) {
    const int vx = (edge_vertex[edge][endpoint] >> 2) & 1;
    const int vy = (edge_vertex[edge][endpoint] >> 1) & 1;
    const int vz = (edge_vertex[edge][endpoint] >> 0) & 1;
    return (int3)(vx, vy, vz);
}

float4 edge_data(read_only image3d_t edges_x,
                 read_only image3d_t edges_y,
                 read_only image3d_t edges_z,
                 int vx,
                 int vy,
                 int vz,
                 int edge) {
    const int4 xyzw = (int4)(vx + edge_offset[edge][0],
                             vy + edge_offset[edge][1],
                             vz + edge_offset[edge][2],
                             0);
    switch (edge_axis[edge]) {
    default:
    case 0:
        return read_imagef(edges_x, nearest_sampler, xyzw);
    case 1:
        return read_imagef(edges_y, nearest_sampler, xyzw);
    case 2:
        return read_imagef(edges_z, nearest_sampler, xyzw);
    }
}

float3 edge_p0(int vx, int vy, int vz, int edge, float3 chunk_origin) {
    const int3 offset = edge_vertex_offset(edge, 0);
    return vertex_at(vx + offset.x, vy + offset.y, vz + offset.z, chunk_origin);
}

float3 edge_p1(int vx, int vy, int vz, int edge, float3 chunk_origin) {
    const int3 offset = edge_vertex_offset(edge, 1);
    return vertex_at(vx + offset.x, vy + offset.y, vz + offset.z, chunk_origin);
}

bool is_edge_active(float samples[2][2][2], int edge) {
    const int3 v0 = edge_vertex_offset(edge, 0);
    const int3 v1 = edge_vertex_offset(edge, 1);
    const float s0 = samples[v0.z][v0.y][v0.x];
    const float s1 = samples[v1.z][v1.y][v1.x];
    return active_edge(s0, s1);
}

kernel void solve_qef(read_only image3d_t samples,
                      read_only image3d_t edges_x,
                      read_only image3d_t edges_y,
                      read_only image3d_t edges_z,
                      float3 chunk_origin,
                      global float *voxel_vertices,
                      global uint *voxel_mask) {
    const int x = get_global_id(0);
    const int y = get_global_id(1);
    const int z = get_global_id(2);
    if (!IS_EXTENDED_VOXEL_COORD(x, y, z)) {
        return;
    }
    /* Each sample in a voxel will be accessed more than once, so it makes
       sense to not fetch it from the global memory every time. */
    float values[2][2][2];
    for (int k = 0; k < 2; ++k) {
        for (int j = 0; j < 2; ++j) {
            for (int i = 0; i < 2; ++i) {
                values[k][j][i] = sample_at(samples, x + i, y + j, z + k);
            }
        }
    }
    uint active_edges = 0;
    float4 positions[12];
    float4 normals[12];

    for (int e = 0; e < 12; ++e) {
        if (is_edge_active(values, e)) {
            float3 p0 = edge_p0(x, y, z, e, chunk_origin);
            float3 p1 = edge_p1(x, y, z, e, chunk_origin);
            float4 tag = edge_data(edges_x, edges_y, edges_z, x, y, z, e);
            positions[active_edges] = (float4)(mix(p0, p1, tag.w), 0);
            normals[active_edges] = (float4)(tag.xyz, 0);
            ++active_edges;
        }
    }

    const uint index =
            x + DIM_EXTENDED_VOXEL_GRID * (y + DIM_EXTENDED_VOXEL_GRID * z);

    if (active_edges > 0) {
        const float3 vertex = qef_solve(positions, normals, active_edges);
        voxel_vertices[3 * index + 0] = vertex.x;
        voxel_vertices[3 * index + 1] = vertex.y;
        voxel_vertices[3 * index + 2] = vertex.z;
    }
    voxel_mask[index] = !!active_edges;
}
