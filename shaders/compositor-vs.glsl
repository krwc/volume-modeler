layout(location=0) in vec2 in_position;
out vec2 uv;

void main() {
    uv = (in_position + 1.0) * 0.5;
    gl_Position = vec4(in_position, 0.0, 1.0);
}
