layout(binding=0) uniform sampler2D image;
layout(binding=1) uniform sampler2D depth;
layout(binding=2) uniform sampler2DArray materials;
uniform mat4 inv_proj;
uniform mat4 inv_view;
uniform float camera_far;
uniform vec3 camera_origin;
in vec2 uv;
out vec4 color;

vec4 triplanar(in float scale, in vec3 N, in vec3 P, in int material_id) {
    vec3 blending = clamp(abs(N) - 0.5, 0.0, 1.0);
    blending *= blending;
    blending *= blending;

    blending /= dot(blending, vec3(1,1,1));
    vec4 xaxis = texture(materials, vec3(P.yz*scale, material_id));
    vec4 yaxis = texture(materials, vec3(P.xz*scale, material_id));
    vec4 zaxis = texture(materials, vec3(P.xy*scale, material_id));
    return xaxis*blending.x + yaxis*blending.y + zaxis*blending.z;
}

vec3 get_ray(int px, int py) {
    const float x = 2.0 * px / float(SCREEN_WIDTH) - 1.0;
    const float y = 2.0 * py / float(SCREEN_HEIGHT) - 1.0;
    vec4 dir = inv_proj * vec4(x, y, -1.0, 1.0);
    dir = inv_view * vec4(dir.x, dir.y, -1.0, 0.0);
    return normalize(vec3(dir.x, dir.y, dir.z));
}

vec3 get_position(int px, int py, float z) {
    return camera_origin + z * camera_far * get_ray(px, py);
}

float light_intensity(in vec3 normal) {
    const vec3 light_dir = normalize(vec3(-1,-1,-1));
    return max(0.1, dot(normal, -light_dir));
}

void main() {
    const float depth = texture(depth, uv).r;
    const vec4 pixel = texture(image, uv);
    gl_FragDepth = depth;

    if (pixel.w >= 0) {
        const int material_id = int(pixel.w);
        const vec3 position =
                get_position(int(gl_FragCoord.x), int(gl_FragCoord.y), depth);
        float I = light_intensity(pixel.xyz);
        color = vec4(I * triplanar(1, pixel.xyz, position, material_id).xyz, 1.0);
    } else {
        color = vec4(pixel.xyz, 1.0);
    }
}
