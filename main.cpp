#include <iostream>
#include <wayland-client.h>
#include <wayland-egl.h>
#include <wayland-cursor.h>
#include <wayland-xdg-shell-client-protocol.h>
#include <EGL/egl.h>
#include <GL/gl.h>
#include <cstring>
#include <linux/input.h>
#include <vector>
#include <map>

static struct wl_display *display;
static struct wl_compositor *compositor = NULL;
static struct wl_subcompositor *subcompositor = NULL;
static struct wl_shell *shell = NULL;
static struct xdg_wm_base *xdg_wm_base = NULL;
static struct wl_seat *seat = NULL;
static struct wl_shm *shm = NULL;
static struct wl_cursor_theme *cursor_theme = NULL;
static struct wl_surface *cursor_surface = NULL;
static EGLDisplay egl_display;
static bool running = true;

struct decoration;

struct button;

struct window {
    EGLContext egl_context;
    struct wl_surface *surface;
    struct wl_shell_surface *shell_surface;
    struct xdg_surface *xdg_surface;
    struct xdg_toplevel *xdg_toplevel = NULL;
    struct wl_egl_window *egl_window;
    EGLSurface egl_surface;

    int width;
    int height;
    uint border_size;
    uint title_size;

    bool maximised = false;

    std::vector<decoration> decorations;

    std::vector<button> buttons;

    bool button_pressed = false;
    wl_surface * current_surface = NULL;    // last entered surface

    bool inhibit_motion = false;
};

struct button {
    struct wl_surface *surface;
    struct wl_subsurface *subsurface;
    struct wl_egl_window *egl_window;
    EGLSurface egl_surface;
    EGLContext egl_context;
    EGLDisplay egl_display;
    double r, g, b, a;

    enum type {
        CLOSE, MAXIMISE, MINIMISE
    } function;

    button(wl_compositor* compositor, wl_subcompositor* subcompositor, wl_surface* source,
           EGLConfig config, int32_t x, int32_t y, type fnct, double _r, double _g, double _b, double _a)
    {
        function = fnct;
        r=_r; g=_g; b=_b; a=_a;
        surface = wl_compositor_create_surface(compositor);
        subsurface = wl_subcompositor_get_subsurface(subcompositor, surface, source);
        wl_subsurface_set_desync(subsurface);
        egl_display = eglGetDisplay(display);
        egl_context = eglCreateContext (egl_display, config, EGL_NO_CONTEXT, NULL);
        eglInitialize(egl_display, NULL, NULL);
        wl_subsurface_set_position(subsurface, x, y);
        egl_window = wl_egl_window_create(surface, 10, 8);
        egl_surface = eglCreateWindowSurface(egl_display, config, egl_window, NULL);
    }

    void draw() const {
        eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context);
        glClearColor(r, g, b, a);
        glClear(GL_COLOR_BUFFER_BIT);
        eglSwapBuffers(egl_display, egl_surface);
    }
};

struct decoration {
    struct wl_surface *surface;
    struct wl_subsurface *subsurface;
    struct wl_egl_window *egl_window;
    EGLSurface egl_surface;
    EGLContext egl_context;
    EGLDisplay egl_display;
    double r, g, b, a;
    uint border_size;
    uint title_bar_size;

    enum wl_shell_surface_resize function;

    decoration(wl_compositor* compositor, wl_subcompositor* subcompositor, wl_surface* source,
               const uint _border_size, const uint _title_bar_size,
               EGLConfig config, enum wl_shell_surface_resize type,
               double _r, double _g, double _b, double _a)
    {
        function = type;
        r=_r; g=_g; b=_b; a=_a;
        border_size = _border_size;
        title_bar_size = _title_bar_size;

        surface = wl_compositor_create_surface(compositor);
        subsurface = wl_subcompositor_get_subsurface(subcompositor, surface, source);
        wl_subsurface_set_desync(subsurface);
        egl_display = eglGetDisplay(display);
        egl_context = eglCreateContext (egl_display, config, EGL_NO_CONTEXT, NULL);
        eglInitialize(egl_display, NULL, NULL);
        wl_subsurface_set_position(subsurface, 0, 0);
        egl_window = wl_egl_window_create(surface, 1, 1);
        egl_surface = eglCreateWindowSurface(egl_display, config, egl_window, NULL);
    }

