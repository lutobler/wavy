#include <stdlib.h>
#include <stdio.h>
#include <cairo/cairo.h>
#include <pango/pangocairo.h>
#include <wlc/wlc-render.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "bar.h"
#include "config.h"
#include "utils.h"
#include "layout.h"
#include "log.h"
#include "vector.h"

#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#define MAX(a,b) (((a) > (b)) ? (a) : (b))

static void update_all_bars() {
    struct vector_t *outs = get_outputs();
    for (uint32_t i = 0; i < outs->length; i++) {
        struct output *out = outs->items[i];
        update_bar(out);
        wlc_output_schedule_render(out->output_handle);
    }
}

void add_widget(enum side_t side, enum hook_t hook, int lua_ref) {
    struct status_entry_t *new_widget;
    new_widget = calloc(1, sizeof(struct status_entry_t));
    if (!new_widget) {
        return;
    }

    new_widget->hook = hook;
    new_widget->side = side;
    new_widget->bg_color = 0;
    new_widget->fg_color = 0;
    new_widget->lua_reg_idx = lua_ref;
    vector_add(status_entries, new_widget);
}

static void update_entry(struct status_entry_t *entry) {
    lua_rawgeti(L_config, LUA_REGISTRYINDEX, entry->lua_reg_idx);
    lua_pcall(L_config, 0, 1, 0);
    if (!lua_istable(L_config, -1)) {
        luaL_error(L_config,
                "Statusbar callback function returned a %s instead of a table",
                lua_typename(L_config, -1));
    }

    if (entry->entry) {
        free(entry->entry);
        entry->entry = NULL;
    }

    if (lua_geti(L_config, -1, 1) == LUA_TNUMBER &&
        lua_geti(L_config, -2, 2) == LUA_TNUMBER &&
        lua_geti(L_config, -3, 3) == LUA_TSTRING) {

        entry->entry = strdup(lua_tostring(L_config, -1));
        entry->fg_color = lua_tointeger(L_config, -2);
        entry->bg_color = lua_tointeger(L_config, -3);
        lua_pop(L_config, 3);
    } else {
        luaL_error(L_config,
                "Invalid entry in table returned by a statusbar callback");
    }
}

void trigger_hook(enum hook_t hook) {
    pthread_mutex_lock(&lua_lock);
    for (uint32_t i = 0; i < status_entries->length; i++) {
        struct status_entry_t *e = status_entries->items[i];
        if (e->hook == hook) {
            update_entry(e);
        }
    }
    pthread_mutex_unlock(&lua_lock);
    update_all_bars();
}

static PangoLayout *setup_pango_layout(cairo_t *cr, char *text) {
    PangoLayout *layout;
    PangoFontDescription *desc;

    layout = pango_cairo_create_layout(cr);
    desc = pango_font_description_from_string(config->statusbar_font);
    pango_layout_set_text(layout, text, -1);
    pango_layout_set_font_description(layout, desc);
    pango_cairo_update_layout(cr, layout);
    pango_font_description_free(desc);

    return layout;
}

static void draw_text(cairo_t *cr, PangoLayout *layout, uint32_t w, uint32_t h,
        uint32_t x, uint32_t y) {

    int width, height;
    pango_layout_get_size(layout, &width, &height);
    width /= PANGO_SCALE;
    height /= PANGO_SCALE;
    cairo_move_to(cr, x + (w - width)/2, y + (h - height)/2);
    pango_cairo_show_layout(cr, layout);
}

static void draw_workspace_indicators(struct output *out, cairo_t *cr) {
    struct vector_t *workspaces = get_workspaces();
    uint32_t ws_rect_width = 20;
    uint32_t bar_height = config->statusbar_height;

    for (unsigned i = 0; i < workspaces->length; i++) {
        struct workspace *ws = workspaces->items[i];
        uint32_t ws_color;
        uint32_t font_color;

        // background color
        if (ws->is_visible && out == ws->assigned_output) {
            ws_color = config->statusbar_active_ws_color;
            font_color = config->statusbar_active_ws_font_color;
        } else {
            ws_color = config->statusbar_inactive_ws_color;
            font_color = config->statusbar_inactive_ws_font_color;
        }

        cr_set_argb_color(cr, ws_color);
        cairo_rectangle(cr, i * ws_rect_width, 0, ws_rect_width,
                bar_height);
        cairo_fill(cr);

        char num[8];
        sprintf(num, "%d", ws->number + 1); // lets use 1-indexed workspaces

        PangoLayout *layout = setup_pango_layout(cr, num);
        cr_set_argb_color(cr, font_color);
        draw_text(cr, layout, ws_rect_width, bar_height,
                i*ws_rect_width, out->bar.g.origin.y);
        g_object_unref(layout);
    }
}

