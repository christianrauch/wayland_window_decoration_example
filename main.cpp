#include <iostream>
#include <wayland-client.h>
#include <wayland-egl.h>
#include <EGL/egl.h>
#include <GL/gl.h>
#include <cstring>
#include <linux/input.h>

#include <vector>

/// Wayland surfaces

#define WIDTH 256
#define HEIGHT 256

static struct wl_display *display;
static struct wl_compositor *compositor = NULL;
static struct wl_subcompositor *subcompositor = NULL;
static struct wl_shell *shell = NULL;
static struct wl_seat *seat = NULL;
static EGLDisplay egl_display;
static char running = 1;

struct decoration;

struct window {
    EGLContext egl_context;
    struct wl_surface *surface;
    struct wl_shell_surface *shell_surface;
    struct wl_egl_window *egl_window;
    EGLSurface egl_surface;

    int width;
    int height;

    std::vector<decoration> decorations;

    bool button_pressed = false;
    wl_surface * current_surface = NULL;    // last entered surface
    wl_fixed_t lastx;
    wl_fixed_t lasty;

    bool inhibit_motion = false;
};

struct decoration {
    struct wl_surface *surface;
    struct wl_subsurface *subsurface;
    struct wl_egl_window *egl_window;
    EGLSurface egl_surface;
    EGLContext egl_context;
    EGLDisplay egl_display;
    int posx, posy;
    int width, height;

    enum Function {
        MOVE,
        RESIZE_LEFT,
        RESIZE_TOP,
        RESIZE_RIGHT,
        RESIZE_BUTTOM,
        RESIZE_UPPERLEFT,
        RESIZE_UPPERRIGHT,
        RESIZE_LOWERRIGHT,
        RESIZE_LOWERLEFT,
    } function;

    decoration(wl_compositor* compositor, wl_subcompositor* subcompositor, wl_surface* source,
              const int x, const int y, const int w, const int h,
              EGLConfig config, Function type)
    {
        function = type;
        posx = x;
        posy = y;
        width = w;
        height = h;

        surface = wl_compositor_create_surface(compositor);
        subsurface = wl_subcompositor_get_subsurface(subcompositor, surface, source);
        wl_subsurface_set_sync(subsurface);
        egl_display = eglGetDisplay(display);
        egl_context = eglCreateContext (egl_display, config, EGL_NO_CONTEXT, NULL);
        eglInitialize(egl_display, NULL, NULL);
        wl_subsurface_set_position(subsurface, x, y);
        egl_window = wl_egl_window_create(surface, w, h);
        egl_surface = eglCreateWindowSurface(egl_display, config, egl_window, NULL);
    }

    ~decoration() {
//        wl_subsurface_destroy(subsurface);
//        wl_subcompositor_destroy(subcompositor);
    }

    void activate() {
        eglMakeCurrent (egl_display, egl_surface, egl_surface, egl_context);
    }

    void swap() {
        eglSwapBuffers(egl_display, egl_surface);
    }

    void reposition(const int dx, const int dy) {
        posx += dx;
        posy += dy;
        wl_subsurface_set_position(subsurface, posx, posy);
        std::cout << "new pos: " << posx << " " << posy << std::endl;
    }
};

// listeners
static void pointer_enter (void *data, struct wl_pointer *pointer, uint32_t serial, struct wl_surface *surface, wl_fixed_t surface_x, wl_fixed_t surface_y) {
    window *w = static_cast<window*>(data);
    w->current_surface = surface;
    w->lastx = surface_x;
    w->lasty = surface_y;
}

static void pointer_leave (void *data, struct wl_pointer *pointer, uint32_t serial, struct wl_surface *surface) {
    window *w = static_cast<window*>(data);
    w->button_pressed = false;
}

