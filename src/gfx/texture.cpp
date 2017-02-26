#include "texture.h"

#include <cstdio>
#include <cassert>

namespace vm {

void Texture::set_parameter(GLenum parameter, int value) {
    glTextureParameteri(id(), parameter, value);
}

void Texture::set_parameter(GLenum parameter, float value) {
    glTextureParameterf(id(), parameter, value);
}

void Texture::generate_mipmaps() {
    glGenerateTextureMipmap(id());
}

void Texture::clear(float r, float g, float b, float a) const {
    const float rgba[] = {
        r, g, b, a
    };
    glClearTexImage(id(), 0, GL_RGBA, GL_FLOAT, rgba);
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
    fprintf(stderr, "Created %dx%dx%d texture %u\n", m_desc.width, m_desc.height,
            m_desc.depth, m_id);
}

Texture3d::~Texture3d() {
    glDeleteTextures(1, &m_id);
}

}
