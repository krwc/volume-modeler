#ifndef VM_GFX_PROGRAM_H
#define VM_GFX_PROGRAM_H
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>

#include <map>
#include <string>
#include <vector>
#include <stdexcept>

namespace vm {

class Program {
    struct ParamDesc {
        GLenum type;
        GLint location;
    };

    struct ShaderDesc {
        std::string source;
        std::string filename;
        GLenum type;
        GLuint id;
    };

    void destroy();

    void discover_constants();
    void set_constant(const ParamDesc &desc, const void *value);
    void compile_shader(ShaderDesc &desc);
    const ParamDesc *find_constant(const std::string &name) const;

    GLuint m_id;
    std::map<std::string, struct ParamDesc> m_constants;
    std::map<std::string, std::string> m_defines;
    std::vector<ShaderDesc> m_shaders;

public:
    Program(const Program &) = delete;
    Program &operator=(const Program &) = delete;

    Program &operator=(Program &&);
    Program(Program &&);

    Program();
    ~Program();

    /** Defines constant value to be used during the preprocessing step. */
    void define(const std::string &name, const std::string &value = "");

    /** Sets the source code of the shader from file */
    void set_shader_from_file(GLenum type, const std::string &path);

    /** Sets shader uniform value, or throws */
    template <typename T>
    void set_constant(const std::string &name, const T &value) {
        if (auto &&desc = find_constant(name)) {
            set_constant(*desc, (const void *) &value);
        } else {
            throw std::invalid_argument("invalid constant: " + name);
        }
    }

    /** Performs preprocessing step, compiles all shaders and gets ready to link
     * the program */
    void compile();

    /** Links the program making it ready for use */
    void link();

    /** Returns an OpenGL program handle to use by @ref glUseProgram */
    inline GLuint id() const {
        return m_id;
    }
};

} // namespace vm

#endif /* VM_GFX_PROGRAM_H */