static void pointer_motion (void *data, struct wl_pointer *pointer, uint32_t time, wl_fixed_t x, wl_fixed_t y) {
//    std::cout << "pointer motion " << wl_fixed_to_double(x) << " " << wl_fixed_to_double(y) << std::endl;
    const wl_fixed_t curx = x;
    const wl_fixed_t cury = y;
    window *w = static_cast<window*>(data);
    const int dx = wl_fixed_to_int(curx - w->lastx);
    const int dy = wl_fixed_to_int(cury - w->lasty);

    if(dx==0 && dy==0)
        return;

    if(w->button_pressed) {
        if(!w->inhibit_motion) {
            for(int i = 0; i<w->decorations.size(); i++) {
                auto &d = w->decorations[i];
                if(d.surface==w->current_surface) {
                    std::cout << "old: " << w->lastx << " " << w->lasty << std::endl;
                    std::cout << "new: " << curx << " " << cury << std::endl;
                    std::cout << "resizing: " << dx << " " << dy << std::endl;
                    switch(w->decorations[i].function) {
                    case decoration::RESIZE_LEFT:
                        w->width -= dx;
                        d.reposition(dx, 0);
                        wl_egl_window_resize(w->egl_window, w->width, w->height, 0, 0);
                        w->inhibit_motion = true;
                        break;
                    case decoration::RESIZE_TOP: break;
                    case decoration::RESIZE_RIGHT:
                        w->width += dx;
                        d.reposition(dx, 0);
                        wl_egl_window_resize(w->egl_window, w->width, w->height, 0, 0);
                        w->inhibit_motion = true;
                        break;
                    case decoration::RESIZE_BUTTOM: break;
                    case decoration::RESIZE_UPPERLEFT: break;
                    case decoration::RESIZE_UPPERRIGHT: break;
                    case decoration::RESIZE_LOWERRIGHT: break;
                    case decoration::RESIZE_LOWERLEFT: break;
                    }
                }
            }
        } // inhibit
        else {
            std::cout << "ignoring: " << dx << " " << dy << std::endl;
            w->inhibit_motion = false;
        }
    }
    w->lastx = x;
    w->lasty = y;
}

static void pointer_button (void *data, struct wl_pointer *pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t state) {
    std::cout << "pointer button " << button << ", state " << state << std::endl;

    window *w = static_cast<window*>(data);
    w->button_pressed = (button==BTN_LEFT) && (state==WL_POINTER_BUTTON_STATE_PRESSED);

    if(w->button_pressed) {
        for(int i = 0; i<w->decorations.size(); i++) {
            if((w->decorations[i].surface==w->current_surface) &&
               (w->decorations[i].function==decoration::MOVE))
            {
                wl_shell_surface_move(w->shell_surface, seat, serial);
            }
        }
    }
}

static void pointer_axis (void *data, struct wl_pointer *pointer, uint32_t time, uint32_t axis, wl_fixed_t value) {
}

static struct wl_pointer_listener pointer_listener = {&pointer_enter, &pointer_leave, &pointer_motion, &pointer_button, &pointer_axis};

static void seat_capabilities (void *data, struct wl_seat *seat, uint32_t capabilities) {
    if (capabilities & WL_SEAT_CAPABILITY_POINTER) {
        struct wl_pointer *pointer = wl_seat_get_pointer (seat);
        wl_pointer_add_listener (pointer, &pointer_listener, data);
    }
//    if (capabilities & WL_SEAT_CAPABILITY_KEYBOARD) {
//        struct wl_keyboard *keyboard = wl_seat_get_keyboard (seat);
//        wl_keyboard_add_listener (keyboard, &keyboard_listener, NULL);
//    }
}
static struct wl_seat_listener seat_listener = {&seat_capabilities};

static void registry_add_object (void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
    if (!strcmp(interface,"wl_compositor")) {
        compositor = static_cast<wl_compositor*>(wl_registry_bind (registry, name, &wl_compositor_interface, 1));
    }
    else if (!strcmp(interface,"wl_shell")) {
        shell = static_cast<wl_shell*>(wl_registry_bind (registry, name, &wl_shell_interface, 1));
    }
    else if (strcmp(interface, "wl_subcompositor") == 0) {
        subcompositor = static_cast<wl_subcompositor*>(wl_registry_bind(registry, name, &wl_subcompositor_interface, 1));
    }
    else if (!strcmp(interface,"wl_seat")) {
        seat = static_cast<wl_seat*>(wl_registry_bind (registry, name, &wl_seat_interface, 1));
        wl_seat_add_listener (seat, &seat_listener, data);
    }
}