    ~decoration() {
//        wl_subsurface_destroy(subsurface);
//        wl_subcompositor_destroy(subcompositor);
    }

    void calc_dim(const int main_w, const int main_h, int &x, int &y, int &w, int &h) {
        // get position and dimension from type and main surface
        switch (function) {
        case WL_SHELL_SURFACE_RESIZE_NONE:
            x=0; y=-title_bar_size;
            w=main_w; h=title_bar_size;
            break;
        case WL_SHELL_SURFACE_RESIZE_TOP:
            x=0; y=-title_bar_size-border_size;
            w=main_w; h=border_size;
            break;
        case WL_SHELL_SURFACE_RESIZE_BOTTOM:
            x=0; y=main_h;
            w=main_w; h=border_size;
            break;
        case WL_SHELL_SURFACE_RESIZE_LEFT:
            x=-border_size; y=-title_bar_size;
            w=border_size; h=main_h+title_bar_size;
            break;
        case WL_SHELL_SURFACE_RESIZE_TOP_LEFT:
            x=-border_size; y=-border_size-title_bar_size;
            w=border_size; h=border_size;
            break;
        case WL_SHELL_SURFACE_RESIZE_BOTTOM_LEFT:
            x=-border_size; y=main_h;
            w=border_size; h=border_size;
            break;
        case WL_SHELL_SURFACE_RESIZE_RIGHT:
            x=main_w; y=-title_bar_size;
            w=border_size; h=main_h+title_bar_size;
            break;
        case WL_SHELL_SURFACE_RESIZE_TOP_RIGHT:
            x=main_w; y=-border_size-title_bar_size;
            w=border_size; h=border_size;
            break;
        case WL_SHELL_SURFACE_RESIZE_BOTTOM_RIGHT:
            x=main_w; y=main_h;
            w=border_size; h=border_size;
            break;
        }
    }

    void resize(const int main_w, const int main_h) {
        int x,y,w,h;
        calc_dim(main_w, main_h, x, y, w, h);
        wl_subsurface_set_position(subsurface, x, y);
//        egl_window = wl_egl_window_create(surface, w, h);
        wl_egl_window_resize(egl_window, w, h, 0, 0);

    }

    void draw() const {
        eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context);
        //glClearColor(1.0, 1.0, 0.0, 1.0);
        glClearColor(r, g, b, a);
        glClear(GL_COLOR_BUFFER_BIT);
        eglSwapBuffers(egl_display, egl_surface);
    }
};

static const std::map<enum wl_shell_surface_resize, std::string> resize_cursor = {
    {WL_SHELL_SURFACE_RESIZE_NONE, "grabbing"},
    {WL_SHELL_SURFACE_RESIZE_TOP, "top_side"},
    {WL_SHELL_SURFACE_RESIZE_BOTTOM, "bottom_side"},
    {WL_SHELL_SURFACE_RESIZE_LEFT, "left_side"},
    {WL_SHELL_SURFACE_RESIZE_TOP_LEFT, "top_left_corner"},
    {WL_SHELL_SURFACE_RESIZE_BOTTOM_LEFT, "bottom_left_corner"},
    {WL_SHELL_SURFACE_RESIZE_RIGHT, "right_side"},
    {WL_SHELL_SURFACE_RESIZE_TOP_RIGHT, "top_right_corner"},
    {WL_SHELL_SURFACE_RESIZE_BOTTOM_RIGHT, "bottom_right_corner"}
};

void window_resize(struct window *window, const int width, const int height, bool full);

// listeners

