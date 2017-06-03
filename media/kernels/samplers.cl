#cmakedefine VM_CHUNK_BORDER @VM_CHUNK_BORDER@
#cmakedefine VM_CHUNK_SIZE @VM_CHUNK_SIZE@
#cmakedefine VM_VOXEL_SIZE @VM_VOXEL_SIZE@

#pragma OPENCL EXTENSION cl_khr_3d_image_writes : enable

#include "media/kernels/utils.h"

float sdf_ball(float3 p, float3 origin, float3 scale, mat3 rotation) {
    return (length(mul_mat3_float3(rotation, p - origin) / scale) - 1)
           * min(min(scale.x, scale.y), scale.z);
}

float sdf_cube(float3 p, float3 origin, float3 scale, mat3 rotation) {
    p = fabs(mul_mat3_float3(rotation, p - origin));
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

    float new_sample = sdf_func(vertex_at(x, y, z, chunk_origin), brush_origin,
                                brush_scale, brush_rotation);
    if (operation_type == 0) {
        new_sample = min(new_sample, old_sample);
    } else {
        new_sample = max(-new_sample, old_sample);
    }
    write_imagef(samples_out, (int4)(x, y, z, 0),
                 (float4)(new_sample, 0, 0, 0));
}

float3 compute_sdf_normal(float3 p,
                          float3 origin,
                          float3 scale,
                          mat3 rotation,
                          float epsilon) {
    const float2 E = (float2)(epsilon, 0);
    const float f = sdf_func(p, origin, scale, rotation);
    const float dx = sdf_func(p + E.xyy, origin, scale, rotation)
                     - sdf_func(p - E.xyy, origin, scale, rotation);
    const float dy = sdf_func(p + E.yxy, origin, scale, rotation)
                     - sdf_func(p - E.yxy, origin, scale, rotation);
    const float dz = sdf_func(p + E.yyx, origin, scale, rotation)
                     - sdf_func(p - E.yyx, origin, scale, rotation);
    return normalize((float3)(dx, dy, dz));
}

#define MAX_BISECTION_STEPS 16
kernel void update_edges(write_only image3d_t edges,
                         int axis,
                         float3 chunk_origin,
                         float3 brush_origin,
                         float3 brush_scale,
                         mat3 brush_rotation) {
    const int xyz0[3] = { get_global_id(0),
                          get_global_id(1),
                          get_global_id(2) };
#define _N (VM_CHUNK_SIZE + 3)
    if (xyz0[axis] >= _N - 1) {
        return;
    }
    if (xyz0[0] >= _N || xyz0[1] >= _N || xyz0[2] >= _N) {
        return;
    }
#undef _N
    const int xyz1[3] = { xyz0[0] + (axis == 0),
                          xyz0[1] + (axis == 1),
                          xyz0[2] + (axis == 2) };

    float3 v0 = vertex_at(xyz0[0], xyz0[1], xyz0[2], chunk_origin);
    float3 v1 = vertex_at(xyz1[0], xyz1[1], xyz1[2], chunk_origin);
    float s0 = sdf_func(v0, brush_origin, brush_scale, brush_rotation);
    float s1 = sdf_func(v1, brush_origin, brush_scale, brush_rotation);
    /* Yes, s0*s1, or otherwise bad things gonna happen */
    if (s0 * s1 > 0) {
        /* No sign change */
        return;
    }
    /* Swap, so that s0 is always inside and s1 outside the volume */
    bool swapped = false;
    if (s0 > s1) {
        swap(float, s0, s1);
        swap(float3, v0, v1);
        swapped = true;
    }

    float lo = 0.0f;
    float hi = 1.0f;
    float mid = 0.0f;
    float value;
    float3 point = (float3)(0, 0, 0);

    for (int i = 0; i < MAX_BISECTION_STEPS; ++i) {
        mid = 0.5f * (lo + hi);
        point = mix(v0, v1, mid);
        value = sdf_func(point, brush_origin, brush_scale, brush_rotation);

        if (value > 0) {
            hi = mid;
        } else if (value < 0) {
            lo = mid;
        } else {
            break;
        }
    }
    const float3 normal = compute_sdf_normal(point, brush_origin, brush_scale, brush_rotation, 1e-5);
    write_imagef(edges, (int4)(xyz0[0], xyz0[1], xyz0[2], 0),
                 (float4)(normal, swapped ? 1 - mid : mid));
}
