#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <wlc/wlc.h>

#include "log.h"
#include "callbacks.h"
#include "commands.h"
#include "border.h"
#include "layout.h"
#include "bar.h"
#include "wallpaper.h"
#include "utils.h"

/*
 * Return true if window should be managed by wavy and false otherwise (for
 * popups, splash screens etc.). See wlc_view_type_bit.
 */
inline static bool is_managed(wlc_handle view) {
    return !(wlc_view_get_type(view) & 0xf);
}

/*
 * "key" is a keycode. We use keysyms internally for keybindings for now, but
 * will (hopefully) support keybindings with keycodes as well in the future.
 */
static bool handle_key(wlc_handle view, uint32_t time,
        const struct wlc_modifiers *modifiers, uint32_t key,
        enum wlc_key_state state) {

    (void) time;
    const uint32_t sym = wlc_keyboard_get_keysym_for_key(key, NULL);

    if (state == WLC_KEY_STATE_PRESSED) {
        return eval_keypress(view, modifiers->mods, sym);
    }
    return false;
}

static bool view_created(wlc_handle view) {
    wavy_log(LOG_DEBUG, "View (handle = %lu, type = %u) created.", view,
            wlc_view_get_type(view));

    if (!is_managed(view)) {
        wlc_view_set_mask(view, wlc_output_get_mask(wlc_view_get_output(view)));
        wlc_view_focus(view);
        wlc_view_bring_to_front(view);
        return true;
    }

    child_add(view);
    return true;
}

static void view_destroyed(wlc_handle view) {
    if (is_managed(view)) {
        child_delete(view);
    } else {
        wlc_view_focus(get_active_view());
    }
}

static void view_properties_updated(wlc_handle view, uint32_t mask) {
    (void) mask; (void) view;
    trigger_hook(HOOK_VIEW_UPDATE);
}

static bool output_created(wlc_handle output) {
    add_output(output);
    return true;
}

static void output_destroyed(wlc_handle output) {
    delete_output(output);
}

static void output_resolution(wlc_handle output, const struct wlc_size *from,
        const struct wlc_size *to) {

    (void) from;
    output_set_resolution(output, to);
}

static bool pointer_motion(wlc_handle view, uint32_t time,
        const struct wlc_point *new_point) {

    (void) view;
    (void) time;
    wlc_pointer_set_position(new_point);

    return false;
}

static void view_focus(wlc_handle view, bool focus) {
    wlc_view_set_state(view, WLC_BIT_ACTIVATED, focus);
	return;
}

// wlc gives view = 0 every time at the moment.
static bool pointer_button(wlc_handle view, uint32_t time,
        const struct wlc_modifiers *mods,
        uint32_t button, enum wlc_button_state state,
        const struct wlc_point *point) {

    (void) time;
    (void) mods;
    (void) button;

    wavy_log(LOG_WAVY, "Pointer button event on view %lu at %ux%u",
            view, point->x, point->y);

    if (state == WLC_BUTTON_STATE_PRESSED) {
        focus_view(view);
        return false;
    } else {
        return false;
    }
}

// wlc sends geometry requests after we already set it, so we ignore it.
static void view_request_geometry(wlc_handle view,
        const struct wlc_geometry *g) {

    if (!is_managed(view)) {
        wavy_log(LOG_DEBUG, "Applying requested geometry to view %lu", view);
        wlc_view_set_geometry(view, 0, g);
    }
}

static void output_render_pre(wlc_handle output) {
    struct output *out = get_output_by_handle(output);
    render_wallpaper(out);
    render_frame_borders(out->active_ws->root_frame);
    render_bar(out);
}

static void compositor_ready() {
    // run autostart programs
    for (uint32_t i = 0; i < config->autostart->length; i++) {
        char *const *str_arr = (char *const *) config->autostart->items[i];
        cmd_exec(str_arr[0], str_arr);
    }
}

static void wlc_log(enum wlc_log_type type, const char *str) {
    (void) type;

    wavy_log(LOG_WLC, (char *) str);
}

void set_wlc_callbacks() {
    wlc_set_keyboard_key_cb(handle_key);
    wlc_set_view_created_cb(view_created);
    wlc_set_view_destroyed_cb(view_destroyed);
    wlc_set_view_properties_updated_cb(view_properties_updated);
    wlc_set_output_created_cb(output_created);
    wlc_set_output_destroyed_cb(output_destroyed);
    wlc_set_output_resolution_cb(output_resolution);
    wlc_set_view_request_geometry_cb(view_request_geometry);
    wlc_log_set_handler(wlc_log);
    wlc_set_output_render_pre_cb(output_render_pre);
    wlc_set_view_focus_cb(view_focus);
    wlc_set_pointer_button_cb(pointer_button);
    wlc_set_pointer_motion_cb(pointer_motion);
    wlc_set_compositor_ready_cb(compositor_ready);
}
