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

class Renderer {
    Program m_raymarcher;
    Program m_box_drawer;
    std::unique_ptr<Buffer> m_triangle_vbo;
    std::unique_ptr<Buffer> m_shape_vbo;

    std::shared_ptr<ComputeContext> m_compute_ctx;

    uint64_t m_origin_refs[VM_CHUNKS_PER_PASS];

    GLuint m_raymarcher_vao;
    GLuint m_shape_vao;
    int m_width;
    int m_height;

    void init_buffer();
    void init_shaders();

public:
    Renderer(std::shared_ptr<ComputeContext> ctx);
    void render(const Scene &scene);
    void render(const std::shared_ptr<Camera> &camera, const Box &box);
    void resize(int screen_width, int screen_height);
};

} // namespace vm

#endif /* VM_GFX_RENDERER_H */
