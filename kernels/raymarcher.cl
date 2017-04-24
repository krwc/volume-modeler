#cmakedefine VM_CHUNK_BORDER @VM_CHUNK_BORDER@
#cmakedefine VM_CHUNK_SIZE @VM_CHUNK_SIZE@
#cmakedefine VM_VOXEL_SIZE @VM_VOXEL_SIZE@

#include "kernels/math-utils.h"

typedef struct {
    mat4 inv_proj;
    mat4 inv_view;
    float3 origin;
    float near;
    float far;
} Camera;

float3 get_ray(int px, int py, const Camera *camera) {
    const float x = 2.0f * px / (float) SCREEN_WIDTH - 1.0f;
    const float y = 2.0f * py / (float) SCREEN_HEIGHT - 1.0f;
    float4 dir = mul_mat4_vec4(camera->inv_proj, (float4)(x, y, -1.0f, 1.0f));
    dir = mul_mat4_vec4(camera->inv_view, (float4)(dir.x, dir.y, -1.0f, 0.0f));
    return normalize((float3)(dir.x, dir.y, dir.z));
}

float texture_box_sdf(const float3 *p, const float3 *origin, float size) {
    const float3 v = fabs(*p - *origin) - size;
    return max(v.x, max(v.y, v.z));
}

bool is_ray_hitting_volume(float3 chunk_origin,
                           float3 o,
                           float3 inv_r,
                           float *out_tmin,
                           float *out_tmax) {
    const float half_box = 0.5f * (VM_VOXEL_SIZE * VM_CHUNK_SIZE);
    const float3 half_box_vec = (float3)(half_box, half_box, half_box);
    const float3 aabb_min = chunk_origin - half_box_vec;
    const float3 aabb_max = chunk_origin + half_box_vec;

    // https://tavianator.com/fast-branchless-raybounding-box-intersections/
    float tmin = -1e9;
    float tmax = +1e9;

    float3 t1 = (aabb_min - o) * inv_r;
    float3 t2 = (aabb_max - o) * inv_r;

    tmin = max(tmin, min(t1.x, t2.x));
    tmin = max(tmin, min(t1.y, t2.y));
    tmin = max(tmin, min(t1.z, t2.z));

    tmax = min(tmax, max(t1.x, t2.x));
    tmax = min(tmax, max(t1.y, t2.y));
    tmax = min(tmax, max(t1.z, t2.z));

    if (tmax >= 0 && tmax >= tmin) {
        *out_tmin = tmin;
        *out_tmax = tmax;
        return true;
    }
    *out_tmin = (float) 1e9;
    *out_tmax = (float) -1e9;
    return false;
}

const sampler_t bilinear_sampler = CLK_NORMALIZED_COORDS_TRUE
                                   | CLK_ADDRESS_CLAMP_TO_EDGE
                                   | CLK_FILTER_LINEAR;

const sampler_t nearest_sampler = CLK_NORMALIZED_COORDS_TRUE
                                  | CLK_ADDRESS_CLAMP_TO_EDGE
                                  | CLK_FILTER_NEAREST;

float volume_sample(read_only image3d_t volume, const float3 *chunk_origin, float3 p) {
    const float3 q =
            (p - *chunk_origin) / (float) ((VM_CHUNK_SIZE + VM_CHUNK_BORDER)
                                           * VM_VOXEL_SIZE)
            + 0.5f;
    return read_imagef(volume, bilinear_sampler, (float4)(q.x, q.y, q.z, 0.0f)).x;
}

#define TOLERANCE (0.05f * VM_VOXEL_SIZE)

