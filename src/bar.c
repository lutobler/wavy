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

static void draw_workspace_indicators(struct output *out) {
    cairo_t *cr = out->bar.cr;
    struct vector_t *workspaces = get_workspaces();
    uint32_t ws_rect_width = 20; // FIXME: don't hardcode this
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

static void draw_data(struct bar_t *bar) {
    if (!status_entries) { // might not be initialized yet
        return;
    }

    cairo_t *cr = bar->cr;
    uint32_t bar_height = config->statusbar_height;
    uint32_t padding = config->statusbar_padding;
    uint32_t gap = config->statusbar_gap;
    uint32_t prev_x_right = bar->g.size.w + gap;
    uint32_t prev_x_left = (get_workspaces())->length * 20 + gap; // FIXME dont hardcode this

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
            x = prev_x_right - width - gap;
            prev_x_right = x;
        } else if (e->side == SIDE_LEFT) {
            x = prev_x_left;
            prev_x_left = prev_x_left + width + gap;
        }

        // background
        cr_set_argb_color(cr, e->bg_color);
        cairo_rectangle(cr, x, 0, width, bar_height);
        cairo_fill(cr);

        // text
        cr_set_argb_color(cr, e->fg_color);
        draw_text(cr, layout, width, bar_height, x, bar->g.origin.y);
        g_object_unref(layout);
    }
}

static void alloc_bar(struct output *out) {
    int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32,
            out->g.size.w);
    out->bar.buffer = calloc(stride * out->g.size.h, sizeof(unsigned char));
    out->bar.surface = cairo_image_surface_create_for_data(out->bar.buffer,
                                                        CAIRO_FORMAT_ARGB32,
                                                        out->g.size.w,
                                                        out->g.size.h,
                                                        stride);
    out->bar.cr = cairo_create(out->bar.surface);
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
        free(out->bar.buffer);
        cairo_surface_destroy(out->bar.surface);
        cairo_destroy(out->bar.cr);
        alloc_bar(out);
        out->bar.dirty = false;
    }

    // background
    cr_set_argb_color(out->bar.cr, config->statusbar_bg_color);
    cairo_paint(out->bar.cr);

    // workspaces
    draw_workspace_indicators(out);

    // user defined statusbar elements
    draw_data(&out->bar);

    cairo_surface_flush(out->bar.surface);
    pthread_mutex_unlock(&out->bar.draw_lock);
}

void render_bar(struct output *out) {
    if (!out || !out->bar.buffer) {
        return;
    }
    wlc_pixels_write(WLC_RGBA8888, &out->bar.g, out->bar.buffer);
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
    out->bar.dirty = true;
    pthread_mutex_init(&out->bar.draw_lock, NULL);
    alloc_bar(out);

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

    cairo_surface_destroy(bar->surface);
    cairo_destroy(bar->cr);
    free(bar->buffer);
}

void stop_bar_threads() {
    pthread_cancel(hook_thread_slow);
    pthread_cancel(hook_thread_fast);
}
