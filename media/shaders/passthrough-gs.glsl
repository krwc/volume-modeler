layout(triangles) in;
layout(triangle_strip, max_vertices=3) out;

uniform mat4 g_mvp;

out vec3 gs_normal;

void main() {
    vec3 v0 = gl_in[0].gl_Position.xyz;
    vec3 v1 = gl_in[1].gl_Position.xyz;
    vec3 v2 = gl_in[2].gl_Position.xyz;
    vec3 normal = normalize(cross(v1-v0, v2-v0));

    gl_Position = g_mvp * vec4(v0, 1);
    gs_normal = normal;
    EmitVertex();
    gl_Position = g_mvp * vec4(v1, 1);
    gs_normal = normal;
    EmitVertex();
    gl_Position = g_mvp * vec4(v2, 1);
    gs_normal = normal;
    EmitVertex();
    EndPrimitive();
}
