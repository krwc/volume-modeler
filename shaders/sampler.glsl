layout(local_size_x=1, local_size_y=1, local_size_z=1) in;
layout(r16f, binding=0) uniform image3D volume;

void main() {
    imageStore(volume, ivec3(gl_GlobalInvocationID), vec4(1,1,1,1));
}
