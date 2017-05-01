#ifndef VM_SCENE_BRUSH_BALL_H
#define VM_SCENE_BRUSH_BALL_H
#include "scene/brush.h"

namespace vm {

class BrushBall : public Brush {
public:
    virtual AABB get_aabb() const {
        return AABB(get_origin() - 0.5f * get_scale(),
                    get_origin() + 0.5f * get_scale());
    }

    virtual int id() const {
        return Brush::Id::Ball;
    }
};

} // namespace vm

#endif /* VM_SCENE_BRUSH_BALL_H */
