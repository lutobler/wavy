#include <stdlib.h>
#include <cairo/cairo.h>
#include <wlc/wlc.h>
#include <wlc/wlc-render.h>
#include <wlc/wlc-wayland.h>
#include <wayland-client.h>

#include "wallpaper.h"
#include "log.h"

// One big TODO

void init_wallpaper(struct output *out, uint32_t width, uint32_t height) {
    struct wlc_geometry g = { {0, 0}, {width, height} };
    out->wallpaper.g = g;

    out->wallpaper.surface = NULL;

    out->wallpaper.surface = cairo_image_surface_create_from_png(wp_file);
    int status = cairo_surface_status(out->wallpaper.surface);
    if (status == CAIRO_STATUS_FILE_NOT_FOUND) {
        wavy_log(LOG_DEBUG, "Wallpaper file not found.");
        exit(EXIT_FAILURE);
    } else if (status != CAIRO_STATUS_SUCCESS) {
        wavy_log(LOG_DEBUG, "Failed to load wallpaper file.");
        exit(EXIT_FAILURE);
    }

    cairo_surface_flush(out->wallpaper.surface);
    out->wallpaper.buffer = cairo_image_surface_get_data(
            out->wallpaper.surface);
}

void render_wallpaper(struct output *out) {
    if (!out) {
        return;
    }
}

void free_wallpaper(struct output *out) {
    cairo_surface_destroy(out->wallpaper.surface);
    out->wallpaper.buffer = NULL;
}
