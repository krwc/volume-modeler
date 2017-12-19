#ifndef VM_SCENE_BRUSH_CUBE_H
#define VM_SCENE_BRUSH_CUBE_H
#include "brush.h"

#include <math/aabb.h>

namespace vm {

class BrushCube : public Brush {
public:
    virtual math::AABB get_aabb() const {
        using namespace glm;
        const vec3 min = -0.5f * get_scale();
        const vec3 max = +0.5f * get_scale();
        const math::AABB aabb = math::AABB(min, max).transform(get_rotation());
        return math::AABB(aabb.min + get_origin(), aabb.max + get_origin());
    }

    virtual int id() const {
        return Brush::Id::Cube;
    }
};

} // namespace vm

#endif /* VM_SCENE_BRUSH_CUBE_H */
