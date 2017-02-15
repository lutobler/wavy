#ifndef __WAYLAND_H
#define __WAYLAND_H

#include <wayland-client.h>

struct registry {
    struct wl_compositor *compositor;
    struct wl_display *display;
    struct wl_shm *shm;
    struct wl_shell *shell;
    struct vector_t *outputs; // vector of struct wl_output *
    struct background *background;
};

struct registry *registry_poll();
void free_registry(struct registry *reg);

struct wl_buffer *create_shm_buffer(struct wl_shm *shm, uint32_t width,
        uint32_t height, void **shm_data);

void create_window(struct wl_surface *surface, struct wl_buffer *buffer,
        uint32_t width, uint32_t height);

#endif
