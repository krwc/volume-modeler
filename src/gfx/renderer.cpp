#include "config.h"

#include "renderer.h"
#include "image.h"

#include "scene/scene.h"

#include "compute/interop.h"

#include "utils/log.h"

#include <glm/glm.hpp>
#include <iostream>
#include <sstream>
#include <set>

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

    glEnableVertexArrayAttrib(m_geometry_vao, 0);
    glVertexArrayAttribFormat(m_geometry_vao, 0, 3, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribBinding(m_geometry_vao, 0, 0);

    glEnableVertexArrayAttrib(m_shape_vao, 0);
    glVertexArrayAttribFormat(m_shape_vao, 0, 3, GL_FLOAT, GL_FALSE, 0u);
    glVertexArrayAttribBinding(m_shape_vao, 0, 0);
}

void Renderer::init_shaders() {
    m_passthrough = Program{};
    m_passthrough.set_shader_from_file(GL_VERTEX_SHADER,
                                       "media/shaders/passthrough-vs.glsl");
    m_passthrough.set_shader_from_file(GL_FRAGMENT_SHADER,
                                       "media/shaders/passthrough-fs.glsl");
    m_passthrough.set_shader_from_file(GL_GEOMETRY_SHADER,
                                       "media/shaders/passthrough-gs.glsl");
    m_passthrough.compile();
    m_passthrough.link();

    m_box_drawer = Program{};
    m_box_drawer.set_shader_from_file(GL_GEOMETRY_SHADER,
                                      "media/shaders/box-gs.glsl");
    m_box_drawer.set_shader_from_file(GL_VERTEX_SHADER,
                                      "media/shaders/box-vs.glsl");
    m_box_drawer.set_shader_from_file(GL_FRAGMENT_SHADER,
                                      "media/shaders/box-fs.glsl");
    m_box_drawer.compile();
    m_box_drawer.link();
}

void Renderer::init_materials(const vector<string> &materials) {
    set<pair<int, int>> extents;
    for (size_t layer = 0; layer < materials.size(); ++layer) {
        LOG(trace) << "Loading material " << materials[layer];

        Image img(materials[layer]);
        extents.emplace(img.width, img.height);

        if (extents.size() > 1) {
            throw logic_error("All material textures must have same size.");
        }

        if (!m_material_array) {
            m_material_array = make_unique<TextureArray>(TextureArrayDesc{
                extents.begin()->first,
                extents.begin()->second,
                static_cast<int>(materials.size()),
                GL_RGBA
            });
            m_material_array->set_parameter(GL_TEXTURE_MIN_FILTER,
                                            GL_LINEAR_MIPMAP_LINEAR);
            m_material_array->set_parameter(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }
        m_material_array->fill(layer, img.pixels, GL_RGB, GL_UNSIGNED_BYTE);
    }
    if (m_material_array) {
        m_material_array->generate_mipmaps();
    }
}

Renderer::Renderer(shared_ptr<ComputeContext> ctx,
                   const vector<string> &materials)
    : m_compute_ctx(ctx)
    , m_geometry_vao()
    , m_shape_vao()
    , m_width(0)
    , m_height(0) {
    init_buffer();
    init_shaders();
    init_materials(materials);

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
    // Draw frame on the screen
    glClearColor(0.3, 0.3, 0.3, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    auto camera = scene.get_camera();

    glUseProgram(m_passthrough.id());
    m_passthrough.set_constant("g_mvp",
                               camera->get_proj() * camera->get_view());

    glBindVertexArray(m_geometry_vao);
    for (const auto &chunk : scene.get_chunks_to_render()) {
        if (!chunk->num_vertices) {
            continue;
        }
        glBindVertexBuffer(0, chunk->vbo.id(), 0, sizeof(vec3));
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, chunk->ibo.id());
        glDrawElements(GL_TRIANGLES, chunk->num_indices, GL_UNSIGNED_INT, 0u);
    }
}

void Renderer::render(const shared_ptr<Camera> &camera, const Box &box) {
    m_box_drawer.set_constant("box_size", box.max - box.min);
    m_box_drawer.set_constant("box_color", box.color);
    m_box_drawer.set_constant("box_transform", box.transform);
    m_box_drawer.set_constant("proj", camera->get_proj());
    m_box_drawer.set_constant("view", camera->get_view());

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
