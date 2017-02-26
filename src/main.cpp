#include <GLFW/glfw3.h>
#include <stdexcept>
#include <memory>

#include "gfx/renderer.h"

#include "scene/scene.h"
#include "scene/brush-cube.h"
#include "scene/brush-ball.h"

using namespace std;

static int g_window_width = 1400;
static int g_window_height = 900;
static GLFWwindow *g_window;
static shared_ptr<vm::Camera> g_camera;
static unique_ptr<vm::Scene> g_scene;
static unique_ptr<vm::Renderer> g_renderer;

static void init() {
    if (!glfwInit()) {
        throw runtime_error("cannot initialize glfw");
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    g_window = glfwCreateWindow(g_window_width, g_window_height,
                                "volume-modeler", nullptr, nullptr);
    if (!g_window) {
        throw runtime_error("cannot create window");
    }
    glfwMakeContextCurrent(g_window);
    g_camera = make_shared<vm::Camera>();
    g_camera->set_origin({0,0,5});
    g_scene = make_unique<vm::Scene>();
    g_scene->set_camera(g_camera);
    vm::BrushCube cube{};
    cube.set_scale({0.4, 0.4, 0.4});
    cube.set_rotation({45, 45, 45});
    g_scene->add(cube);

    g_renderer = make_unique<vm::Renderer>();
    g_renderer->resize(g_window_width, g_window_height);
}

static void deinit() {
    glfwDestroyWindow(g_window);
}

static void handle_resize() {
    int current_w, current_h;
    glfwGetFramebufferSize(g_window, &current_w, &current_h);

    if (current_w != g_window_width || current_h != g_window_height) {
        g_window_width = current_w;
        g_window_height = current_h;
        g_renderer->resize(g_window_width, g_window_height);
    }
}

static void handle_events() {
    return;
}

static void render_scene() {
    g_renderer->render(*g_scene.get());
}

int main() {
    init();
    while (!glfwWindowShouldClose(g_window)) {
        handle_resize();
        handle_events();
        render_scene();
        glfwSwapBuffers(g_window);
        glfwPollEvents();
    }
    deinit();
    return 0;
}
