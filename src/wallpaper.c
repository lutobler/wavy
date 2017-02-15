#include <wlc/wlc.h>
#include <cairo/cairo.h>
#include <stdlib.h>
#include <unistd.h>

#include "wayland-background-client-protocol.h"
#include "wallpaper.h"
#include "log.h"
#include "wayland.h"
#include "vector.h"
#include "config.h"

static void *set_wallpaper(unsigned char *buf, cairo_surface_t *cs) {
    struct registry *reg = registry_poll();
    if (!reg) {
        wavy_log(LOG_ERROR, "Failed to poll registry");
        return NULL;
    }

    void *shm_data = NULL;
    struct wl_surface *surface = wl_compositor_create_surface(reg->compositor);
    struct wl_buffer *buffer = create_shm_buffer(reg->shm, 1920, 1080,
            &shm_data);

    memcpy(shm_data, buf, 1920 * 1080 * 4);
    create_window(surface, buffer, 1920, 1080);

    for (uint32_t i = 0; i < reg->outputs->length; i++) {
        struct wl_output *o = reg->outputs->items[i];
        background_set_background(reg->background, o, surface);
    }

    while (wl_display_dispatch(reg->display) != -1);

    free_registry(reg);
    cairo_surface_destroy(cs);
    return NULL;
}

void init_wallpaper() {
    char *png_file = config->wallpaper;
    if (!png_file) {
        return;
    }

    cairo_surface_t *cs = cairo_image_surface_create_from_png(png_file);
    int status = cairo_surface_status(cs);

    if (status == CAIRO_STATUS_FILE_NOT_FOUND) {
        wavy_log(LOG_ERROR, "Wallpaper file not found.");
        exit(EXIT_FAILURE);
    } else if (status != CAIRO_STATUS_SUCCESS) {
        wavy_log(LOG_ERROR, "Failed to load wallpaper file.");
        exit(EXIT_FAILURE);
    }

    wavy_log(LOG_DEBUG, "Loaded wallpaper file: %s", png_file);

    cairo_surface_flush(cs);
    unsigned char *buf = cairo_image_surface_get_data(cs);

    pid_t p;
    if ((p = fork()) == 0) {
        set_wallpaper(buf, cs);
        _exit(EXIT_FAILURE);
    } else if (p < 0) {
        wavy_log(LOG_ERROR, "Failed to fork background process");
    }
}
