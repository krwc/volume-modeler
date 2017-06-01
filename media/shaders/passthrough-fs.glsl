out vec4 out_color;
in vec3 vs_position;
in vec3 gs_normal;

void main() {
    out_color = vec4(gs_normal, 1);
}