static void xdg_surface_handle_configure(void *data,
        struct xdg_surface *xdg_surface, uint32_t serial) {
    xdg_surface_ack_configure(xdg_surface, serial);
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_handle_configure,
};

void xdg_toplevel_handle_configure(void *data, struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height, struct wl_array *states) {
    if (width==0 || height==0)
        return;
    struct window *window = static_cast<struct window*>(data);
    window_resize(window, width, height, true);
}

static void xdg_toplevel_handle_close(void *data, struct xdg_toplevel *xdg_toplevel) {
    running = false;
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
    .configure = xdg_toplevel_handle_configure,
    .close = xdg_toplevel_handle_close,
};

static void pointer_enter (void *data, struct wl_pointer *pointer, uint32_t serial, struct wl_surface *surface, wl_fixed_t surface_x, wl_fixed_t surface_y) {
    window *w = static_cast<window*>(data);
    w->current_surface = surface;

    std::string cursor = "left_ptr";

    for(const decoration &d: w->decorations) {
        if(d.surface==surface) {
            if(resize_cursor.count(d.function)) {
                cursor = resize_cursor.at(d.function);
            }
        }
    }

    const auto image = wl_cursor_theme_get_cursor(cursor_theme, cursor.c_str())->images[0];
    wl_pointer_set_cursor(pointer, serial, cursor_surface, image->hotspot_x, image->hotspot_y);
    wl_surface_attach(cursor_surface, wl_cursor_image_get_buffer(image), 0, 0);
    wl_surface_damage(cursor_surface, 0, 0, image->width, image->height);
    wl_surface_commit(cursor_surface);
}

static void pointer_leave (void *data, struct wl_pointer *pointer, uint32_t serial, struct wl_surface *surface) {
    window *w = static_cast<window*>(data);
    w->button_pressed = false;
}

static void pointer_motion (void *data, struct wl_pointer *pointer, uint32_t time, wl_fixed_t x, wl_fixed_t y) {
//    std::cout << "pointer motion " << wl_fixed_to_double(x) << " " << wl_fixed_to_double(y) << std::endl;
}

