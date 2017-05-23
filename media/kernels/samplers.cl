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

#define _N (VM_CHUNK_SIZE + 3)
    if (x >= _N || y >= _N || z >= _N) {
        return;
    }
#undef _N

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

constant int3 axis_offset[3] = {
    (int3)(1, 0, 0),
    (int3)(0, 1, 0),
    (int3)(0, 0, 1)
};

float3 compute_sdf_normal(float3 p, float3 brush_scale, float epsilon) {
    const float2 E = (float2)(epsilon, 0);
    const float dx =
            sdf_func(p + E.xyy, brush_scale) - sdf_func(p - E.xyy, brush_scale);
    const float dy =
            sdf_func(p + E.yxy, brush_scale) - sdf_func(p - E.yxy, brush_scale);
    const float dz =
            sdf_func(p + E.yyx, brush_scale) - sdf_func(p - E.yyx, brush_scale);
    return normalize((float3)(dx, dy, dz));
}

#define MAX_BISECTION_STEPS 8
kernel void update_edges(write_only image3d_t edges,
                         int axis,
                         float3 chunk_origin,
                         float3 brush_origin,
                         float3 brush_scale,
                         mat3 brush_rotation) {
    const int3 xyz0 = (int3)(get_global_id(0),
                             get_global_id(1),
                             get_global_id(2));
    const int3 xyz1 = xyz0 + axis_offset[axis];
#define _N (VM_CHUNK_SIZE + 3)
    if (xyz1.x >= _N || xyz1.y >= _N || xyz1.z >= _N) {
        return;
    }
#undef _N
    float3 v0 = mul_mat3_float3(brush_rotation,
                                vertex_at(xyz0.x, xyz0.y, xyz0.z, chunk_origin)
                                        - brush_origin);
    float3 v1 = mul_mat3_float3(brush_rotation,
                                vertex_at(xyz1.x, xyz1.y, xyz1.z, chunk_origin)
                                        - brush_origin);
    float s0 = sdf_func(v0, brush_scale);
    float s1 = sdf_func(v1, brush_scale);
    if (s0 * s1 >= 0) {
        /* No sign change */
        return;
    }
    /* Swap, so that s0 is always inside and s1 outside the volume */
    if (s0 > 0) {
        swap(float, s0, s1);
        swap(float3, v0, v1);
    }

    float lo = 0.0f;
    float hi = 1.0f;
    float mid = 0.0f;
    float value;
    float3 point = (float3)(0, 0, 0);

    for (int i = 0; i < MAX_BISECTION_STEPS; ++i) {
        mid = 0.5f * (lo + hi);
        point = mix(v0, v1, mid);
        value = sdf_func(point, brush_scale);

        if (value > 0) {
            hi = mid;
        } else if (value < 0) {
            lo = mid;
        } else {
            break;
        }
    }
    const float3 normal = compute_sdf_normal(point, brush_scale, 1e-4);
    write_imagef(edges, (int4)(xyz0, 0), (float4)(mid, normal));
}
