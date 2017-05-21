layout(points) in;
layout(triangle_strip, max_vertices=24) out;

uniform vec3 box_size;
uniform mat3 box_transform;
uniform mat4 view;
uniform mat4 proj;

out vec4 viewspace_position;

void emit_vertex(in vec3 vertex) {
    viewspace_position =
            view * vec4(gl_in[0].gl_Position.xyz + box_transform * vertex, 1.0);
    gl_Position = proj * viewspace_position;
    EmitVertex();
}

void emit_quad(in vec3 v0, in vec3 v1, in vec3 v2, in vec3 v3) {
    emit_vertex(v0);
    emit_vertex(v1);
    emit_vertex(v2);
    emit_vertex(v3);
    EndPrimitive();
}

void main() {
    const vec3 min = -0.5 * box_size;
    const vec3 max = +0.5 * box_size;

    /**
     * 2------3
     * |      |
     * |      |
     * 0------1
     */
    // back
    emit_quad(vec3(min.x, min.y, min.z),
              vec3(min.x, max.y, min.z),
              vec3(max.x, min.y, min.z),
              vec3(max.x, max.y, min.z));
    // left
    emit_quad(vec3(min.x, min.y, min.z),
              vec3(min.x, min.y, max.z),
              vec3(min.x, max.y, min.z),
              vec3(min.x, max.y, max.z));
    // front
    emit_quad(vec3(min.x, min.y, max.z),
              vec3(max.x, min.y, max.z),
              vec3(min.x, max.y, max.z),
              vec3(max.x, max.y, max.z));
    // right
    emit_quad(vec3(max.x, min.y, max.z),
              vec3(max.x, min.y, min.z),
              vec3(max.x, max.y, max.z),
              vec3(max.x, max.y, min.z));
    // top
    emit_quad(vec3(min.x, max.y, min.z),
              vec3(min.x, max.y, max.z),
              vec3(max.x, max.y, min.z),
              vec3(max.x, max.y, max.z));
    // bottom
    emit_quad(vec3(min.x, min.y, min.z),
              vec3(max.x, min.y, min.z),
              vec3(min.x, min.y, max.z),
              vec3(max.x, min.y, max.z));


}
