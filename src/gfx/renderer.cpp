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

    m_shape_vbo = make_unique<Buffer>(BufferDesc{
        GL_ARRAY_BUFFER,
        GL_DYNAMIC_DRAW,
        nullptr,
        4096
    });

    glCreateVertexArrays(1, &m_raymarcher_vao);
    assert(m_raymarcher_vao > 0);
    glEnableVertexArrayAttrib(m_raymarcher_vao, 0);
    glVertexArrayAttribFormat(m_raymarcher_vao, 0, 2, GL_FLOAT, GL_FALSE, 0u);
    glVertexArrayAttribBinding(m_raymarcher_vao, 0, 0);

    glCreateVertexArrays(1, &m_shape_vao);
    glEnableVertexArrayAttrib(m_shape_vao, 0);
    glVertexArrayAttribFormat(m_shape_vao, 0, 3, GL_FLOAT, GL_FALSE, 0u);
    glVertexArrayAttribBinding(m_shape_vao, 0, 0);
}

void Renderer::init_shaders() {
    m_raymarcher = Program{};
    m_raymarcher.define("VM_VOXEL_SIZE", to_string(VM_VOXEL_SIZE));
    m_raymarcher.define("VM_CHUNK_SIZE", to_string(VM_CHUNK_SIZE));
    m_raymarcher.define("VM_CHUNK_BORDER", to_string(VM_CHUNK_BORDER));
    m_raymarcher.define("VM_CHUNKS_PER_PASS", to_string(VM_CHUNKS_PER_PASS));
    m_raymarcher.define("SCREEN_WIDTH", to_string(m_width));
    m_raymarcher.define("SCREEN_HEIGHT", to_string(m_height));
    m_raymarcher.set_shader_from_file(GL_VERTEX_SHADER,
                                      "shaders/raymarcher-vs.glsl");
    m_raymarcher.set_shader_from_file(GL_FRAGMENT_SHADER,
                                      "shaders/raymarcher-fs.glsl");
    m_raymarcher.compile();
    m_raymarcher.link();

    m_box_drawer = Program{};
    m_box_drawer.set_shader_from_file(GL_GEOMETRY_SHADER,
                                      "shaders/box-gs.glsl");
    m_box_drawer.set_shader_from_file(GL_VERTEX_SHADER,
                                      "shaders/box-vs.glsl");
    m_box_drawer.set_shader_from_file(GL_FRAGMENT_SHADER,
                                      "shaders/box-fs.glsl");
    m_box_drawer.compile();
    m_box_drawer.link();

    for (size_t i = 0; i < VM_CHUNKS_PER_PASS; ++i) {
        m_origin_refs[i] =
            m_raymarcher.get_constant_ref("chunk_origin[" + to_string(i) + "]");
    }
}

Renderer::Renderer()
    : m_raymarcher()
    , m_triangle_vbo()
    , m_raymarcher_vao(GL_NONE)
    , m_shape_vao(GL_NONE)
    , m_width(0)
    , m_height(0) {
    init_buffer();
    init_shaders();

    glEnable(GL_SCISSOR_TEST);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glDepthFunc(GL_LEQUAL);
    glDepthRange(0.0f, 1.0f);
    glFrontFace(GL_CCW);
}

void Renderer::render(const Scene &scene) {
    (void) scene;
    glClearColor(0.3, 0.3, 0.3, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(m_raymarcher.id());
    glBindVertexArray(m_raymarcher_vao);
    glBindVertexBuffer(0, m_triangle_vbo->id(), 0, sizeof(vec2));

    auto camera = scene.get_camera();
    m_raymarcher.set_constant("inv_proj", inverse(camera->get_proj()));
    m_raymarcher.set_constant("inv_view", inverse(camera->get_view()));
    m_raymarcher.set_constant("camera_origin", camera->get_origin());
    m_raymarcher.set_constant("camera_far_plane", camera->get_far_plane());

    auto &&chunks = scene.get_chunks_to_render();

    for (size_t i = 0; i < chunks.size(); i += VM_CHUNKS_PER_PASS) {
        int num_chunks = 0;
        for (size_t j = i; j-i < VM_CHUNKS_PER_PASS && j < chunks.size(); j++) {
            m_raymarcher.set_constant(m_origin_refs[j-i], chunks[j]->origin);
            glActiveTexture(GL_TEXTURE0 + j-i);
            glBindTexture(GL_TEXTURE_3D, chunks[j]->volume->id());
            ++num_chunks;
        }
        m_raymarcher.set_constant("num_chunks", num_chunks);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }
#if 0
    for (const Scene::Chunk *chunk : scene.get_chunks_to_render()) {
        const vec3 c = chunk->origin;
        const vec3 h = vec3(0.5f,0.5f,0.5f) * float(VM_VOXEL_SIZE * (VM_CHUNK_SIZE+VM_CHUNK_BORDER));
        render(camera, Box{c - h, c + h, vec4(0.6, 0.4, 0.1, 0.5)});
    }
#endif
}

void Renderer::render(const shared_ptr<Camera> &camera, const Box &box) {
    m_box_drawer.set_constant("box_size", box.max - box.min);
    m_box_drawer.set_constant("box_color", box.color);
    m_box_drawer.set_constant("box_transform", box.transform);
    m_box_drawer.set_constant("proj", camera->get_proj());
    m_box_drawer.set_constant("view", camera->get_view());
    m_box_drawer.set_constant("camera_far_plane", camera->get_far_plane());

    const vec3 origin = 0.5f * (box.max + box.min);
    m_shape_vbo->update(&origin, sizeof(origin), 0);

    glUseProgram(m_box_drawer.id());
    glBindVertexArray(m_shape_vao);
    glBindVertexBuffer(0, m_shape_vbo->id(), 0, sizeof(vec3));
    glDrawArrays(GL_POINTS, 0, 1);
}

void Renderer::resize(int screen_width, int screen_height) {
    m_width = screen_width;
    m_height = screen_height;
    glViewport(0, 0, m_width, m_height);
    glScissor(0, 0, m_width, m_height);
    init_shaders();
}

} // namespace vm
