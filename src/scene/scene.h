#ifndef VM_SCENE_SCENE_H
#define VM_SCENE_SCENE_H
#include <memory>

#include "scene/camera.h"
#include "scene/brush.h"

namespace vm {

class Scene {
    std::shared_ptr<Camera> m_camera;

public:
    Scene();
    /**
     * Samples @p brush over the scene adding its volume to it.
     */
    void add(const Brush &brush);

    /**
     * Samples @p brush over the scene subtracting its volume from it.
     */
    void sub(const Brush &brush);

    /**
     * Sets the current camera.
     */
    inline void set_camera(const std::shared_ptr<Camera> &camera) {
        m_camera = camera;
    }

    /** @returns the current camera. */
    inline const std::shared_ptr<Camera> get_camera() const {
        return m_camera;
    }
};

} // namespace vm

#endif /* VM_SCENE_SCENE_H */
