#ifndef UTILS_H
#define UTILS_H

constant sampler_t nearest_sampler = CLK_NORMALIZED_COORDS_FALSE
                                     | CLK_ADDRESS_CLAMP_TO_EDGE
                                     | CLK_FILTER_NEAREST;

float sample_at(read_only image3d_t samples, int x, int y, int z) {
    return read_imagef(samples, nearest_sampler, (int4)(x, y, z, 0)).x;
}

bool active_edge(float s0, float s1) {
    return (s0 > 0 && s1 == 0) || (s0 == 0 && s1 > 0) || (s0 * s1 < 0);
}

/* No typeof() support in core it seems */
#define swap(type, a, b) \
    do {                 \
        type tmp = a;    \
        a = b;           \
        b = tmp;         \
    } while (0)

float3 vertex_at(int x, int y, int z, float3 chunk_origin) {
    // OPTIMIZATION: If chunk_origin were chunk's minimal point, then the
    // coordinates of the vertex would be obtainable via single MAD:
    // chunk_origin + VOXEL_SIZE * xyz
    const float3 half_dim =
            0.5f
            * (float3)(VM_CHUNK_SIZE + 3, VM_CHUNK_SIZE + 3, VM_CHUNK_SIZE + 3);
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
