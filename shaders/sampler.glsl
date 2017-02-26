layout(local_size_x=1, local_size_y=1, local_size_z=1) in;
layout(r16f, binding=0) uniform image3D volume;

#define BRUSH_CUBE  0
#define BRUSH_BALL  1
uniform int brush;
uniform vec3 brush_origin;
uniform vec3 brush_scale;
uniform mat3 brush_rotation;

#define OPERATION_ADD   0
#define OPERATION_SUB   1
uniform int operation;

uniform vec3 chunk_origin;

float ball_sdf(in vec3 p) {
    const vec3 v = brush_rotation * (p - brush_origin);
    return (length(v / brush_scale) - 1) * min(min(brush_scale.x, brush_scale.y), brush_scale.z);
}

float cube_sdf(in vec3 p) {
    const vec3 v = abs(brush_rotation * (p - brush_origin));
    return max(v.x - brush_scale.x, max(v.y - brush_scale.y, v.z - brush_scale.z));
}

vec3 get_position(in ivec3 xyz) {
    const vec3 half_dim = 0.5 * vec3(VM_CHUNK_SIZE, VM_CHUNK_SIZE, VM_CHUNK_SIZE);
    return VM_VOXEL_SIZE * (xyz - half_dim) + chunk_origin;
}

void main() {
    const ivec3 xyz = ivec3(gl_GlobalInvocationID);
    const vec3 p = get_position(xyz);
    const float old_sample = imageLoad(volume, xyz).r;
    float new_sample;

    if (brush == BRUSH_CUBE) {
        new_sample = cube_sdf(p);
    } else {
        new_sample = ball_sdf(p);
    }

    if (operation == OPERATION_ADD) {
        new_sample = min(new_sample, old_sample);
    } else {
        new_sample = max(-new_sample, old_sample);
    }

    imageStore(volume, xyz, vec4(new_sample));
}
