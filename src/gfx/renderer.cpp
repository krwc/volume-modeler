#include "config.h"
#include "renderer.h"

#include "scene/scene.h"

#include "utils/compute-interop.h"

#include <glm/glm.hpp>
#include <iostream>
#include <sstream>

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
#warning "TODO: remove old raymarcher shaders"
    m_tex_drawer = Program{};
    m_tex_drawer.set_shader_from_file(GL_VERTEX_SHADER,
                                      "shaders/texture-vs.glsl");
    m_tex_drawer.set_shader_from_file(GL_FRAGMENT_SHADER,
                                      "shaders/texture-fs.glsl");
    m_tex_drawer.compile();
    m_tex_drawer.link();

    m_box_drawer = Program{};
    m_box_drawer.set_shader_from_file(GL_GEOMETRY_SHADER,
                                      "shaders/box-gs.glsl");
    m_box_drawer.set_shader_from_file(GL_VERTEX_SHADER,
                                      "shaders/box-vs.glsl");
    m_box_drawer.set_shader_from_file(GL_FRAGMENT_SHADER,
                                      "shaders/box-fs.glsl");
    m_box_drawer.compile();
    m_box_drawer.link();
}

void Renderer::init_kernels() {
    auto program =
            compute::program::create_with_source_file("kernels/raymarcher.cl",
                                                      m_compute_ctx->context);
    ostringstream options;
    options << " -DSCREEN_WIDTH=" << m_width;
    options << " -DSCREEN_HEIGHT=" << m_height;
    options << " -w";
    options << " -cl-mad-enable";
    options << " -cl-single-precision-constant";
    options << " -cl-fast-relaxed-math";
    program.build(options.str());
    m_raymarcher = program.create_kernel("raymarcher");
    m_initializer = program.create_kernel("initialize");
}

void Renderer::init_textures() {
    m_frame =
            make_unique<Texture2d>(TextureDesc2d{ m_width, m_height, GL_RGBA });
    m_frame->set_parameter(GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    m_frame->set_parameter(GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    m_frame->clear(0,0,0,0);
    m_cl_frame = compute::opengl_texture(m_compute_ctx->context, GL_TEXTURE_2D,
                                         0, m_frame->id());

    m_depth = compute::image2d(
            m_compute_ctx->context, m_width, m_height,
            compute::image_format(compute::image_format::r,
                                  compute::image_format::float32));
}

Renderer::Renderer(shared_ptr<ComputeContext> ctx)
    : m_compute_ctx(ctx)
    , m_raymarcher_vao(GL_NONE)
    , m_shape_vao(GL_NONE)
    , m_width(0)
    , m_height(0) {
    init_buffer();
    init_shaders();
    init_kernels();

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
    // Make sure OpenGL finished, or otherwise deadlocks may happen.
    glFlush();
    glFinish();

    auto camera = scene.get_camera();
    struct T1 {
        Mat4Repr inv_proj;
        Mat4Repr inv_view;
        Vec3Repr origin;
        cl_float near;
        cl_float far;
    } camera_repr;

    camera_repr.inv_proj = Mat4Repr(inverse(camera->get_proj()));
    camera_repr.inv_view = Mat4Repr(inverse(camera->get_view()));
    camera_repr.origin = Vec3Repr(camera->get_origin());
    camera_repr.near = camera->get_near_plane();
    camera_repr.far = camera->get_far_plane();

    // TODO: Boost somehow does some craziness when acquiring context (it creates some
    // temporary contexts fuck knows why). Therefore we're using lower-level API to
    // avoid that.
    clEnqueueAcquireGLObjects(m_compute_ctx->queue.get(),
                              1,
                              &m_cl_frame.get(),
                              0,
                              nullptr,
                              nullptr);

    // Clear frame
    m_initializer.set_arg(0, m_cl_frame);
    m_initializer.set_arg(1, vec4(0.3, 0.3, 0.3, 1));
    m_compute_ctx->queue.enqueue_nd_range_kernel(
            m_initializer, 2, nullptr, compute::dim(m_width, m_height).data(),
            nullptr);
    // Clear depth
    m_initializer.set_arg(0, m_depth);
    m_initializer.set_arg(1, vec4(1, 1, 1, 1));
    m_compute_ctx->queue.enqueue_nd_range_kernel(
            m_initializer, 2, nullptr, compute::dim(m_width, m_height).data(),
            nullptr);

    m_raymarcher.set_arg(0, m_cl_frame);
    m_raymarcher.set_arg(1, m_depth);
    m_raymarcher.set_arg(2, m_depth);
    m_raymarcher.set_arg(3, sizeof(camera_repr), &camera_repr);

#warning "TODO: multiple images per job (or not? it was slower when I initially tested it)"
    for (auto &&chunk : scene.get_chunks_to_render()) {
        m_raymarcher.set_arg(4, *chunk->volume);
        m_raymarcher.set_arg(5, chunk->origin);

        m_compute_ctx->queue.enqueue_nd_range_kernel(
                m_raymarcher,
                2,
                nullptr,
                compute::dim(m_width, m_height).data(),
                nullptr);
    }

    clEnqueueReleaseGLObjects(m_compute_ctx->queue.get(),
                              1,
                              &m_cl_frame.get(),
                              0,
                              nullptr,
                              nullptr);

    m_compute_ctx->queue.flush();
    m_compute_ctx->queue.finish();

    // Draw frame on the screen
    glClearColor(0.3, 0.3, 0.3, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glBindVertexArray(m_raymarcher_vao);
    glBindVertexBuffer(0, m_triangle_vbo->id(), 0, sizeof(vec2));

    glUseProgram(m_tex_drawer.id());
    glBindTextureUnit(0, m_frame->id());
    glDrawArrays(GL_TRIANGLES, 0, 6);

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
    init_kernels();
    init_textures();
}

} // namespace vm
