#ifndef VM_COMPUTE_INTEROP_H
#define VM_COMPUTE_INTEROP_H
#include <glm/glm.hpp>

#include "compute/context.h"

namespace vm {

struct Vec3Repr {
    cl_float3 row;

    Vec3Repr(const glm::vec3 &v) : row({ v.x, v.y, v.z }) {}
    Vec3Repr() : Vec3Repr(glm::vec3(0, 0, 0)) {}
};

struct Vec4Repr {
    cl_float4 row;

    Vec4Repr(const glm::vec4 &v) : row({ v.x, v.y, v.z, v.w }) {}
    Vec4Repr() : Vec4Repr(glm::vec4(0, 0, 0, 0)) {}
};

struct Mat3Repr {
    Vec3Repr row0;
    Vec3Repr row1;
    Vec3Repr row2;

    Mat3Repr(const glm::mat3 &m)
            : row0({ m[0].x, m[1].x, m[2].x })
            , row1({ m[0].y, m[1].y, m[2].y })
            , row2({ m[0].z, m[1].z, m[2].z }) {}

    Mat3Repr() : Mat3Repr(glm::mat3(0.0f)) {}
};

struct Mat4Repr {
    Vec4Repr row0;
    Vec4Repr row1;
    Vec4Repr row2;
    Vec4Repr row3;

    Mat4Repr(const glm::mat4 &m)
            : row0({ m[0].x, m[1].x, m[2].x, m[3].x })
            , row1({ m[0].y, m[1].y, m[2].y, m[3].y })
            , row2({ m[0].z, m[1].z, m[2].z, m[3].z })
            , row3({ m[0].w, m[1].w, m[2].w, m[3].w }) {}

    Mat4Repr() : Mat4Repr(glm::mat4(0.0f)) {}
};

} // namespace vm

namespace boost {
namespace compute {
namespace detail {

template <>
struct set_kernel_arg<int> {
    void operator()(kernel &kernel_, size_t index, const int v) {
        cl_int value = v;
        kernel_.set_arg(index, sizeof(value), &value);
    }
};

template <>
struct set_kernel_arg<glm::vec3> {
    void operator()(kernel &kernel_, size_t index, const glm::vec3 &v) {
        vm::Vec3Repr value(v);
        kernel_.set_arg(index, sizeof(value), &value);
    }
};

template <>
struct set_kernel_arg<glm::vec4> {
    void operator()(kernel &kernel_, size_t index, const glm::vec4 &v) {
        vm::Vec4Repr value(v);
        kernel_.set_arg(index, sizeof(value), &value);
    }
};

template <>
struct set_kernel_arg<glm::mat3> {
    void operator()(kernel &kernel_, size_t index, const glm::mat3 &m) {
        vm::Mat3Repr value(m);
        kernel_.set_arg(index, sizeof(value), &value);
    }
};

template <>
struct set_kernel_arg<glm::mat4> {
    void operator()(kernel &kernel_, size_t index, const glm::mat4 &m) {
        vm::Mat4Repr value(m);
        kernel_.set_arg(index, sizeof(value), &value);
    }
};

} // namespace detail
} // namespace compute
} // namespace boost

#endif