static void registry_remove_object (void *data, struct wl_registry *registry, uint32_t name) {

}

static struct wl_registry_listener registry_listener = {&registry_add_object, &registry_remove_object};

static void shell_surface_ping (void *data, struct wl_shell_surface *shell_surface, uint32_t serial) {
    wl_shell_surface_pong (shell_surface, serial);
}

static void shell_surface_configure (void *data, struct wl_shell_surface *shell_surface, uint32_t edges, int32_t width, int32_t height) {
    struct window *window = static_cast<struct window*>(data);
//    wl_egl_window_resize (window->egl_window, width, height, 0, 0);
}

static void shell_surface_popup_done (void *data, struct wl_shell_surface *shell_surface) {

}

static struct wl_shell_surface_listener shell_surface_listener = {&shell_surface_ping, &shell_surface_configure, &shell_surface_popup_done};


static void create_window (struct window *window, int32_t width, int32_t height) {
    eglBindAPI (EGL_OPENGL_API);
    EGLint attributes[] = {
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
    EGL_NONE};
    EGLConfig config;
    EGLint num_config;
    eglChooseConfig(egl_display, attributes, &config, 1, &num_config);
    window->egl_context = eglCreateContext(egl_display, config, EGL_NO_CONTEXT, NULL);

    window->width = width;
    window->height = height;

    window->surface = wl_compositor_create_surface (compositor);
    window->shell_surface = wl_shell_get_shell_surface (shell, window->surface);
    wl_shell_surface_add_listener(window->shell_surface, &shell_surface_listener, window);
    wl_shell_surface_set_toplevel(window->shell_surface);
    wl_shell_surface_set_title(window->shell_surface, "example");
    window->egl_window = wl_egl_window_create(window->surface, width, height);
    window->egl_surface = eglCreateWindowSurface(egl_display, config, window->egl_window, NULL);

    // subsurface
    window->decorations.emplace_back(compositor, subcompositor, window->surface, width/2, -10, 20, 20, config, decoration::MOVE);
    window->decorations.emplace_back(compositor, subcompositor, window->surface, -10, width/2, 20, 20, config, decoration::RESIZE_LEFT);
    window->decorations.emplace_back(compositor, subcompositor, window->surface, width-10, width/2, 20, 20, config, decoration::RESIZE_RIGHT);
}

static void delete_window (struct window *window) {
    eglDestroySurface (egl_display, window->egl_surface);
    wl_egl_window_destroy (window->egl_window);
    wl_shell_surface_destroy (window->shell_surface);
    wl_surface_destroy (window->surface);
    eglDestroyContext (egl_display, window->egl_context);
}

static void draw_window (struct window *window) {
    eglMakeCurrent (egl_display, window->egl_surface, window->egl_surface, window->egl_context);
    glClearColor (0.0, 1.0, 0.0, 1.0);
    glClear (GL_COLOR_BUFFER_BIT);
    eglSwapBuffers (egl_display, window->egl_surface);

    // draw all decoration elements
    for(auto &d : window->decorations) {
        d.activate();
        glClearColor (1.0, 1.0, 0.0, 1.0);
        glClear (GL_COLOR_BUFFER_BIT);
        d.swap();
    }
}

int main() {
    std::cout << "Hello World!" << std::endl;

    struct window window;

    display = wl_display_connect(NULL);
    struct wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, &window);
    wl_display_roundtrip(display);

    egl_display = eglGetDisplay (display);
    eglInitialize(egl_display, NULL, NULL);

    create_window(&window, WIDTH, HEIGHT);

    while (running) {
        wl_display_dispatch_pending (display);
        draw_window (&window);
    }

    delete_window (&window);
    eglTerminate (egl_display);
    wl_display_disconnect (display);

    return 0;
}
