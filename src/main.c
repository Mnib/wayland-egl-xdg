#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>
#include <wayland-egl.h>

#include <EGL/egl.h>
#include <GLES2/gl2.h>

#include "xdg-shell-client-protocol.h"

#define DO(x)                                                                                                          \
    if (!x)                                                                                                            \
    return false

#define UNUSED(x) (void)(x)

#define WIDTH 450
#define HEIGHT 800

struct wl_display *s_Display;
struct wl_compositor *s_Compositor;
struct xdg_wm_base *s_WmBase;
struct wl_egl_window *s_EglWindow;
EGLDisplay *s_EglDisplay;
EGLContext *s_EglContext;
EGLSurface *s_EglSurface;

bool s_ShouldClose = false;

// Toplevel Handlers
static void handle_xdg_toplevel_configure(void *data, struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height,
                                          struct wl_array *states) {
    UNUSED(data);
    UNUSED(xdg_toplevel);
    UNUSED(width);
    UNUSED(height);
    UNUSED(states);

    // Noop
}

static void handle_xdg_toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel) {
    UNUSED(data);
    UNUSED(xdg_toplevel);

    fprintf(stdout, "Close request\n");
    s_ShouldClose = true;
}

static void handle_xdg_toplevel_configure_bounds(void *data, struct xdg_toplevel *xdg_toplevel, int32_t width,
                                                 int32_t height) {
    UNUSED(data);
    UNUSED(xdg_toplevel);
    UNUSED(width);
    UNUSED(height);

    // Noop
}

static void handle_xdg_toplevel_configure_wm_capabilities(void *data,
                                                          struct xdg_toplevel *xdg_toplevel,
                                                          struct wl_array *capabilities) {
    UNUSED(data);
    UNUSED(xdg_toplevel);
    UNUSED(capabilities);

    // Noop
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
    handle_xdg_toplevel_configure,
    handle_xdg_toplevel_close,
    handle_xdg_toplevel_configure_bounds,
    handle_xdg_toplevel_configure_wm_capabilities,
};
// END Toplevel Handlers


// Surface handlers
void handle_xdg_surface_configure(void *data, struct xdg_surface *xdg_surface, uint32_t serial) {
    xdg_surface_ack_configure(xdg_surface, serial);
    UNUSED(data);
}

const struct xdg_surface_listener xdg_surface_listener = {handle_xdg_surface_configure};
// END Surface handlers

// Registry handlers
static void handle_registry_global(void *data, struct wl_registry *registry, uint32_t name, const char *interface,
                                   uint32_t version) {
    UNUSED(data);
    UNUSED(version);

    if (strcmp(interface, "wl_compositor") == 0) {
        s_Compositor = (struct wl_compositor *)wl_registry_bind(registry, name, &wl_compositor_interface, 1);
    } else if (strcmp(interface, "xdg_wm_base") == 0) {
        s_WmBase = (struct xdg_wm_base *)wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
    }
}

static void handle_registry_global_remove(void *data, struct wl_registry *wl_registry, uint32_t name) {
    UNUSED(data);
    UNUSED(wl_registry);
    UNUSED(name);

    // Noop
}

static const struct wl_registry_listener registry_listener = {handle_registry_global, handle_registry_global_remove};
// END Registry handlers


bool show_window() {
    s_Display = wl_display_connect(NULL);
    if (s_Display == NULL) {
        fprintf(stderr, "Failed to connect to wayland display\n");
        return false;
    }

    struct wl_registry *registry = wl_display_get_registry(s_Display);
    wl_registry_add_listener(registry, &registry_listener, NULL);
    wl_display_roundtrip(s_Display);

    if (s_Compositor == NULL || s_WmBase == NULL) {
        fprintf(stderr, "Could not find a compositor or shell\n");
        return false;
    }

    struct wl_surface *surface = wl_compositor_create_surface(s_Compositor);
    if (surface == NULL) {
        fprintf(stderr, "Failed to create wayland surface\n");
        return false;
    }

    struct xdg_surface *xdg_surface = xdg_wm_base_get_xdg_surface(s_WmBase, surface);
    xdg_surface_add_listener(xdg_surface, &xdg_surface_listener, NULL);

    struct xdg_toplevel *xdg_toplevel = xdg_surface_get_toplevel(xdg_surface);
    xdg_toplevel_set_title(xdg_toplevel, "Wayland EGL Window");
    xdg_toplevel_add_listener(xdg_toplevel, &xdg_toplevel_listener, NULL);
    xdg_toplevel_set_min_size(xdg_toplevel, WIDTH, HEIGHT);
    xdg_toplevel_set_max_size(xdg_toplevel, WIDTH, HEIGHT);

    wl_surface_commit(surface);

    struct wl_region *region = wl_compositor_create_region(s_Compositor);
    wl_region_add(region, 0, 0, WIDTH, HEIGHT);
    wl_surface_set_opaque_region(surface, region);

    s_EglWindow = wl_egl_window_create(surface, WIDTH, HEIGHT);
    if (s_EglWindow == EGL_NO_SURFACE) {
        fprintf(stderr, "Failed to create EGL window\n");
        return false;
    }

    return true;
}

