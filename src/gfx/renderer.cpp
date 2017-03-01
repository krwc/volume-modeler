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
    m_triangle_vbo = make_unique<Buffer>(BufferDesc{
        GL_ARRAY_BUFFER,
        GL_STATIC_DRAW,
        quad,
        sizeof(quad)
    });

    glCreateVertexArrays(1, &m_vao);
    assert(m_vao > 0);
    glEnableVertexArrayAttrib(m_vao, 0);
    glVertexArrayAttribFormat(m_vao, 0, 2, GL_FLOAT, GL_FALSE, 0u);
    glVertexArrayAttribBinding(m_vao, 0, 0u);
}

void Renderer::init_shaders() {
    m_raymarcher = Program{};
    m_raymarcher.define("VM_VOXEL_SIZE", to_string(VM_VOXEL_SIZE));
    m_raymarcher.define("VM_CHUNK_SIZE", to_string(VM_CHUNK_SIZE));
    m_raymarcher.define("VM_CHUNK_BORDER", to_string(VM_CHUNK_BORDER));
    m_raymarcher.define("SCREEN_WIDTH", to_string(m_width));
    m_raymarcher.define("SCREEN_HEIGHT", to_string(m_height));
    m_raymarcher.set_shader_from_file(GL_VERTEX_SHADER,
                                      "shaders/raymarcher-vs.glsl");
    m_raymarcher.set_shader_from_file(GL_FRAGMENT_SHADER,
                                      "shaders/raymarcher-fs.glsl");
    m_raymarcher.compile();
    m_raymarcher.link();
}

Renderer::Renderer()
    : m_raymarcher()
    , m_triangle_vbo()
    , m_vao(GL_NONE)
    , m_width(0)
    , m_height(0) {
    init_buffer();
    init_shaders();

    glEnable(GL_SCISSOR_TEST);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthRange(0.0f, 1.0f);
    glFrontFace(GL_CCW);
}

void Renderer::render(const Scene &scene) {
    (void) scene;
    glClearColor(0.3, 0.3, 0.3, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(m_raymarcher.id());
    glBindVertexArray(m_vao);
    glBindVertexBuffer(0, m_triangle_vbo->id(), 0, sizeof(vec2));

    auto camera = scene.get_camera();
    m_raymarcher.set_constant("inv_proj", inverse(camera->get_proj()));
    m_raymarcher.set_constant("inv_view", inverse(camera->get_view()));
    m_raymarcher.set_constant("camera_origin", camera->get_origin());
    m_raymarcher.set_constant("camera_far_plane", camera->get_far_plane());

    for (const Scene::Chunk *chunk : scene.get_chunks_to_render()) {
        m_raymarcher.set_constant("chunk_origin", chunk->origin);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_3D, chunk->volume->id());
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }
}

void Renderer::resize(int screen_width, int screen_height) {
    m_width = screen_width;
    m_height = screen_height;
    glViewport(0, 0, m_width, m_height);
    glScissor(0, 0, m_width, m_height);
    init_shaders();
}

} // namespace vm
