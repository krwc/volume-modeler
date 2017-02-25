#ifndef VM_SCENE_SCENE_H
#define VM_SCENE_SCENE_H

namespace vm {
class Brush;

class Scene {
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
};

} // namespace vm

#endif /* VM_SCENE_SCENE_H */
