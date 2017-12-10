#include "config/config.h"

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

#define OPERATION_ADD 0
#define OPERATION_SUB 1

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

    if (!IS_SAMPLE_COORD(x, y, z)) {
        return;
    }

    const short old_sample =
            read_imagei(samples_in, nearest_sampler, (int4)(x, y, z, 0)).x;

    short new_sample = as_sign(sdf_func(vertex_at(x, y, z, chunk_origin),
                                     brush_origin,
                                     brush_scale,
                                     brush_rotation));
    switch (operation_type) {
    default:
    case OPERATION_ADD:
        new_sample = min(new_sample, old_sample);
        break;
    case OPERATION_SUB:
        new_sample = max((short)(-new_sample), old_sample);
        break;
    }
    write_imagei(samples_out, (int4)(x, y, z, 0),
                 (int4)(new_sample, 0, 0, 0));
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
    /* Minimal edge endpoint */
    const int x0 = get_global_id(0);
    const int y0 = get_global_id(1);
    const int z0 = get_global_id(2);

    if (!IS_EDGE_COORD(x0, y0, z0, axis)) {
        return;
    }
    /* Maximal edge endpoint */
    const int x1 = get_global_id(0) + (axis == 0);
    const int y1 = get_global_id(1) + (axis == 1);
    const int z1 = get_global_id(2) + (axis == 2);

    float3 v0 = vertex_at(x0, y0, z0, chunk_origin);
    float3 v1 = vertex_at(x1, y1, z1, chunk_origin);
    short s0 = as_sign(sdf_func(v0, brush_origin, brush_scale, brush_rotation));
    short s1 = as_sign(sdf_func(v1, brush_origin, brush_scale, brush_rotation));

    /* This must be weaker than active_edge() or otherwise SDF subtraction
       won't work. */
    if (s0 * s1 > 0) {
        /* No sign change */
        return;
    }

    /* Swap, so that s0 is always inside and s1 outside the volume */
    bool swapped = false;
    if (s0 > s1) {
        swap(float3, v0, v1);
        swapped = true;
    }

    float lo = 0.0f;
    float hi = 1.0f;
    float mid = 0.0f;
    float value;
    float3 point;

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
    if (swapped) {
        mid = 1 - mid;
    }
    const float3 normal = compute_sdf_normal(point, brush_origin, brush_scale,
                                             brush_rotation, 1e-5);
    write_imagef(edges, (int4)(x0, y0, z0, 0), (float4)(normal, mid));
}