bool init_egl() {
    int32_t major, minor;

    // clang-format off
    const int32_t context_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };
    // clang-format on

    // clang-format off
    const int32_t config_attribs[] = {
        EGL_SURFACE_TYPE,       EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE,    EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE,           8,
        EGL_GREEN_SIZE,         8,
        EGL_BLUE_SIZE,          8,
        EGL_DEPTH_SIZE,         24,
        EGL_NONE
    };
    // clang-format on

    s_EglDisplay = eglGetDisplay((EGLNativeDisplayType)s_Display);
    if (s_EglDisplay == EGL_NO_DISPLAY) {
        fprintf(stderr, "Can't create egl display\n");
        return false;
    }

    if (eglInitialize(s_EglDisplay, &major, &minor) != EGL_TRUE) {
        fprintf(stderr, "Can't initialise egl display\n");
        return false;
    }

    fprintf(stdout,
            "Using display %p\n"
            "============================================\n"
            "  Version: %d.%d\n"
            "  Vendor:  %s\n"
            "  Extensions: %s\n"
            "============================================",
            s_EglDisplay, major, minor, eglQueryString(s_EglDisplay, EGL_VENDOR),
            eglQueryString(s_EglDisplay, EGL_EXTENSIONS));

    int32_t num_config = 0;
    int32_t config_match = 0;

    if (!eglBindAPI(EGL_OPENGL_ES_API)) {
        fprintf(stderr, "Failed to bind api\n");
        return false;
    }

    if (!eglGetConfigs(s_EglDisplay, NULL, 0, &num_config)) {
        fprintf(stderr, "Failed to find EGL configurations to choose from.\n");
        return false;
    }

    EGLConfig *configs = calloc(num_config, sizeof(EGLConfig));
    if (!eglChooseConfig(s_EglDisplay, config_attribs, configs, num_config, &config_match) || !config_match) {
        fprintf(stderr, "Failed to find EGL configuration with appropriate attributes.\n");
        return false;
    }

    s_EglContext = eglCreateContext(s_EglDisplay, configs[0], EGL_NO_CONTEXT, context_attribs);
    if (s_EglContext == EGL_NO_CONTEXT) {
        fprintf(stderr, "Failed to create EGL context\n");
        return false;
    }

    s_EglSurface = eglCreateWindowSurface(s_EglDisplay, configs[0], (EGLNativeWindowType)s_EglWindow, NULL);
    if (s_EglSurface == EGL_NO_SURFACE) {
        fprintf(stderr, "Failed to create EGL surface\n");
        return false;
    }

    if (!eglMakeCurrent(s_EglDisplay, s_EglSurface, s_EglSurface, s_EglContext)) {
        fprintf(stderr, "Failed to make context current\n");
        return false;
    }

    free(configs);

    return true;
}

void signal_handler(int sig) {
    UNUSED(sig);

    s_ShouldClose = true;
}

int main() {
    DO(show_window());
    DO(init_egl());

    fprintf(stdout,
            "GL API Loaded\n"
            "==================================================\n"
            "  Version:     %s\n"
            "  SL Version:  %s\n"
            "  Vendor:      %s\n"
            "  Renderer:    %s\n"
            "  Extensions:  %s\n"
            "==================================================\n",
            glGetString(GL_VERSION), glGetString(GL_SHADING_LANGUAGE_VERSION), glGetString(GL_VENDOR),
            glGetString(GL_RENDERER), glGetString(GL_EXTENSIONS));

    signal(SIGINT, signal_handler);

    while (!s_ShouldClose) {
        glClearColor(0.3f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        eglSwapBuffers(s_EglDisplay, s_EglSurface);

        if (wl_display_dispatch(s_Display) == -1) {
            fprintf(stdout, "wl_display_dispatch: -1");
            break;
        }
    }

    // Destroy EGL
    eglMakeCurrent(s_EglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroySurface(s_EglDisplay, s_EglSurface);
    eglDestroyContext(s_EglDisplay, s_EglContext);
    eglTerminate(s_EglDisplay);

    xdg_wm_base_destroy(s_WmBase);
    wl_display_disconnect(s_Display);
    wl_egl_window_destroy(s_EglWindow);

    return 0;
}
