#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <stdexcept>
#include <memory>
#include <chrono>
#include <fstream>
#include <thread>

#include "gfx/renderer.h"
#include "compute/context.h"

#include "scene/scene.h"
#include "scene/brush-cube.h"
#include "scene/brush-ball.h"

#include "utils/log.h"

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
static shared_ptr<vm::ComputeContext> g_compute_ctx;

static chrono::time_point<chrono::steady_clock> g_frametime_beg;
static chrono::time_point<chrono::steady_clock> g_frametime_end;

static unique_ptr<vm::Brush> g_brushes[] = {
    make_unique<vm::BrushCube>(),
    make_unique<vm::BrushBall>()
};
static int g_brush_id;
static vec3 g_brush_scale(1,1,1);

static int g_material_id;
static const vector<string> g_material_array {
    "media/textures/brick.jpg",
    "media/textures/brick1.jpg",
    "media/textures/grass.jpg",
    "media/textures/rock.jpg",
    "media/textures/stones.jpg",
    "media/textures/wood.jpg"
};

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

    g_compute_ctx = vm::make_gl_shared_compute_context();

    g_renderer = make_unique<vm::Renderer>(g_compute_ctx, g_material_array);
    g_renderer->resize(g_window_width, g_window_height);
    glfwSetScrollCallback(g_window, handle_scroll);
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
    if (glfwGetKey(g_window, GLFW_KEY_1) == GLFW_PRESS) {
        g_brush_id = 0;
    }
    if (glfwGetKey(g_window, GLFW_KEY_2) == GLFW_PRESS) {
        g_brush_id = 1;
    }
    if (glfwGetKey(g_window, GLFW_KEY_F1) == GLFW_PRESS) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }
    if (glfwGetKey(g_window, GLFW_KEY_F2) == GLFW_PRESS) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }


    g_camera->set_origin(g_camera->get_origin() + accel * inv_rotation * translation);
}

static void handle_scroll(GLFWwindow *window, double xoffset, double yoffset) {
    (void) window;
    (void) xoffset;
    if (glfwGetKey(g_window, GLFW_KEY_X)) {
        g_brush_scale.x += 2*VM_VOXEL_SIZE * yoffset;
    } else if (glfwGetKey(g_window, GLFW_KEY_Y)) {
        g_brush_scale.y += 2*VM_VOXEL_SIZE * yoffset;
    } else if (glfwGetKey(g_window, GLFW_KEY_Z)) {
        g_brush_scale.z += 2*VM_VOXEL_SIZE * yoffset;
    } else {
        g_material_id += yoffset;
        g_material_id = std::max(0, std::min(g_material_id, int(g_material_array.size())-1));
        get_current_brush()->set_material(g_material_id);
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
    const vec3 origin =
            g_camera->get_origin()
            - dir * vec3(0, 0, std::max(2.0f, 1.0f + brush->get_scale().z));
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

    if (frametime_sum >= 1000) {
        LOG(debug) << "frametime=" << g_dt << "ms; "
                   << "avg=" << (frametime_sum / frame_counter) << "ms";
        frametime_sum = 0;
        frame_counter = 0;
    }
}

int main(int argc, char **argv) {
    if (argc > 2) {
        LOG(error) << "Usage: " << argv[0] << " [scene-persistence-dir]";
        return 1;
    }
    setenv("CUDA_CACHE_DISABLE", "1", 1);
    init();

    string scene_persistence_dir = "scene";
    if (argc == 2) {
        scene_persistence_dir = argv[1];
    }
    g_scene = make_unique<vm::Scene>(g_compute_ctx, g_camera,
                                     scene_persistence_dir);
    vm::BrushCube cube;
    cube.set_rotation(glm::radians(glm::vec3{45.0f, 0.0f, 0.0f}));
    g_scene->add(cube);
    cube.set_rotation(glm::radians(glm::vec3{0.0f, 45.0f, 0.0f}));
    g_scene->add(cube);
    cube.set_rotation(glm::radians(glm::vec3{0.0f, 0.0f, 45.0f}));
    g_scene->add(cube);
#if 0
    const int size = 20;
    for (int z = -size/2; z < size/2; ++z) {
        for (int x = -size/2; x < size/2; ++x) {
            const glm::vec3 origin(x, -2, z);
            cube.set_origin(origin);
            g_scene->add(cube);
        }
    }
#endif

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
    return 0;
}
