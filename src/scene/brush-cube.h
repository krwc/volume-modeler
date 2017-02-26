#ifndef VM_SCENE_BRUSH_CUBE_H
#define VM_SCENE_BRUSH_CUBE_H
#include "scene/brush.h"

namespace vm {

class BrushCube : public Brush {
public:
    virtual AABB get_aabb() const {
        using namespace glm;
        const vec3 min = -0.5f * get_scale();
        const vec3 max = +0.5f * get_scale();
        const AABB aabb = AABB(min, max).transform(get_rotation());
        return AABB(aabb.min + get_origin(), aabb.max + get_origin());
    }
};

} // namespace vm

#endif /* VM_SCENE_BRUSH_CUBE_H */
