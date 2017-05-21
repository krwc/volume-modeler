out vec4 color;

in vec4 viewspace_position;

uniform vec4 box_color;
uniform float camera_far_plane;

void main() {
    color = box_color;
    gl_FragDepth = length(viewspace_position.xyz) / camera_far_plane;
}
