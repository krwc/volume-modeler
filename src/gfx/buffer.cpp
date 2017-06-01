#include "buffer.h"

#include <cassert>
#include <stdexcept>

namespace vm {
using namespace std;

Buffer &Buffer::operator=(Buffer &&other) {
    if (&other != this) {
        this->~Buffer();
        m_desc = std::move(other.m_desc);
        m_id = other.m_id;
        other.m_id = GL_NONE;
    }
    return *this;
}

Buffer::Buffer() : m_id(GL_NONE) {}

Buffer::Buffer(Buffer &&other)
    : m_id(GL_NONE) {
    *this = move(other);
}

Buffer::Buffer(const BufferDesc &desc) {
    m_desc = desc;
    glCreateBuffers(1, &m_id);
    glNamedBufferData(m_id, desc.initial_size, desc.initial_data, desc.hint);
}

Buffer::~Buffer() {
    if (m_id != GL_NONE) {
        glDeleteBuffers(1, &m_id);
    }
}

void Buffer::fill(const void *data, size_t size) {
    if (size == 0u) {
        throw invalid_argument("unexpected size 0");
    }
    if (size > m_desc.get().initial_size) {
        glNamedBufferData(m_id, size, data, m_desc.get().hint);
        m_desc.get().initial_size = size;
    } else {
        update(data, size, 0u);
    }
}

void Buffer::update(const void *data, size_t size, size_t offset) {
    if (!data) {
        throw invalid_argument("data cannot be nullptr");
    }
    if (offset + size > m_desc.get().initial_size) {
        throw invalid_argument("data size exceeds buffer capacity");
    }
    glNamedBufferSubData(m_id, offset, size, data);
}

} // namespace vm
