#cmakedefine VM_CHUNK_BORDER @VM_CHUNK_BORDER@
#cmakedefine VM_CHUNK_SIZE @VM_CHUNK_SIZE@
#cmakedefine VM_VOXEL_SIZE @VM_VOXEL_SIZE@

#pragma OPENCL EXTENSION cl_khr_3d_image_writes : enable

#include "media/kernels/utils.h"

float sdf_ball(float3 p, float3 scale) {
    return (length(p / scale) - 1) * min(min(scale.x, scale.y), scale.z);
}

float sdf_cube(float3 p, float3 scale) {
    p = fabs(p);
    return max(p.x - scale.x, max(p.y - scale.y, p.z - scale.z));
}

#if defined(BRUSH_BALL)
#define sdf_func sdf_ball
#elif defined(BRUSH_CUBE)
#define sdf_func sdf_cube
#endif

kernel void sample(read_only image3d_t samples_in,
                   write_only image3d_t samples_out,
                   int operation_type,
                   float3 chunk_origin,
                   float3 brush_origin,
                   float3 brush_scale,
                   mat3 brush_rotation) {
    const int x = get_global_id(0);
    const int y = get_global_id(1);
    const int z = get_global_id(2);

    if (x >= VM_CHUNK_SIZE + 3
         || y >= VM_CHUNK_SIZE + 3
         || z >= VM_CHUNK_SIZE + 3) {
        return;
    }

    const float old_sample =
            read_imagef(samples_in, nearest_sampler, (int4)(x, y, z, 0)).x;
    const float3 p = vertex_at(x, y, z, chunk_origin) - brush_origin;
    const float3 v = mul_mat3_float3(brush_rotation, p);

    float new_sample = sdf_func(v, brush_scale);
    if (operation_type == 0) {
        new_sample = min(new_sample, old_sample);
    } else {
        new_sample = max(-new_sample, old_sample);
    }
    write_imagef(samples_out, (int4)(x, y, z, 0),
                 (float4)(new_sample, 0, 0, 0));
}

#define MAX_BISECTION_STEPS 8
kernel void update_edges(read_only image3d_t edges,
                         int axis,
                         float3 chunk_origin,
                         float3 brush_origin,
                         float3 brush_scale,
                         mat3 brush_rotation) {
    const int x = get_global_id(0);
    const int y = get_global_id(1);
    const int z = get_global_id(2);

    if (x >= VM_CHUNK_SIZE + 2
         || y >= VM_CHUNK_SIZE + 2
         || z >= VM_CHUNK_SIZE + 2) {
        return;
    }
}
