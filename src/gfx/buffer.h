#ifndef VM_GFX_BUFFER_H
#define VM_GFX_BUFFER_H

#include <GL/glew.h>

#include <boost/optional.hpp>

namespace vm {

struct BufferDesc {
    GLenum type;
    GLenum hint;
    const void *initial_data;
    size_t initial_size;
};

class Buffer {
    boost::optional<BufferDesc> m_desc;
    GLuint m_id;

public:
    Buffer &operator=(const Buffer &) = delete;
    Buffer(const Buffer &) = delete;

    Buffer &operator=(Buffer &&);
    Buffer(Buffer &&);

    /**
     * Creates buffer of specified @p desc#type and initializes it with @p
     * desc#initial_data.
     *
     * @param desc#type          Type of the buffer.
     * @param desc#hint          Usage hint for the driver.
     * @param desc#initial_data  Pointer to the data to write into the buffer or nullptr.
     * @param desc#initial_size  Size of the data to write. MUST NOT be 0.
     */
    Buffer(const BufferDesc &desc);

    /**
     * Deletes buffer from the GPU memory via @ref Buffer#Delete.
     */
    ~Buffer();

    /**
     * Writes data to the buffer.
     *
     * If the buffer is too small to hold @p size bytes it is trashed and resized.
     * Otherwise only @p size bytes are updated and the remaining bytes are untouched.
     *
     * @param data  Data to write. MUST NOT be nullptr.
     * @param size  Size of the data. MUST NOT be 0.
     */
    void fill(const void *data, size_t size);

    /**
     * Updates part of the contents of the buffer. Note that @ref size + @ref offset
     * can AT MOST be equal to the size of the buffer, or otherwise an exception will
     * be thrown.
     *
     * @param data      Data to write. MUST NOT be nullptr.
     * @param size      Size of the data. MUST NOT be 0.
     * @param offset    Offset where writing should start.
     */
    void update(const void *data, size_t size, size_t offset = 0);

    /**
     * @returns underlying buffer type
     */
    inline GLenum type() const {
        return m_desc.get().type;
    }

    /**
     * @returns underlying handle
     */
    inline GLuint id() const {
        return m_id;
    }

    /**
     * @returns current buffer size in bytes
     */
    inline size_t size() const {
        return m_desc.get().initial_size;
    }
};

} // namespace vm

#endif /* VM_GFX_BUFFER_H */
