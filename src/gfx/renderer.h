#ifndef VM_GFX_RENDERER_H
#define VM_GFX_RENDERER_H
#include <memory>
#include "config.h"

#include "gfx/buffer.h"
#include "gfx/compute-context.h"
#include "gfx/program.h"
#include "gfx/texture.h"

#include <glm/glm.hpp>

namespace vm {
class Scene;
class Camera;

struct Box {
    glm::vec3 min;
    glm::vec3 max;
    glm::vec4 color;
    glm::mat3 transform;
};

class RaiiGLObject {
public:
    typedef std::function<void(GLsizei, GLuint *)> CreateFunc;
    typedef std::function<void(GLsizei, GLuint *)> DeleteFunc;

    RaiiGLObject(const RaiiGLObject &) = delete;
    RaiiGLObject &operator=(const RaiiGLObject &) = delete;

    RaiiGLObject(CreateFunc &&create_f, DeleteFunc &&delete_f)
        : m_id(GL_NONE)
        , m_deleter(delete_f) {
        create_f(1, &m_id);
        assert(m_id > 0);
    }

    ~RaiiGLObject() {
        if (m_id != GL_NONE) {
            m_deleter(1, &m_id);
        }
    }

    RaiiGLObject(RaiiGLObject &&other) : m_id(GL_NONE) {
        *this = std::move(other);
    }

    RaiiGLObject &operator=(RaiiGLObject &&other) {
        if (&other != this) {
            m_id = other.m_id;
            other.m_id = GL_NONE;
        }
        return *this;
    }

    operator GLuint() const {
        return m_id;
    }

private:
    GLuint m_id;
    DeleteFunc m_deleter;

};

class Vao : public RaiiGLObject {
public:
    Vao() : RaiiGLObject(glCreateVertexArrays, glDeleteVertexArrays) {}
};

class Renderer {
    Program m_tex_drawer;
    Program m_box_drawer;
    std::unique_ptr<Buffer> m_triangle_vbo;
    std::unique_ptr<Buffer> m_shape_vbo;

    std::shared_ptr<ComputeContext> m_compute_ctx;
    std::unique_ptr<Texture2d> m_frame;
    std::unique_ptr<Texture2d> m_depth;
    std::unique_ptr<TextureArray> m_material_array;

    compute::kernel m_raymarcher;
    compute::kernel m_initializer;
    compute::opengl_texture m_cl_depth;
    compute::opengl_texture m_cl_frame;

    Vao m_triangle_vao;
    Vao m_shape_vao;
    int m_width;
    int m_height;

    void init_buffer();
    void init_shaders();
    void init_kernels();
    void init_textures();
    void init_materials(const std::vector<std::string> &);

public:
    Renderer(std::shared_ptr<ComputeContext> ctx,
             const std::vector<std::string> &materials);
    void render(const Scene &scene);
    void render(const std::shared_ptr<Camera> &camera, const Box &box);
    void resize(int screen_width, int screen_height);
};

} // namespace vm

#endif /* VM_GFX_RENDERER_H */
