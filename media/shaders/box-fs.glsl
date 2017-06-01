out vec4 color;

in vec4 viewspace_position;

uniform vec4 box_color;

void main() {
    color = box_color;
}
