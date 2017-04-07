#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <stdexcept>
#include <memory>
#include <chrono>
#include <fstream>

#include "gfx/renderer.h"

#include "scene/scene.h"
#include "scene/brush-cube.h"
#include "scene/brush-ball.h"

using namespace std;
using namespace glm;

static bool g_mouse_grabbed;
static bool g_brush_should_rotate;

static double g_rotx;
static double g_roty;

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
static int g_brush_id;
static vec3 g_brush_scale(1,1,1);

static void handle_scroll(GLFWwindow *window, double xoffset, double yoffset);

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
    if (glewInit() != GLEW_OK) {
        throw runtime_error("cannot initialize glew");
    }
    g_camera = make_shared<vm::Camera>();
    g_camera->set_origin({0,0,5});
    g_camera->set_aspect_ratio(float(g_window_width) / g_window_height);

    g_scene = make_unique<vm::Scene>();
    g_scene->set_camera(g_camera);

    g_renderer = make_unique<vm::Renderer>();
    g_renderer->resize(g_window_width, g_window_height);
    glfwSetScrollCallback(g_window, handle_scroll);
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

vm::Brush *get_current_brush() {
    return g_brushes[g_brush_id].get();
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
    if (glfwGetKey(g_window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        g_mouse_grabbed = false;
        glfwSetInputMode(g_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
    if (glfwGetKey(g_window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS) {
        g_brush_should_rotate = true;
    } else {
        g_brush_should_rotate = false;
    }
    if (glfwGetKey(g_window, GLFW_KEY_F1) == GLFW_PRESS) {
        ofstream persistence("scene.dat", ios_base::binary | ios_base::trunc);
        persistence << *g_scene.get();
        cerr << "Outputed scene to scene.dat" << endl;
    }
    if (glfwGetKey(g_window, GLFW_KEY_1) == GLFW_PRESS) {
        g_brush_id = 0;
    }
    if (glfwGetKey(g_window, GLFW_KEY_2) == GLFW_PRESS) {
        g_brush_id = 1;
    }

    g_camera->set_origin(g_camera->get_origin() + accel * inv_rotation * translation);
}

static void handle_scroll(GLFWwindow *window, double xoffset, double yoffset) {
    (void) window;
    (void) xoffset;
    if (glfwGetKey(g_window, GLFW_KEY_X)) {
        g_brush_scale.x += 2*VM_VOXEL_SIZE * yoffset;
    }
    if (glfwGetKey(g_window, GLFW_KEY_Y)) {
        g_brush_scale.y += 2*VM_VOXEL_SIZE * yoffset;
    }
    if (glfwGetKey(g_window, GLFW_KEY_Z)) {
        g_brush_scale.z += 2*VM_VOXEL_SIZE * yoffset;
    }
    const float min_scale = 3.5 * VM_VOXEL_SIZE;
    g_brush_scale = max(vec3(min_scale, min_scale, min_scale), g_brush_scale);
}

static void handle_mouse() {
    static double last_x;
    static double last_y;
    static bool brush_lock;
    double mx, my;

    glfwGetCursorPos(g_window, &mx, &my);
    double dx = last_x - mx;
    double dy = last_y - my;
    last_x = mx;
    last_y = my;

    int left_button = glfwGetMouseButton(g_window, GLFW_MOUSE_BUTTON_LEFT);
    int right_button = glfwGetMouseButton(g_window, GLFW_MOUSE_BUTTON_RIGHT);
    if (left_button == GLFW_PRESS) {
        if (!g_mouse_grabbed) {
            brush_lock = true;
        }
        glfwSetInputMode(g_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        g_mouse_grabbed = true;
    } else if (left_button == GLFW_RELEASE) {
        brush_lock = false;
    }

    if (g_mouse_grabbed) {
        g_rotx -= radians(0.1 * dy);
        g_roty -= radians(0.1 * dx);
        g_camera->set_rotation_x(g_rotx);
        g_camera->set_rotation_y(g_roty);
    }

    if (g_mouse_grabbed && !brush_lock && left_button == GLFW_PRESS) {
        g_scene->add(*get_current_brush());
    }
    if (g_mouse_grabbed && !brush_lock && right_button == GLFW_PRESS) {
        g_scene->sub(*get_current_brush());
    }

}

static void handle_events() {
    handle_keyboard();
    handle_mouse();

    vm::Brush *brush = get_current_brush();
    const mat3 dir = inverse(mat3(g_camera->get_view()));
    const vec3 origin = g_camera->get_origin() - dir * vec3(0, 0, 3.0f);
    brush->set_origin(origin);
}

static void render_scene() {
    static const vec4 brush_color = vec4(0.3f, 0.1f, 0.8f, 0.5f);

    g_renderer->render(*g_scene.get());

    vm::Brush *brush = get_current_brush();
    brush->set_rotation({0, 0, 0});
    brush->set_scale(g_brush_scale);

    vm::AABB brush_aabb = brush->get_aabb();

    if (g_brush_should_rotate) {
        brush->set_rotation({g_rotx, g_roty, 0});
    }
    const mat3 brush_transform = g_brush_should_rotate
                                         ? inverse(mat3(g_camera->get_view()))
                                         : mat3(1.0f);
    const vm::Box brush_box = {
        brush_aabb.min,
        brush_aabb.max,
        brush_color,
        brush_transform
    };
    g_renderer->render(g_camera, brush_box);
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

int main(int argc, char **argv) {
    if (argc > 2) {
        cerr << "Usage: " << argv[0] << " [scene-to-restore.dat]" << endl;
        return 1;
    }
    init();

    if (argc == 2) {
        ifstream persistence(argv[1], ios::binary);
        persistence >> *g_scene.get();
        g_rotx = g_camera->get_rotation_x();
        g_roty = g_camera->get_rotation_y();
    }

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
