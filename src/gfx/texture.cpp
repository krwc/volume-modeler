#include "texture.h"

#include <cstdio>
#include <cassert>

namespace vm {

void Texture::set_parameter(GLenum parameter, int value) {
    glTextureParameteri(get_type(), parameter, value);
}

void Texture::set_parameter(GLenum parameter, float value) {
    glTextureParameterf(get_type(), parameter, value);
}

void Texture::generate_mipmaps() {
    glGenerateTextureMipmap(id());
}

namespace {
size_t get_type_size(GLenum type) {
    switch (type) {
    case GL_BYTE:
    case GL_UNSIGNED_BYTE:
        return sizeof(char);
    case GL_FLOAT:
        return sizeof(float);
    default:
        assert(0 && "unsupported type");
        return -1;
    }
}

size_t get_format_size(GLenum format) {
    switch (format) {
    case GL_RED:
        return 1;
    case GL_RG:
        return 2;
    case GL_RGB:
        return 3;
    case GL_RGBA:
        return 4;
    default:
        assert(0 && "unsupported format");
        return -1;
    }
}
}

Texture2d::Texture2d(const TextureDesc2d &desc)
    : m_id(GL_NONE)
    , m_desc(desc) {
    glCreateTextures(GL_TEXTURE_2D, 1, &m_id);
    assert(m_id > 0);

    GLint current_texture = GL_NONE;
    glGetIntegerv(GL_TEXTURE_2D, &current_texture);
    glBindTexture(GL_TEXTURE_2D, m_id);
    glTexImage2D(GL_TEXTURE_2D, 0, desc.internal_format, desc.width,
                 desc.height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glBindTexture(GL_TEXTURE_2D, current_texture);
    fprintf(stderr, "Created %dx%d texture %u\n", m_desc.width, m_desc.height, m_id);
}

Texture2d::~Texture2d() {
    glDeleteTextures(1, &m_id);
}

void Texture2d::write(int level,
                      int x,
                      int y,
                      int w,
                      int h,
                      const void *data,
                      GLenum data_format,
                      GLenum data_type) {
    fprintf(stderr, "Writing %u bytes to texture %u\n",
            w * h * get_type_size(data_type) * get_format_size(data_format),
            m_id);
    glTextureSubImage2D(m_id, level, x, y, w, h, data_format, data_type, data);
}

Texture3d::Texture3d(const TextureDesc3d &desc)
    : m_id(GL_NONE)
    , m_desc(desc) {
    glCreateTextures(GL_TEXTURE_3D, 1, &m_id);
    assert(m_id > 0);

    GLint current_texture = GL_NONE;
    glGetIntegerv(GL_TEXTURE_3D, &current_texture);
    glBindTexture(GL_TEXTURE_3D, m_id);
    glTexImage3D(GL_TEXTURE_3D, 0, desc.internal_format, desc.width,
                 desc.height, desc.depth, 0, GL_RGBA, GL_FLOAT, nullptr);
    glBindTexture(GL_TEXTURE_3D, current_texture);
    fprintf(stderr, "Created %dx%dx%d texture\n", m_desc.width, m_desc.height,
            m_desc.depth);
}

Texture3d::~Texture3d() {
    glDeleteTextures(1, &m_id);
}

void Texture3d::write(int level,
                      int x,
                      int y,
                      int z,
                      int w,
                      int h,
                      int d,
                      const void *data,
                      GLenum data_format,
                      GLenum data_type) {
    fprintf(stderr, "Writing %u bytes to texture %u\n",
            w * h * d * get_type_size(data_type) * get_format_size(data_format),
            m_id);
    glTextureSubImage3D(m_id, level, x, y, z, w, h, d, data_format, data_type,
                        data);
}

}
