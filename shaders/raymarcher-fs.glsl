layout(location=0) out vec4 FragColor;
layout(binding=0) uniform sampler3D volume;

uniform mat4 inv_proj;
uniform mat4 inv_view;
uniform vec3 camera_origin;
uniform float camera_far_plane;

uniform float tolerance = 0.001;
uniform vec3 chunk_origin;

in vec2 uv;

vec3 get_ray(in ivec2 xy) {
    const float x = 2.0 * xy.x / SCREEN_WIDTH - 1.0;
    const float y = 2.0 * xy.y / SCREEN_HEIGHT - 1.0;
    vec4 dir = inv_proj * vec4(x, y, -1.0, 1.0);
    dir = inv_view * vec4(dir.x, dir.y, -1.0, 0.0);
    return normalize(vec3(dir));
}

float texture_box_sdf(in vec3 p, in vec3 origin, in float size) {
    const vec3 v = abs(p - origin) - size;
    return max(v.x, max(v.y, v.z));
}

bool is_ray_hitting_volume(in vec3 o, in vec3 inv_r, out float out_tmin) {
    const float half_box = 0.5 * (VM_VOXEL_SIZE * (VM_CHUNK_SIZE + 1));
    const vec3 aabb_min = chunk_origin - half_box * vec3(1,1,1);
    const vec3 aabb_max = chunk_origin + half_box * vec3(1,1,1);

    // https://tavianator.com/fast-branchless-raybounding-box-intersections/
    float tmin = -1e9;
    float tmax = +1e9;

    vec3 t1 = (aabb_min - o) * inv_r;
    vec3 t2 = (aabb_max - o) * inv_r;

    tmin = max(tmin, min(t1.x, t2.x));
    tmin = max(tmin, min(t1.y, t2.y));
    tmin = max(tmin, min(t1.z, t2.z));

    tmax = min(tmax, max(t1.x, t2.x));
    tmax = min(tmax, max(t1.y, t2.y));
    tmax = min(tmax, max(t1.z, t2.z));

    if (tmax >= 0 && tmax >= tmin) {
        out_tmin = tmin;
        return true;
    }
    out_tmin = -1e9;
    return false;
}

float volume_sample(in vec3 p) {
    const vec3 q = (p - chunk_origin) / ((VM_CHUNK_SIZE + VM_CHUNK_BORDER) * VM_VOXEL_SIZE) + 0.5;
    return texture(volume, q).r;
}

vec4 volume_sdf(in vec3 p) {
    float d = texture_box_sdf(p, chunk_origin, 0.5 * VM_CHUNK_SIZE * VM_VOXEL_SIZE);

    if (d >= tolerance) {
        return vec4(0, 0, 0, d);
    }

    const float uv_offset = 3.8 * 0.5 / (VM_CHUNK_SIZE);
    const float sp0 = volume_sample(p + vec3(uv_offset, 0, 0));
    const float sp1 = volume_sample(p + vec3(0, uv_offset, 0));
    const float sp2 = volume_sample(p + vec3(0, 0, uv_offset));

    const float sm0 = volume_sample(p - vec3(uv_offset, 0, 0));
    const float sm1 = volume_sample(p - vec3(0, uv_offset, 0));
    const float sm2 = volume_sample(p - vec3(0, 0, uv_offset));

    const vec3 normal = normalize(vec3(sp0 - sm0, sp1 - sm1, sp2 - sm2));
    return vec4(normal, (sp0 + sp1 + sp2 + sm0 + sm1 + sm2) / 6);
}

#define MAX_STEPS 128
vec4 raymarch(in ivec2 xy, in vec3 o, in vec3 r, in float z) {
    if (volume_sdf(o + r * z).w <= tolerance) {
        return vec4(0.3, 0.3, 0.3, z);
    }

    for (int i = 0; i < MAX_STEPS; ++i) {
        vec3 p = o + z * r;
        vec4 s = volume_sdf(p);

        if (s.w <= tolerance) {
            vec3 normal = s.xyz;
            vec3 color = normal;
            return vec4(color, z);
        }
        z += s.w;
    }
    return vec4(0.3, 0.3, 0.3, camera_far_plane);
}

void main() {
    const ivec2 xy = ivec2(gl_FragCoord.xy);
    const vec3 r = get_ray(xy);
    const vec3 inv_r = 1.0 / r;
    float t = 0;

    if (is_ray_hitting_volume(camera_origin, inv_r, t)) {
        /* Far beyond camera far plane */
        if (t >= camera_far_plane) {
            discard;
        }

        vec4 result = raymarch(xy, camera_origin, r, max(0.25, t));
        if (result.w >= camera_far_plane) {
            discard;
        }

        FragColor = vec4(result.xyz, 1.0);
        gl_FragDepth = result.w / camera_far_plane;
    } else {
        FragColor = vec4(0.3, 0.3, 0.3, 1);
        gl_FragDepth = camera_far_plane;
    }
}
