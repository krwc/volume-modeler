#ifndef VM_GFX_RENDERER_H
#define VM_GFX_RENDERER_H
#include <memory>

#include "gfx/buffer.h"
#include "gfx/program.h"
#include "gfx/texture.h"

namespace vm {
class Scene;

class Renderer {
    Program m_sampler;
    Program m_raymarcher;
    std::unique_ptr<Buffer> m_triangle_vbo;
    GLuint m_vao;
    int m_width;
    int m_height;

    void init_buffer();
    void init_shaders();

public:
    Renderer();
    void render(const Scene &scene);
    void resize(int screen_width, int screen_height);
};

} // namespace vm

#endif /* VM_GFX_RENDERER_H */