static void draw_data(struct bar_t *bar, cairo_t *cr) {
    if (!status_entries) { // possibly uninitialized
        return;
    }

    uint32_t bar_height = config->statusbar_height;
    uint32_t padding = config->statusbar_padding;
    uint32_t gap = config->statusbar_gap;

    // add gap so the rightmost element is flush with the end of the screen
    uint32_t prev_x_right = bar->g.size.w + gap;
    uint32_t prev_x_left = (get_workspaces())->length * 20;

    uint32_t sep_x = 0;
    uint32_t sep_h = bar_height * 0.6; // a factor of 0.6 seems to look nice
    uint32_t sep_y = (bar_height - sep_h) / 2;
    uint32_t sep_w = config->statusbar_separator_width;

    bool first_right = true;
    bool first_left = true;

    for (uint32_t i = 0; i < status_entries->length; i++) {
        struct status_entry_t *e = status_entries->items[i];
        if (!e->entry || strlen(e->entry) == 0 ) {
            continue;
        }

        // don't draw overlapping elements
        if (prev_x_left > prev_x_right) {
            break;
        }

        int text_width, text_height;
        PangoLayout *layout = setup_pango_layout(cr, e->entry);
        pango_layout_get_size(layout, &text_width, &text_height);
        text_width /= PANGO_SCALE;

        uint32_t x = 0;
        uint32_t width = text_width + 2*padding;

        if (e->side == SIDE_RIGHT) {
            sep_x = prev_x_right - (gap / 2) - sep_w;
            x = prev_x_right - width - MAX(gap, sep_w);
            prev_x_right = x;
        } else if (e->side == SIDE_LEFT) {
            // 'prev' actually means next here
            x = prev_x_left + MAX(sep_w, gap);
            prev_x_left = x + width;
            sep_x = x - (gap / 2) - sep_w;
        }

        // background
        cr_set_argb_color(cr, e->bg_color);
        cairo_rectangle(cr, x, 0, width, bar_height);
        cairo_fill(cr);

        // text
        cr_set_argb_color(cr, e->fg_color);
        draw_text(cr, layout, width, bar_height, x, bar->g.origin.y);
        g_object_unref(layout);

        // separator
        if (config->statusbar_separator_enabled &&
            !(first_right && e->side == SIDE_RIGHT)) {

            cr_set_argb_color(cr, config->statusbar_separator_color);

            // adding 0.5 makes cairo render the line sharply
            cairo_move_to(cr, sep_x + 0.5, sep_y);
            cairo_line_to(cr, sep_x + 0.5, sep_y + sep_h);

            cairo_set_line_width(cr, sep_w);
            cairo_stroke(cr);
            cairo_fill(cr);
        }

        if (e->side == SIDE_RIGHT) {
            first_right = false;
        } else if (e->side == SIDE_LEFT) {
            first_left = false;
        }
    }
}

static void alloc_bar(struct output *out) {
    int stride = 4 * out->bar.g.size.w;

    // front buffer
    out->bar.front->buffer = calloc(stride * out->bar.g.size.h,
            sizeof(unsigned char));
    out->bar.front->surface = cairo_image_surface_create_for_data(
            out->bar.front->buffer, CAIRO_FORMAT_ARGB32, out->bar.g.size.w,
            out->bar.g.size.h, stride);
    out->bar.front->cr = cairo_create(out->bar.front->surface);

    // back buffer
    out->bar.back->buffer = calloc(stride * out->bar.g.size.h,
            sizeof(unsigned char));
    out->bar.back->surface = cairo_image_surface_create_for_data(
            out->bar.back->buffer, CAIRO_FORMAT_ARGB32, out->bar.g.size.w,
            out->bar.g.size.h, stride);
    out->bar.back->cr = cairo_create(out->bar.back->surface);
}

