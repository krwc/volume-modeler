#include "program.h"

#include "utils/log.h"

#include <cassert>
#include <iterator>
#include <fstream>
#include <sstream>

namespace vm {
using namespace std;

void Program::destroy() {
    if (m_id) {
        glDeleteProgram(m_id);
    }

    for (ShaderDesc &desc : m_shaders) {
        glDeleteShader(desc.id);
    }
    m_shaders.clear();
}

Program &Program::operator=(Program &&other) {
    if (&other != this) {
        destroy();
        m_id = other.m_id;
        m_constants = move(other.m_constants);
        m_defines = move(other.m_defines);
        m_shaders = move(other.m_shaders);
        other.m_id = GL_NONE;
    }
    return *this;
}

Program::Program(Program &&other) : m_id(GL_NONE) {
    *this = move(other);
}

Program::Program()
    : m_id(GL_NONE)
    , m_constants()
    , m_defines()
    , m_shaders() {
    m_id = glCreateProgram();
    assert(m_id > 0);
}

Program::~Program() {
    destroy();
}

void Program::define(const string &name, const string &value) {
    m_defines[name] = value;
}

namespace {
string read_file(const string &path) {
    ifstream f(path);
    if (!f.is_open()) {
        throw invalid_argument("Cannot open: " + path);
    }
    return string(istreambuf_iterator<char>(f), istreambuf_iterator<char>());
}
}

void Program::set_shader_from_file(GLenum type, const string &file) {
    m_shaders.push_back(ShaderDesc{ read_file(file), file, type, glCreateShader(type) });
}

void Program::compile_shader(ShaderDesc &desc) {
    string preprocessed_source = "#version 430\n";
    for (auto &&it : m_defines) {
        preprocessed_source += "#define " + it.first + " " + it.second + "\n";
    }
    /* We don't want to expose our little hack with appending defines */
    preprocessed_source += "#line 0\n";
    preprocessed_source += desc.source;

    GLint length = static_cast<GLint>(preprocessed_source.length());
    const char *source = preprocessed_source.c_str();
    glShaderSource(desc.id, 1, &source, &length);
    int status, log_length;
    glCompileShader(desc.id);
    glGetShaderiv(desc.id, GL_COMPILE_STATUS, &status);
    glGetShaderiv(desc.id, GL_INFO_LOG_LENGTH, &log_length);
    string compilation_log;

    if (status == GL_FALSE) {
        compilation_log.resize(log_length - 1, '\0');
        glGetShaderInfoLog(desc.id, log_length - 1, &log_length,
                           &compilation_log[0]);
        stringstream error;
        error << "Failed to compile: "
              << desc.filename
              << '\n'
              << compilation_log;
        throw runtime_error(error.str());
    }
}

void Program::compile() {
    for (ShaderDesc &desc : m_shaders) {
        compile_shader(desc);
    }
}

void Program::link() {
    for (ShaderDesc &desc : m_shaders) {
        glAttachShader(m_id, desc.id);
    }
    glLinkProgram(m_id);
    for (ShaderDesc &desc : m_shaders) {
        glDetachShader(m_id, desc.id);
    }

    int status;
    glGetProgramiv(m_id, GL_LINK_STATUS, &status);
    int length;
    glGetProgramiv(m_id, GL_INFO_LOG_LENGTH, &length);
    if (length > 0) {
        string log(length - 1, 0);
        glGetProgramInfoLog(m_id, length - 1, &length, &log[0]);
        LOG(debug) << "Program link log: " << endl << log;
    }
    if (status == GL_FALSE) {
        throw runtime_error("Cannot link program");
    }
    discover_constants();
}

void Program::discover_constants() {
    m_constants.clear();
    int num_active_uniforms = 0;
    glGetProgramInterfaceiv(m_id, GL_UNIFORM, GL_ACTIVE_RESOURCES, &num_active_uniforms);
    enum {
        PROPERTY_BLOCK_INDEX = 0,
        PROPERTY_NAME_LENGTH = 1,
        PROPERTY_LOCATION = 2,
        PROPERTY_TYPE = 3
    };
    const GLenum properties[4] = {
        GL_BLOCK_INDEX,
        GL_NAME_LENGTH,
        GL_LOCATION,
        GL_TYPE
    };

    for (int index = 0; index < num_active_uniforms; ++index) {
        GLint results[4];
        glGetProgramResourceiv(m_id, GL_UNIFORM, index, 4, properties, 4,
                               nullptr, results);

        /* Ignore uniforms located in Uniform Blocks */
        if (results[PROPERTY_BLOCK_INDEX] != -1) {
            continue;
        }

        string uniform_name(results[PROPERTY_NAME_LENGTH] - 1, '\0');
        glGetProgramResourceName(m_id, GL_UNIFORM, index,
                                 results[PROPERTY_NAME_LENGTH], nullptr,
                                 &uniform_name[0]);
        m_constants[uniform_name] = Program::ParamDesc {
            (GLenum) results[PROPERTY_TYPE],
            (GLint) results[PROPERTY_LOCATION]
        };

        auto opening_brace = uniform_name.find('[');
        if (opening_brace == string::npos) {
            continue;
        }
        /* Thank you OpenGL for treating arrays differently. Now my life is
           much, much easier */
        const string array_name = uniform_name.substr(0, opening_brace);
        for (int index = 1; ; ++index) {
            string element = array_name + "[" + to_string(index) + "]";
            GLint location = glGetUniformLocation(m_id, element.c_str());
            if (location < 0) {
                break;
            }

            m_constants[element] = Program::ParamDesc {
                (GLenum) results[PROPERTY_TYPE],
                location
            };
        }
    }
}

uint64_t Program::get_constant_ref(const string &name) {
    if (const Program::ParamDesc *desc = find_constant(name)) {
        return reinterpret_cast<uint64_t>(desc);
    }
    return numeric_limits<uint64_t>::max();
}

void Program::set_constant(const ParamDesc &desc, const void *data) {
    const GLenum type = desc.type;
    const GLint location = desc.location;
    switch (type) {
    case GL_INT:
    case GL_BOOL:
    case GL_SAMPLER_2D:
    case GL_SAMPLER_2D_ARRAY:
    case GL_SAMPLER_2D_SHADOW:
        glProgramUniform1i(m_id, location, *((const GLint *) data));
        break;
    case GL_FLOAT:
        glProgramUniform1fv(m_id, location, 1, (const GLfloat *) data);
        break;
    case GL_FLOAT_VEC2:
        glProgramUniform2fv(m_id, location, 1, (const GLfloat *) data);
        break;
    case GL_FLOAT_VEC3:
        glProgramUniform3fv(m_id, location, 1, (const GLfloat *) data);
        break;
    case GL_FLOAT_VEC4:
        glProgramUniform4fv(m_id, location, 1, (const GLfloat *) data);
        break;
    case GL_FLOAT_MAT3:
        glProgramUniformMatrix3fv(m_id, location, 1, GL_FALSE,
                                  (const GLfloat *) data);
        break;
    case GL_FLOAT_MAT4:
        glProgramUniformMatrix4fv(m_id, location, 1, GL_FALSE,
                                  (const GLfloat *) data);
        break;
    case GL_INT_VEC3:
        glProgramUniform3iv(m_id, location, 1, (const GLint *) data);
        break;
    default:
        assert(0 && "constant type not supported");
        return;
    }
}

const Program::ParamDesc *Program::find_constant(const string &name) const {
    auto &&it = m_constants.find(name);
    if (it != m_constants.end()) {
        return &it->second;
    }
    return nullptr;
}

} // namespace vm
