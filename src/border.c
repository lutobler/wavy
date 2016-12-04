#include <stdlib.h>
#include <stdint.h>
#include <cairo/cairo.h>
#include <wlc/wlc.h>
#include <wlc/wlc-render.h>

#include "border.h"
#include "layout.h"
#include "utils.h"

static void draw_border_rectangle(cairo_t *cr, uint32_t thickness,
        struct wlc_geometry *g, uint32_t x_off, uint32_t y_off) {

    if (thickness == 0) {
        return;
    }

    // left border
    cairo_rectangle(cr, x_off, y_off, thickness, g->size.h);
    cairo_fill(cr);

    // right border
    cairo_rectangle(cr, x_off + g->size.w - thickness, y_off, thickness,
            g->size.h);
    cairo_fill(cr);

    // top border
    cairo_rectangle(cr, x_off, y_off, g->size.w, thickness);
    cairo_fill(cr);

    // bottom border
    cairo_rectangle(cr, x_off, y_off + g->size.h - thickness,
            g->size.w, thickness);
    cairo_fill(cr);
}

static void frame_buffer_realloc(struct frame *fr) {
    if (!fr) {
        return;
    }

    free_border(&fr->border);

    fr->border.cr = NULL;
    fr->border.surface = NULL;

	int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32,
                                                fr->border.g_gaps.size.w);

    fr->border.buf_size = stride * fr->border.g_gaps.size.h * sizeof(unsigned char);
    fr->border.buffer = calloc(fr->border.buf_size, 1);
    fr->border.surface = cairo_image_surface_create_for_data(fr->border.buffer,
                                                        CAIRO_FORMAT_ARGB32,
                                                        fr->border.g_gaps.size.w,
                                                        fr->border.g_gaps.size.h,
                                                        stride);
    fr->border.cr = cairo_create(fr->border.surface);
}

void free_border(struct border_t *border) {
    if (border->buffer) {
        free(border->buffer);
    }

    if (border->cr) {
        cairo_destroy(border->cr);
    }

    if (border->surface) {
        cairo_surface_destroy(border->surface);
    }
}

void update_view_border(struct frame *fr, wlc_handle view,
        struct wlc_geometry *g) {

    cairo_t *cr = fr->border.cr;

    // figure out the right color
    if (fr->active_view == view && get_active_frame() == fr) {
        cr_set_argb_color(cr, config->view_border_active_color);
    } else {
        cr_set_argb_color(cr, config->view_border_inactive_color);
    }

    draw_border_rectangle(cr, config->view_border_size, g,
            g->origin.x, g->origin.y);
    cairo_surface_flush(fr->border.surface);
}

void update_frame_border(struct frame *fr, bool realloc) {
    if (realloc || !fr->border.buffer) {
        // reallocate the buffer entirely
        frame_buffer_realloc(fr);
    } else {
        // just clear the buffer by setting it to zero
        memset(fr->border.buffer, 0, fr->border.buf_size);
    }

    cairo_t *cr = fr->border.cr;
    cairo_surface_t *surface = fr->border.surface;

    uint32_t color;
    uint32_t size;
    if (fr == get_active_frame()) {
        if (fr->children->length == 0) {
            size = config->frame_border_empty_size;
            color = config->frame_border_empty_active_color;
        } else {
            size = config->frame_border_size;
            color = config->frame_border_active_color;
        }
    } else {
        if (fr->children->length == 0) {
            size = config->frame_border_empty_size;
            color = config->frame_border_empty_inactive_color;
        } else {
            size = config->frame_border_size;
            color = config->frame_border_inactive_color;
        }
    }

    cr_set_argb_color(cr, color);
    draw_border_rectangle(cr, size, &fr->border.g_gaps, 0, 0);
    cairo_surface_flush(surface);
}

void render_frame_borders(struct frame *fr) {
    if (!fr) {
        return;
    }

    render_frame_borders(fr->left);
    render_frame_borders(fr->right);

    if (fr->split == SPLIT_NONE && fr->border.buffer) {
        wlc_pixels_write(WLC_RGBA8888, &fr->border.g_gaps, fr->border.buffer);
    }
}
