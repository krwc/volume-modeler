#include <GLFW/glfw3.h>
#include <stdexcept>

#include "scene/scene.h"
#include "scene/brush.h"

using namespace std;

static const int g_initial_window_width = 1400;
static const int g_initial_window_height = 900;
static GLFWwindow *g_window;
static vm::Scene g_scene;

static void init() {
    if (!glfwInit()) {
        throw runtime_error("cannot initialize glfw");
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    g_window = glfwCreateWindow(g_initial_window_width, g_initial_window_height,
                                "volume-modeler", nullptr, nullptr);
    if (!g_window) {
        throw runtime_error("cannot create window");
    }
    glfwMakeContextCurrent(g_window);
}

static void deinit() {
    glfwDestroyWindow(g_window);
}

static void handle_events() {
    return;
}

static void render_scene() {
    g_scene.render();
}

int main() {
    init();
    while (!glfwWindowShouldClose(g_window)) {
        handle_events();
        render_scene();
        glfwSwapBuffers(g_window);
        glfwPollEvents();
    }
    deinit();
    return 0;
}
