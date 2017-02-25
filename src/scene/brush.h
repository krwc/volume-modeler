#ifndef VM_SCENE_BRUSH_H
#define VM_SCENE_BRUSH_H

#include <glm/glm.hpp>

#include "math/aabb.h"

namespace vm {

/**
 * @brief An abstract class representing a brush that can be used to draw on the
 * scene.
 */
class Brush {
public:
    enum Type {
        Cube = 0,
        Ball = 1
    };

    Brush(Brush::Type type)
        : m_position()
        , m_rotation()
        , m_scale(1, 1, 1)
        , m_type(type)
    {}

    /**
     * @returns the bounding box of the brush. Must be implemented by the
     * deriving classes.
     */
    virtual const AABB &get_aabb() const = 0;

    /** @returns the type of this brush */
    inline Brush::Type get_type() const {
        return m_type;
    }

    /** @returns the origin of the brush */
    inline const glm::vec3 &get_position() const {
        return m_position;
    }

    /** @returns the vector of rotations in each dimension (xyz) in radians */
    inline const glm::vec3 &get_rotation() const {
        return m_rotation;
    }

    /** @returns the vector of scales in each dimension (xyz) */
    inline const glm::vec3 &get_scale() const {
        return m_scale;
    }

    /** @brief sets the origin of the brush */
    inline void set_position(const glm::vec3 &origin) {
        m_position = origin;
    }

    /** @brief sets the rotation (in radians) in each dimension (xyz) */
    inline void set_rotation(const glm::vec3 &rotation) {
        m_rotation = rotation;
    }

    /** @brief sets the scale in each dimension (xyz). By default the scale is 1 */
    inline void set_scale(const glm::vec3 &scale) {
        m_scale = scale;
    }

private:
    glm::vec3 m_position;
    glm::vec3 m_rotation;
    glm::vec3 m_scale;
    Brush::Type m_type;
};

} // namespace vm

#endif /* VM_SCENE_BRUSH_H */