void update_bar(struct output *out) {
    // prevent concurrent use of cairo objects
    pthread_mutex_lock(&out->bar.draw_lock);

    // reallocate the bar if the dirty bit indicates a change of the bar size
    if (out->bar.dirty) {
        out->bar.g.origin.x = 0;
        out->bar.g.origin.y = (config->statusbar_position == POS_TOP) ? 0 :
                                out->g.size.h;
        out->bar.g.size.w = out->g.size.w;
        out->bar.g.size.h = config->statusbar_height;
        if (out->bar.front->buffer) {
            free(out->bar.front->buffer);
        }
        if (out->bar.back->buffer) {
            free(out->bar.back->buffer);
        }
        if (out->bar.front->surface) {
            cairo_surface_destroy(out->bar.front->surface);
            cairo_destroy(out->bar.front->cr);
        }
        if (out->bar.back->surface) {
            cairo_surface_destroy(out->bar.back->surface);
            cairo_destroy(out->bar.back->cr);
        }
        alloc_bar(out);
        out->bar.dirty = false;

        // when drawing over other stuff, replace the destination layer.
        // this means transparent elements like background/workspace aren't
        // composed, the color/transparency of the last drawn layer is applied.
        cairo_set_operator(out->bar.front->cr, CAIRO_OPERATOR_SOURCE);
        cairo_set_operator(out->bar.back->cr, CAIRO_OPERATOR_SOURCE);
    }

    // background
    cr_set_argb_color(out->bar.back->cr, config->statusbar_bg_color);
    cairo_paint(out->bar.back->cr);

    // workspaces
    draw_workspace_indicators(out, out->bar.back->cr);

    // user defined statusbar elements
    draw_data(&out->bar, out->bar.back->cr);

    cairo_surface_flush(out->bar.back->surface);

    // swap the front/back buffer
    struct bar_buffer *tmp = out->bar.back;
    out->bar.back = out->bar.front;
    out->bar.front = tmp;

    pthread_mutex_unlock(&out->bar.draw_lock);
}

void render_bar(struct output *out) {
    if (!out || !out->bar.front->buffer) {
        return;
    }
    wlc_pixels_write(WLC_RGBA8888, &out->bar.g, out->bar.front->buffer);
}

void init_bar_config() {
    status_entries = vector_init();
}

static void *hook_thread_slow_func(void *arg) {
    (void) arg;
    while (1) {
        trigger_hook(HOOK_PERIODIC_SLOW);
        sleep(30);
    }
    return NULL;
}

static void *hook_thread_fast_func(void *arg) {
    (void) arg;
    while (1) {
        trigger_hook(HOOK_PERIODIC_FAST);
        sleep(1);
    }
    return NULL;
}

static void *trigger_all_hooks(void *arg) {
    (void) arg;
    for (uint32_t i = 0; i < 5; i++) {
        trigger_hook(i);
    }
    return NULL;
}

void init_bar_threads() {
    pthread_create(&hook_thread_fast, NULL, hook_thread_fast_func, NULL);
    pthread_create(&hook_thread_slow, NULL, hook_thread_slow_func, NULL);
}

void init_bar(struct output *out) {
    out->bar.dirty = true; // buffers will be allocated on next update
    pthread_mutex_init(&out->bar.draw_lock, NULL);
    out->bar.front = calloc(1, sizeof(struct bar_buffer));
    if (!out->bar.front) {
        wavy_log(LOG_ERROR, "Failed to allocate bar_buffer struct");
        exit(EXIT_FAILURE);
    }
    out->bar.back = calloc(1, sizeof(struct bar_buffer));
    if (!out->bar.front) {
        free(out->bar.front);
        wavy_log(LOG_ERROR, "Failed to allocate bar_buffer struct");
        exit(EXIT_FAILURE);
    }

    // trigger all the hooks once on initialization. use a separate thread
    // so startup isn't blocked by a slow script.
    pthread_t init_hooks_thread;
    pthread_create(&init_hooks_thread, NULL, trigger_all_hooks, NULL);
}

void free_bar(struct bar_t *bar) {
    for (uint32_t i = 0; i < status_entries->length; i++) {
        struct status_entry_t *e = status_entries->items[i];
        if (e->entry) {
            free(e->entry);
        }
        free(e);
    }
    vector_free(status_entries);

    cairo_surface_destroy(bar->front->surface);
    cairo_surface_destroy(bar->back->surface);
    cairo_destroy(bar->front->cr);
    cairo_destroy(bar->back->cr);
    free(bar->front->buffer);
    free(bar->back->buffer);
}

void stop_bar_threads() {
    pthread_cancel(hook_thread_slow);
    pthread_cancel(hook_thread_fast);
}
