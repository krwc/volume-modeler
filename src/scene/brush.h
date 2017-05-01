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
    Brush()
        : m_origin()
        , m_rotation(1.0f)
        , m_scale(1, 1, 1)
        , m_material(0)
    {}

    virtual ~Brush() {}

    /**
     * @returns the bounding box of the brush. Must be implemented by the
     * deriving classes.
     */
    virtual AABB get_aabb() const = 0;

    /** @returns the origin of the brush */
    inline const glm::vec3 &get_origin() const {
        return m_origin;
    }

    /** @returns the vector of rotations in each dimension (xyz) in radians */
    inline const glm::mat3 &get_rotation() const {
        return m_rotation;
    }

    /** @returns the vector of scales in each dimension (xyz) */
    inline const glm::vec3 &get_scale() const {
        return m_scale;
    }

    /** @brief sets the origin of the brush */
    inline void set_origin(const glm::vec3 &origin) {
        m_origin = origin;
    }

    /** @brief sets the rotation (in radians) in each dimension (xyz) */
    inline void set_rotation(const glm::vec3 &rotation) {
        const glm::quat rotx = glm::rotate(glm::quat(), rotation.x, { 1.0f, 0.0f, 0.0f });
        const glm::quat roty = glm::rotate(glm::quat(), rotation.y, { 0.0f, 1.0f, 0.0f });
        const glm::quat rotz = glm::rotate(glm::quat(), rotation.z, { 0.0f, 0.0f, 1.0f });
        m_rotation = mat3_cast(rotx * roty * rotz);
    }

    /** @brief sets the scale in each dimension (xyz). By default the scale is 1 */
    inline void set_scale(const glm::vec3 &scale) {
        m_scale = scale;
    }

    inline void set_material(int material) {
        m_material = material;
    }

    /** Obtains an unique identifier of the brush (to pass for the shader) */
    virtual int id() const = 0;

    /** Obtains material used for this brush */
    inline int material() const {
        return m_material;
    }

private:
    glm::vec3 m_origin;
    glm::mat3 m_rotation;
    glm::vec3 m_scale;
    int m_material;
};

} // namespace vm

#endif /* VM_SCENE_BRUSH_H */
