#include <GLFW/glfw3.h>
#include <stdexcept>
#include <memory>
#include <chrono>

#include "gfx/renderer.h"

#include "scene/scene.h"
#include "scene/brush-cube.h"
#include "scene/brush-ball.h"

using namespace std;
using namespace glm;

static double g_dt = 1.0;
static double g_acceleration = 1.0;
static int g_window_width = 1400;
static int g_window_height = 900;
static GLFWwindow *g_window;
static shared_ptr<vm::Camera> g_camera;
static unique_ptr<vm::Scene> g_scene;
static unique_ptr<vm::Renderer> g_renderer;

static chrono::time_point<chrono::steady_clock> g_frametime_beg;
static chrono::time_point<chrono::steady_clock> g_frametime_end;

static unique_ptr<vm::Brush> g_brushes[] = {
    make_unique<vm::BrushCube>(),
    make_unique<vm::BrushBall>()
};

static int g_selected_brush;

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
    glfwSwapInterval(0);
    g_camera = make_shared<vm::Camera>();
    g_camera->set_origin({0,0,5});
    g_camera->set_aspect_ratio(float(g_window_width) / g_window_height);

    g_scene = make_unique<vm::Scene>();
    g_scene->set_camera(g_camera);
    vm::BrushCube cube{};
    cube.set_scale({1, 1, 1});
    cube.set_rotation(radians(vec3(45, 0, 0)));
    g_scene->add(cube);
    cube.set_rotation(radians(vec3(0, 45, 0)));
    g_scene->add(cube);
    cube.set_rotation(radians(vec3(0, 0, 45)));
    g_scene->add(cube);

    vm::BrushBall ball{};
    ball.set_origin({0.5, 0.5, 0.5});
    g_scene->sub(ball);

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
        g_camera->set_aspect_ratio(float(g_window_width) / g_window_height);
    }
}

static void handle_keyboard() {
    static const vec3 AXIS_X(1,0,0);
    static const vec3 AXIS_Z(0,0,-1);

    float accel = 0.025f * g_acceleration;

    if (glfwGetKey(g_window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
        accel *= 10;
    }

    vec3 translation;
    mat3 inv_rotation = inverse(g_camera->get_rotation());
    if (glfwGetKey(g_window, GLFW_KEY_W) == GLFW_PRESS) {
        translation += AXIS_Z;
    }
    if (glfwGetKey(g_window, GLFW_KEY_S) == GLFW_PRESS) {
        translation -= AXIS_Z;
    }
    if (glfwGetKey(g_window, GLFW_KEY_A) == GLFW_PRESS) {
        translation -= AXIS_X;
    }
    if (glfwGetKey(g_window, GLFW_KEY_D) == GLFW_PRESS) {
        translation += AXIS_X;
    }

    g_camera->set_origin(g_camera->get_origin() + accel * inv_rotation * translation);
}

static void handle_mouse() {
    static double last_x;
    static double last_y;
    static double rot_x;
    static double rot_y;
    double mx, my;

    glfwGetCursorPos(g_window, &mx, &my);
    double dx = last_x - mx;
    double dy = last_y - my;
    last_x = mx;
    last_y = my;

    if (glfwGetMouseButton(g_window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
        rot_x -= radians(0.1 * dy);
        rot_y -= radians(0.1 * dx);
    }

    g_camera->set_rotation_x(rot_x);
    g_camera->set_rotation_y(rot_y);
}

static void handle_events() {
    handle_keyboard();
    handle_mouse();
}

static void render_scene() {
    g_renderer->render(*g_scene.get());
}

static void report_frametime() {
    static unsigned frame_counter = 0;
    static double frametime_sum = 0;
    ++frame_counter;
    frametime_sum += g_dt;

    if (frame_counter == 600) {
        fprintf(stderr, "frametime=%.3fms; avg=%.3fms\n", g_dt,
                frametime_sum / frame_counter);
        frametime_sum = 0;
        frame_counter = 0;
    }
}

int main() {
    init();
    while (!glfwWindowShouldClose(g_window)) {
        using namespace chrono;
        g_dt = duration_cast<microseconds>(g_frametime_end - g_frametime_beg).count() / 1000.0;
        g_acceleration = g_dt / 16.0;
        report_frametime();
        g_frametime_beg = steady_clock::now();
        glfwPollEvents();
        handle_resize();
        handle_events();
        render_scene();
        glfwSwapBuffers(g_window);
        g_frametime_end = steady_clock::now();
    }
    deinit();
    return 0;
}
