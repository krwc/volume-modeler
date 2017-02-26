#include "config.h"
#include "renderer.h"

#include "scene/scene.h"

#include <glm/glm.hpp>

namespace vm {
using namespace glm;
using namespace std;

namespace {
const vec2 quad[] = {
    { -1, -1 }, { 1, -1 }, { -1, 1 },
    { -1, 1 },  { 1, -1 }, { 1, 1 }
};
}

void Renderer::init_buffer() {
    m_triangle_vbo = make_unique<Buffer>(
            BufferDesc{ GL_ARRAY_BUFFER, GL_STATIC_DRAW, quad, sizeof(quad) });

    glCreateVertexArrays(1, &m_vao);
    assert(m_vao > 0);
    glEnableVertexArrayAttrib(m_vao, 0);
    glVertexArrayAttribFormat(m_vao, 0, 2, GL_FLOAT, GL_FALSE, 0u);
    glVertexArrayAttribBinding(m_vao, 0, 0u);
}

void Renderer::init_shaders() {
    m_sampler.define("VM_VOXEL_SIZE", to_string(VM_VOXEL_SIZE));
    m_sampler.define("VM_CHUNK_SIZE", to_string(VM_CHUNK_SIZE));
    m_sampler.set_shader_from_file(GL_COMPUTE_SHADER, "shaders/sampler.glsl");
    m_sampler.compile();
    m_sampler.link();

    m_raymarcher.define("VM_VOXEL_SIZE", to_string(VM_VOXEL_SIZE));
    m_raymarcher.define("VM_CHUNK_SIZE", to_string(VM_CHUNK_SIZE));
    m_raymarcher.set_shader_from_file(GL_COMPUTE_SHADER,
                                      "shaders/raymarcher.glsl");
    m_raymarcher.compile();
    m_raymarcher.link();

    m_compositor.set_shader_from_file(GL_VERTEX_SHADER,
                                      "shaders/compositor-vs.glsl");
    m_compositor.set_shader_from_file(GL_FRAGMENT_SHADER,
                                      "shaders/compositor-fs.glsl");
    m_compositor.compile();
    m_compositor.link();
}

Renderer::Renderer()
    : m_sampler()
    , m_raymarcher()
    , m_compositor()
    , m_triangle_vbo()
    , m_vao(GL_NONE)
    , m_width(0)
    , m_height(0) {
    init_buffer();
    init_shaders();
}

void Renderer::render(const Scene &scene) {
    (void) scene;
    glUseProgram(m_compositor.id());
    glBindVertexArray(m_vao);
    glBindVertexBuffer(0, m_triangle_vbo->id(), 0, sizeof(vec2));
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void Renderer::resize(int screen_width, int screen_height) {
    m_width = screen_width;
    m_height = screen_height;
    glViewport(0, 0, m_width, m_height);
    glScissor(0, 0, m_width, m_height);
}

} // namespace vm
