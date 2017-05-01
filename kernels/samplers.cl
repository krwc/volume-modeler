#cmakedefine VM_CHUNK_BORDER @VM_CHUNK_BORDER@
#cmakedefine VM_CHUNK_SIZE @VM_CHUNK_SIZE@
#cmakedefine VM_VOXEL_SIZE @VM_VOXEL_SIZE@

#pragma OPENCL EXTENSION cl_khr_3d_image_writes : enable

#include "kernels/math-utils.h"

constant sampler_t nearest_sampler = CLK_NORMALIZED_COORDS_FALSE
                                     | CLK_ADDRESS_CLAMP_TO_EDGE
                                     | CLK_FILTER_NEAREST;

kernel void initialize(write_only image3d_t output,
                       float4 value) {
    const int x = get_global_id(0);
    const int y = get_global_id(1);
    const int z = get_global_id(2);
    write_imagef(output, (int4)(x, y, z, 0), value);
}

float ball_sdf(float3 p, float3 brush_scale) {
    return (length(p / brush_scale) - 1)
           * min(min(brush_scale.x, brush_scale.y), brush_scale.z);
}

float cube_sdf(float3 p, float3 brush_scale) {
    p = fabs(p);
    return max(p.x - brush_scale.x, max(p.y - brush_scale.y, p.z - brush_scale.z));
}

float3 get_position(int x, int y, int z, float3 chunk_origin) {
    const float3 half_dim = 0.5f * (float3)(VM_CHUNK_SIZE + VM_CHUNK_BORDER,
                                            VM_CHUNK_SIZE + VM_CHUNK_BORDER,
                                            VM_CHUNK_SIZE + VM_CHUNK_BORDER);
    return (float)(VM_VOXEL_SIZE) * ((float3)(x, y, z) - half_dim) + chunk_origin;
}

#define SAMPLE_IMPL(sdf_function)                                            \
    do {                                                                     \
        const int x = get_global_id(0);                                      \
        const int y = get_global_id(1);                                      \
        const int z = get_global_id(2);                                      \
        const float2 old_sample =                                            \
                read_imagef(input, nearest_sampler, (int4)(x, y, z, 0)).xy;  \
                                                                             \
        const float3 p = get_position(x, y, z, chunk_origin) - brush_origin; \
        const float3 v = mul_mat3_vec3(brush_rotation, p);                   \
                                                                             \
        float new_sample = sdf_function(v, brush_scale);                     \
        int new_material = material_id;                                      \
        if (operation_type == 0) {                                           \
            if (new_sample >= old_sample.x) {                                \
                new_sample = old_sample.x;                                   \
                new_material = old_sample.y;                                 \
            }                                                                \
        } else {                                                             \
            new_sample = max(-new_sample, old_sample.x);                     \
            new_material = old_sample.y;                                     \
        }                                                                    \
        write_imagef(output, (int4)(x, y, z, 0),                             \
                     (float4)(new_sample, new_material, 0, 0));              \
    } while (0)

kernel void sample_sphere(read_only image3d_t input,
                          write_only image3d_t output,
                          int operation_type,
                          int material_id,
                          float3 chunk_origin,
                          float3 brush_origin,
                          float3 brush_scale,
                          mat3 brush_rotation) {
    SAMPLE_IMPL(ball_sdf);
}

kernel void sample_cube(read_only image3d_t input,
                        write_only image3d_t output,
                        int operation_type,
                        int material_id,
                        float3 chunk_origin,
                        float3 brush_origin,
                        float3 brush_scale,
                        mat3 brush_rotation) {
    SAMPLE_IMPL(cube_sdf);
}