float4 volume_sdf(read_only image3d_t volume,
                  const float3 *chunk_origin,
                  const float3 *p) {
    float d = texture_box_sdf(p, chunk_origin, 0.5f * VM_CHUNK_SIZE * VM_VOXEL_SIZE);
    if (d >= TOLERANCE) {
        return (float4)(0.0f, 0.0f, 0.0f, d);
    }
    const float uv_offset = 3.8f * 0.5f / (VM_CHUNK_SIZE + VM_CHUNK_BORDER);
    const float sp0 =
            volume_sample(volume, chunk_origin, *p + (float3)(uv_offset, 0, 0));
    const float sp1 =
            volume_sample(volume, chunk_origin, *p + (float3)(0, uv_offset, 0));
    const float sp2 =
            volume_sample(volume, chunk_origin, *p + (float3)(0, 0, uv_offset));
    const float sm0 =
            volume_sample(volume, chunk_origin, *p - (float3)(uv_offset, 0, 0));
    const float sm1 =
            volume_sample(volume, chunk_origin, *p - (float3)(0, uv_offset, 0));
    const float sm2 =
            volume_sample(volume, chunk_origin, *p - (float3)(0, 0, uv_offset));

    const float3 normal = normalize((float3)(sp0 - sm0, sp1 - sm1, sp2 - sm2));
    const float avg = (sp0 + sp1 + sp2 + sm0 + sm1 + sm2) / 6.0f;
    return (float4)(normal.x, normal.y, normal.z, avg);
}

float ball_sdf(float3 p, float3 brush_scale) {
    return (length(p / brush_scale) - 1)
           * min(min(brush_scale.x, brush_scale.y), brush_scale.z);
}

#define MAX_STEPS 128
float4 raymarch(read_only image3d_t volume,
                const Camera *camera,
                const float3 *chunk_origin,
                const float3 *r,
                float z) {
    float3 current = z * (*r) + camera->origin;
    if (volume_sdf(volume, chunk_origin, &current).w <= TOLERANCE) {
        return (float4)(0.3f, 0.3f, 0.3f, z);
    }

    for (int i = 0; i < MAX_STEPS; ++i) {
        float3 p = camera->origin + z * (*r);
        float4 s = volume_sdf(volume, chunk_origin, &p);

        if (s.w <= TOLERANCE) {
            float3 normal = s.xyz;
            float3 color = normal;
            return (float4)(color.x, color.y, color.z, z);
        }
        z += s.w;
    }
    return (float4)(0.3f, 0.3f, 0.3f, camera->far);
}

kernel void initialize(write_only image2d_t output,
                       float4 value) {
    const int u = get_global_id(0);
    const int v = get_global_id(1);
    write_imagef(output, (int2)(u, v), value);
}

kernel void raymarcher(write_only image2d_t output,
                       read_only image2d_t in_depth,
                       write_only image2d_t out_depth,
                       read_only image3d_t volume,
                       float3 chunk_origin,
                       Camera camera) {
    // TODO: Why are such (u,v) coords faster than default?!
    const int u = SCREEN_WIDTH - 1 - get_global_id(0);
    const int v = /*SCREEN_HEIGHT - 1 -*/ get_global_id(1);
    const float3 r = get_ray(u, v, &camera);
    const float3 inv_r = 1.0f / r;
    float tmin = 0.0f;
    float tmax = 0.0f;

    float4 result = (float4)(0.3f, 0.3f, 0.3f, camera.far);
    // TODO: FOR LOOP OVER ALL VOLUMES
    float depth = read_imagef(in_depth, nearest_sampler, (int2)(u, v)).x;

    if (is_ray_hitting_volume(chunk_origin, camera.origin, inv_r, &tmin, &tmax)) {
        if (tmin < camera.far && tmin / camera.far < depth) {
            float4 current = raymarch(volume, &camera, &chunk_origin, &r,
                                      max(0.25f, (float)(tmin - VM_VOXEL_SIZE)));
            if (current.w < result.w) {
                result = current;
            }

            write_imagef(output, (int2)(u, v),
                         (float4)(result.x, result.y, result.z, 1.0f));

            result.w /= camera.far;
            write_imagef(out_depth, (int2)(u, v),
                         (float4)(result.w, result.w, result.w, result.w));
        }
    }
}
