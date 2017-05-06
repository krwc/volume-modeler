#ifndef VM_SCENE_CHUNK_H
#define VM_SCENE_CHUNK_H
#include <glm/glm.hpp>

#include "gfx/compute-context.h"

#include <boost/optional.hpp>
#include <mutex>

namespace vm {

class Chunk {
    compute::image3d m_volume;
    std::mutex m_mutex;
    glm::ivec3 m_coord;
    int m_lod;

public:
    /**
     * Constructs an empty chunk of the scene, with no volumetric data
     * associated - it may be loaded later on.
     */
#define _N (VM_CHUNK_SIZE + VM_CHUNK_BORDER)
    Chunk(const glm::ivec3 &coord,
          const compute::context &context,
          const compute::image_format &volume_format,
          int lod = 0)
            : m_volume(context, _N, _N, _N, volume_format),
              m_mutex(),
              m_coord(coord),
              m_lod(lod) {}
#undef _N

    inline compute::image3d &get_volume() {
        return m_volume;
    }

    inline const compute::image3d &get_volume() const {
        return m_volume;
    }

    inline const glm::ivec3 &get_coord() const {
        return m_coord;
    }

    inline std::mutex &get_mutex() {
        return m_mutex;
    }

    inline int get_lod() {
        return m_lod;
    }
};

} // namespace vm

#endif /* VM_SCENE_CHUNK_H */
