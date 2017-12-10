#ifndef UTILS_H
#define UTILS_H

constant sampler_t nearest_sampler = CLK_NORMALIZED_COORDS_FALSE
                                     | CLK_ADDRESS_CLAMP_TO_EDGE
                                     | CLK_FILTER_NEAREST;

short sample_at(read_only image3d_t samples, int x, int y, int z) {
    return read_imagei(samples, nearest_sampler, (int4)(x, y, z, 0)).x;
}

bool active_edge(short s0, short s1) {
    return (s0 > 0 && s1 == 0) || (s0 == 0 && s1 > 0) || (s0 * s1 < 0);
}

short as_sign(float value) {
    if (value < 0) {
        return -1;
    } else if (value > 0) {
        return 1;
    } else {
        return 0;
    }
}

/* No typeof() support in core it seems */
#define swap(type, a, b) \
    do {                 \
        type tmp = a;    \
        a = b;           \
        b = tmp;         \
    } while (0)

/**
 * @returns true if (x,y,z) is a voxel which would be enclosed inside a
 * voxel grid, false otherwise.
 */
#define IS_VOXEL_COORD(x, y, z) \
    ((x) < VOXEL_GRID_DIM && (y) < VOXEL_GRID_DIM && (z) < VOXEL_GRID_DIM)

/* @returns true if (x,y,z) is a valid sampling point, false otherwise. */
#define IS_SAMPLE_COORD(x, y, z) \
    ((x) < SAMPLE_GRID_DIM && (y) < SAMPLE_GRID_DIM && (z) < SAMPLE_GRID_DIM)

/* @returns true if (x,y,z) is a valid x-edge index, false otherwise. */
#define IS_X_EDGE_COORD(x, y, z) \
    ((x) < SAMPLE_GRID_DIM - 1 && (y) < SAMPLE_GRID_DIM && (z) < SAMPLE_GRID_DIM)

/* @returns true if (x,y,z) is a valid y-edge index, false otherwise. */
#define IS_Y_EDGE_COORD(x, y, z) \
    ((x) < SAMPLE_GRID_DIM && (y) < SAMPLE_GRID_DIM - 1 && (z) < SAMPLE_GRID_DIM)

/* @returns true if (x,y,z) is a valid z-edge index, false otherwise. */
#define IS_Z_EDGE_COORD(x, y, z) \
    ((x) < SAMPLE_GRID_DIM && (y) < SAMPLE_GRID_DIM && (z) < SAMPLE_GRID_DIM - 1)

/* @returns true if (x,y,z) is a valid edge index, false otherwise. */
#define IS_EDGE_COORD(x, y, z, axis)              \
    (((axis) == 0 && IS_X_EDGE_COORD(x, y, z))    \
     || ((axis) == 1 && IS_Y_EDGE_COORD(x, y, z)) \
     || ((axis) == 2 && IS_Z_EDGE_COORD(x, y, z)))

float3 vertex_at(int x, int y, int z, float3 chunk_origin) {
    /**
     * TODO OPTIMIZATION: If chunk_origin were chunk's minimal point, then the
     * coordinates of the vertex would be obtainable via single MAD:
     * chunk_origin + VOXEL_SIZE * xyz
     */
    const float3 half_dim =
            0.5f * (float3)(SAMPLE_GRID_DIM, SAMPLE_GRID_DIM, SAMPLE_GRID_DIM);
    return (float) (VM_VOXEL_SIZE) * ((float3)(x, y, z) - half_dim)
           + chunk_origin;
}

typedef struct {
    float3 row0;
    float3 row1;
    float3 row2;
} mat3;

typedef struct {
    float4 row0;
    float4 row1;
    float4 row2;
    float4 row3;
} mat4;

float3 mul_mat3_float3(mat3 A, float3 v) {
    return (float3)(dot(A.row0, v), dot(A.row1, v), dot(A.row2, v));
}

float4 mul_mat4_float4(mat4 A, float4 v) {
    return (float4)(dot(A.row0, v), dot(A.row1, v), dot(A.row2, v), dot(A.row3, v));
}

#endif // UTILS_H