static void pointer_button (void *data, struct wl_pointer *pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t state) {
//    std::cout << "pointer button " << button << ", state " << state << std::endl;

    window *w = static_cast<window*>(data);
    w->button_pressed = (button==BTN_LEFT) && (state==WL_POINTER_BUTTON_STATE_PRESSED);

    if(w->button_pressed) {
        for(int i = 0; i<w->decorations.size(); i++) {
            if(w->decorations[i].surface==w->current_surface) {
                switch(w->decorations[i].function) {
                case WL_SHELL_SURFACE_RESIZE_NONE:
                    if(w->xdg_toplevel) {
                        xdg_toplevel_move(w->xdg_toplevel, seat, serial);
                    } else if(w->shell_surface) {
                        wl_shell_surface_move(w->shell_surface, seat, serial);
                    }
                    break;
                default:
                    if(w->xdg_toplevel) {
                        xdg_toplevel_resize(w->xdg_toplevel, seat, serial, w->decorations[i].function);
                    } else if(w->shell_surface) {
                        wl_shell_surface_resize(w->shell_surface, seat, serial, w->decorations[i].function);
                    }
                    break;
                }
            }
        }

        for(const struct button &b: w->buttons) {
            if(b.surface==w->current_surface) {
                switch (b.function) {
                case button::type::CLOSE:
                    running = false;
                    break;
                case button::type::MAXIMISE:
                    if(w->maximised) {
                        wl_shell_surface_set_toplevel(w->shell_surface);
                        window_resize(w, w->width, w->height, false);
                    }
                    else {
                        // store original window size
                        wl_egl_window_get_attached_size(w->egl_window, &w->width, &w->height);
                        wl_shell_surface_set_maximized(w->shell_surface, NULL);
                    }
                    w->maximised = !w->maximised;
                    break;
                case button::type::MINIMISE:
                    std::cout << "not implemented: minimise" << std::endl;
                    break;
                }
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
        cursor_surface = wl_compositor_create_surface(compositor);
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
    else if (strcmp(interface, "wl_shm") == 0) {
        shm = static_cast<wl_shm*>(wl_registry_bind(registry, name, &wl_shm_interface, version));
        cursor_theme = wl_cursor_theme_load(nullptr, 32, shm);
    }
    else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        std::cout << "have " << xdg_wm_base_interface.name << std::endl;
        xdg_wm_base = static_cast<struct xdg_wm_base*>(wl_registry_bind(registry, name, &xdg_wm_base_interface, version));
    }
}

static void registry_remove_object (void *data, struct wl_registry *registry, uint32_t name) {

}

static struct wl_registry_listener registry_listener = {&registry_add_object, &registry_remove_object};

static void shell_surface_ping (void *data, struct wl_shell_surface *shell_surface, uint32_t serial) {
    wl_shell_surface_pong (shell_surface, serial);
}

static void shell_surface_configure(void *data, struct wl_shell_surface *shell_surface, uint32_t edges, int32_t width, int32_t height) {
    struct window *window = static_cast<struct window*>(data);
//    std::cout << "config " << edges << " " << width << " " << height << std::endl;
    window_resize(window, width, height, true);
}

static void shell_surface_popup_done (void *data, struct wl_shell_surface *shell_surface) {

}

static struct wl_shell_surface_listener shell_surface_listener = {&shell_surface_ping, &shell_surface_configure, &shell_surface_popup_done};


static void create_window(struct window *window, int32_t width, int32_t height) {
    const uint border_size = 5;
    const uint title_size = 15;
    eglBindAPI (EGL_OPENGL_API);
    EGLint attributes[] = {
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
//        EGL_ALPHA_SIZE, 8,
        EGL_NONE};
    EGLConfig config;
    EGLint num_config;
    eglChooseConfig(egl_display, attributes, &config, 1, &num_config);
    window->egl_context = eglCreateContext(egl_display, config, EGL_NO_CONTEXT, NULL);

    window->width = width;
    window->height = height;
    window->border_size = border_size;
    window->title_size = title_size;

    window->surface = wl_compositor_create_surface (compositor);

    if(xdg_wm_base) {
        window->xdg_surface = xdg_wm_base_get_xdg_surface(xdg_wm_base, window->surface);
        window->xdg_toplevel = xdg_surface_get_toplevel(window->xdg_surface);
        xdg_surface_add_listener(window->xdg_surface, &xdg_surface_listener, window);
        xdg_toplevel_add_listener(window->xdg_toplevel, &xdg_toplevel_listener, window);
        xdg_toplevel_set_title(window->xdg_toplevel, "example");
    }
    else if(shell) {
        window->shell_surface = wl_shell_get_shell_surface (shell, window->surface);
        wl_shell_surface_add_listener(window->shell_surface, &shell_surface_listener, window);
        wl_shell_surface_set_toplevel(window->shell_surface);
        wl_shell_surface_set_title(window->shell_surface, "example");
    }
    else {
        std::cout << "no xdg_wm_base, no wl_shell" << std::endl;
    }

    window->egl_window = wl_egl_window_create(window->surface, width, height);
    window->egl_surface = eglCreateWindowSurface(egl_display, config, window->egl_window, NULL);

    // subsurface
    window->decorations.emplace_back(compositor, subcompositor, window->surface, border_size, title_size, config, WL_SHELL_SURFACE_RESIZE_NONE, 1,0,0,1);

    window->decorations.emplace_back(compositor, subcompositor, window->surface, border_size, title_size, config, WL_SHELL_SURFACE_RESIZE_LEFT, 1,1,0,1);
    window->decorations.emplace_back(compositor, subcompositor, window->surface, border_size, title_size, config, WL_SHELL_SURFACE_RESIZE_RIGHT, 1,1,0,1);
    window->decorations.emplace_back(compositor, subcompositor, window->surface, border_size, title_size, config, WL_SHELL_SURFACE_RESIZE_TOP, 1,1,0,1);
    window->decorations.emplace_back(compositor, subcompositor, window->surface, border_size, title_size, config, WL_SHELL_SURFACE_RESIZE_BOTTOM, 1,1,0,1);

    window->decorations.emplace_back(compositor, subcompositor, window->surface, border_size, title_size, config, WL_SHELL_SURFACE_RESIZE_TOP_LEFT, 0,0,1,1);
    window->decorations.emplace_back(compositor, subcompositor, window->surface, border_size, title_size, config, WL_SHELL_SURFACE_RESIZE_TOP_RIGHT, 0,0,1,1);
    window->decorations.emplace_back(compositor, subcompositor, window->surface, border_size, title_size, config, WL_SHELL_SURFACE_RESIZE_BOTTOM_LEFT, 0,0,1,1);
    window->decorations.emplace_back(compositor, subcompositor, window->surface, border_size, title_size, config, WL_SHELL_SURFACE_RESIZE_BOTTOM_RIGHT, 0,0,1,1);

    window->buttons.emplace_back(compositor, subcompositor, window->decorations[0].surface, config, 5, 4, button::type::CLOSE, 0,0,0,1);
    window->buttons.emplace_back(compositor, subcompositor, window->decorations[0].surface, config, 20, 4, button::type::MAXIMISE, 0.5,0.5,0.5,1);
    window->buttons.emplace_back(compositor, subcompositor, window->decorations[0].surface, config, 35, 4, button::type::MINIMISE, 1,1,1,1);

    window_resize(window, width, height, false);
}

static void delete_window (struct window *window) {
    eglDestroySurface (egl_display, window->egl_surface);
    wl_egl_window_destroy (window->egl_window);
    wl_shell_surface_destroy (window->shell_surface);
    if(xdg_wm_base) {
        xdg_toplevel_destroy(window->xdg_toplevel);
        xdg_surface_destroy(window->xdg_surface);
    }
    wl_surface_destroy (window->surface);
    eglDestroyContext (egl_display, window->egl_context);
}

void window_resize(struct window *window, const int width, const int height, bool full) {
//    std::cout << "config " << width << " " << height << std::endl;
    // main surface with from full surface
    int main_w, main_h;
    if(full) {
        main_w = width-2*window->border_size;
        main_h = height-2*window->border_size-window->title_size;
//        std::cout << "new size " << main_w << " " << main_h << std::endl;
    }
    else {
        main_w = width;
        main_h = height;
    }

    // set minimum size
    main_w = std::max(main_w, 50);
    main_h = std::max(main_h, 50);

    // resize main surface
    wl_egl_window_resize(window->egl_window, main_w, main_h, 0, 0);

    // resize all decoration elements
    for(auto &d : window->decorations) { d.resize(main_w, main_h); }
}

static void draw_window(struct window *window) {
    eglMakeCurrent(egl_display, window->egl_surface, window->egl_surface, window->egl_context);
    glClearColor(0.0, 1.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
    eglSwapBuffers(egl_display, window->egl_surface);

    // draw all decoration elements
    for(const decoration &d : window->decorations) { d.draw(); }

    for(const button &b : window->buttons) { b.draw(); }
}

int main() {
    std::cout << "Hello World!" << std::endl;

    struct window window;

    display = wl_display_connect(NULL);
    if(!display) {
        std::cerr << "cannot connect to Wayland server" << std::endl;
        return EXIT_FAILURE;
    }
    struct wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, &window);
    wl_display_roundtrip(display);

    egl_display = eglGetDisplay (display);
    eglInitialize(egl_display, NULL, NULL);

    create_window(&window, 256, 256);

    while (running) {
        wl_display_dispatch_pending (display);
        draw_window (&window);
    }

    delete_window (&window);
    eglTerminate (egl_display);
    wl_display_disconnect (display);

    return 0;
}
