#ifndef VM_SCENE_CAMERA_H
#define VM_SCENE_CAMERA_H
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace vm {

class Camera {
    glm::mat4 m_view;
    glm::mat4 m_proj;
    glm::vec3 m_origin;
    float m_rotx;
    float m_roty;
    float m_fov;
    float m_aspect_ratio;
    float m_near_plane;
    float m_far_plane;

    void recalculate_proj();
    void recalculate_view();

    glm::quat get_quat_rotation_x() const;
    glm::quat get_quat_rotation_y() const;

public:
    Camera(const glm::vec3 &origin = { 0, 0, 0 },
           float fov = 70.0f,
           float aspect_ratio = 4.0 / 3.0,
           float near_plane = 0.1f,
           float far_plane = 1000.0f);

    /** @brief sets camera aspect ratio to match view area aspect ratio */
    void set_aspect_ratio(float aspect_ratio);

    /** @brief sets camera viewing frustum near plane */
    void set_near_plane(float near_plane);

    /** @brief sets camera viewing frustum far plane */
    void set_far_plane(float far_plane);

    /** @brief sets camera fov (in radians) */
    void set_fov(float radians);

    /** @brief sets camera origin */
    void set_origin(const glm::vec3 &origin);

    /** @brief sets rotation around x axis */
    void set_rotation_x(float radians);

    /** @brief sets rotation around y axis */
    void set_rotation_y(float radians);

    inline const glm::mat4 &get_view() const {
        return m_view;
    }

    inline const glm::mat4 &get_proj() const {
        return m_proj;
    }

    inline glm::mat3 get_rotation() const {
        return glm::mat3_cast(get_quat_rotation_x() * get_quat_rotation_y());
    }

    inline float get_rotation_x() const {
        return m_rotx;
    }

    inline float get_rotation_y() const {
        return m_roty;
    }

    inline float get_fov() const {
        return m_fov;
    }

    inline float get_near_plane() const {
        return m_near_plane;
    }

    inline float get_far_plane() const {
        return m_far_plane;
    }

    inline float get_aspect_ratio() const {
        return m_aspect_ratio;
    }

    inline const glm::vec3 &get_origin() const {
        return m_origin;
    }

    friend std::istream &operator>>(std::istream &in, Camera &camera);
    friend std::ostream &operator<<(std::ostream &out, const Camera &camera);
};

} // namespace vm

#endif /* VM_SCENE_CAMERA_H */
