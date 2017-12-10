out vec4 out_color;
in vec3 vs_position;
in vec3 gs_normal;

void main() {
    const vec3 L = normalize(vec3(1,1,1));
    out_color = vec4(gs_normal.xyz*abs(min(0.1, dot(gs_normal, -L))), 1);
}
