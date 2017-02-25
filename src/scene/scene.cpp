#include "config.h"

#include "scene.h"
#include "brush.h"

namespace vm {

Scene::Scene()
    : m_camera()
    , m_sampler()
    , m_raymarcher()
{ }

void Scene::add(const Brush &brush) {
    return;
}

void Scene::sub(const Brush &brush) {
    return;
}

void Scene::draw() {
    return;
}

} // namespace vm
