#ifndef VM_SCENE_BRUSH_BALL_H
#define VM_SCENE_BRUSH_BALL_H
#include "brush.h"

#include <math/aabb.h>

namespace vm {

class BrushBall : public Brush {
public:
    virtual math::AABB get_aabb() const {
        return math::AABB(get_origin() - 0.5f * get_scale(),
                          get_origin() + 0.5f * get_scale());
    }

    virtual int id() const {
        return Brush::Id::Ball;
    }
};

} // namespace vm

#endif /* VM_SCENE_BRUSH_BALL_H */
