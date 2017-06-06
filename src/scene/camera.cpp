#include "camera.h"

#include "utils/persistence.h"

#include <glm/gtx/transform.hpp>
#include <glm/gtx/projection.hpp>

namespace vm {
using namespace std;
using namespace glm;

namespace {
const vec3 CAMERA_Y = { 0, 1, 0 };
const vec3 CAMERA_Z = { 0, 0, -1 };
const vec3 CAMERA_X = cross(CAMERA_Z, CAMERA_Y);
}

Camera::Camera(const glm::vec3 &origin,
               float fov,
               float aspect_ratio,
               float near_plane,
               float far_plane)
    : m_view()
    , m_proj()
    , m_origin(origin)
    , m_rotx(0.0f)
    , m_roty(0.0f)
    , m_fov(fov)
    , m_aspect_ratio(aspect_ratio)
    , m_near_plane(near_plane)
    , m_far_plane(far_plane) {
    recalculate_proj();
    recalculate_view();
}

void Camera::recalculate_proj() {
    m_proj = perspective(m_fov, m_aspect_ratio, m_near_plane, m_far_plane);
}

void Camera::recalculate_view() {
    const mat4 T = translate(mat4(), -m_origin);
    const mat4 R = mat4_cast(get_quat_rotation_x() * get_quat_rotation_y());
    m_view = R * T;
}

quat Camera::get_quat_rotation_x() const {
    return rotate(quat(), m_rotx, CAMERA_X);
}

quat Camera::get_quat_rotation_y() const {
    return rotate(quat(), m_roty, CAMERA_Y);
}

void Camera::set_aspect_ratio(float aspect_ratio) {
    m_aspect_ratio = aspect_ratio;
    recalculate_proj();
}

void Camera::set_near_plane(float near_plane) {
    m_near_plane = near_plane;
    recalculate_proj();
}

void Camera::set_far_plane(float far_plane) {
    m_far_plane = far_plane;
    recalculate_proj();
}

void Camera::set_fov(float fov) {
    m_fov = fov;
    recalculate_proj();
}

void Camera::set_origin(const vec3 &origin) {
    m_origin = origin;
    recalculate_view();
}

void Camera::set_rotation_x(float radians) {
    m_rotx = radians;
    recalculate_view();
}

void Camera::set_rotation_y(float radians) {
    m_roty = radians;
    recalculate_view();
}

istream &operator>>(istream &in, Camera &camera) {
    // clang-format off
    in >= camera.m_origin
       >= camera.m_rotx
       >= camera.m_roty
       >= camera.m_fov
       >= camera.m_aspect_ratio
       >= camera.m_near_plane
       >= camera.m_far_plane;
    // clang-format on
    camera.recalculate_view();
    camera.recalculate_proj();
    return in;
}

ostream &operator<<(ostream &out, const Camera &camera) {
    // clang-format off
    out <= camera.m_origin
        <= camera.m_rotx
        <= camera.m_roty
        <= camera.m_fov
        <= camera.m_aspect_ratio
        <= camera.m_near_plane
        <= camera.m_far_plane;
    // clang-format on
    return out;
}

} // namespace vm
