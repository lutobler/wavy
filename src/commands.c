#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <wlc/wlc.h>

#include "vector.h"
#include "commands.h"
#include "layout.h"
#include "log.h"
#include "utils.h"

/*
 * A *_cmd function will be called (via a pointer to it in the keybind_t struct)
 * when a keybinding is pressed by the user. They are registered by calling
 * lua functions implemented in windy.c.
 */

bool close_view_cmd(struct keybind_t *kb, wlc_handle view) {
    (void) kb;
    if (view) {
        wlc_view_close(view);
    }
    return true;
}

bool exit_cmd(struct keybind_t *kb, wlc_handle view) {
    (void) view; (void) kb;
    wavy_log(LOG_WAVY, "Wavy is being terminated ...");
    wlc_terminate();
    return true;
}

bool spawn_cmd(struct keybind_t *kb, wlc_handle view) {
    (void) view;
    char *const *str_arr = (char *const *) kb->args.ptr;
    cmd_exec(str_arr[0], str_arr);
    return true;
}

bool lua_cmd(struct keybind_t *kb, wlc_handle view) {
    (void) view;
    lua_rawgeti(L_config, LUA_REGISTRYINDEX, kb->args.num);
    lua_call(L_config, 0, 0);
    return true;
}

bool cycle_tiling_mode_cmd(struct keybind_t *kb, wlc_handle view) {
    (void) view; (void) kb;
    wavy_log(LOG_DEBUG, "Cycling tiling mode");
    cycle_tiling_mode();
    return true;
}

bool cycle_view_cmd(struct keybind_t *kb, wlc_handle view) {
    (void) view;
    cycle_view_in_frame(kb->args.num); // 1 := forward, 0 := backward
    return true;
}

bool new_frame_cmd(struct keybind_t *kb, wlc_handle view) {
    (void) view;
    frame_add(kb->args.dir);
    return true;
}

bool delete_frame_cmd(struct keybind_t *kb, wlc_handle view) {
    (void) view; (void) kb;
    frame_delete();
    return true;
}

bool select_cmd(struct keybind_t *kb, wlc_handle view) {
    (void) view;
    focus_direction(kb->args.dir);
    return true;
}

bool resize_cmd(struct keybind_t *kb, wlc_handle view) {
    (void) view;
    wavy_log(LOG_DEBUG, "Resizing by %f", kb->args.f);
    frame_resize_percent(kb->args.dir, kb->args.f);
    return true;
}

bool switch_workspace_cmd(struct keybind_t *kb, wlc_handle view) {
    (void) view;
    workspace_switch_to(kb->args.num - 1);
    return true;
}

bool prev_workspace_cmd(struct keybind_t *kb, wlc_handle view) {
    (void) view; (void) kb;
    workspace_prev();
    return true;
}

bool next_workspace_cmd(struct keybind_t *kb, wlc_handle view) {
    (void) view; (void) kb;
    workspace_next();
    return true;
}

bool move_to_ws_cmd(struct keybind_t *kb, wlc_handle view) {
    (void) view;
    move_to_workspace(kb->args.num - 1);
    return true;
}

bool add_ws_cmd(struct keybind_t *kb, wlc_handle view) {
    (void) view; (void) kb;
    workspace_add();
    return true;
}

bool move_cmd(struct keybind_t *kb, wlc_handle view) {
    (void) view;
    move_direction(kb->args.dir);
    return true;
}

void init_commands() {
    commands = vector_init();
}

void free_commands() {
    for (uint32_t i = 0; i < commands->length; i++) {
        struct keybind_t *kb = commands->items[i];
        free(kb);
    }

    vector_free(commands);
}

void cmd_update(uint32_t mods, uint32_t keysym, struct keybind_arg_t args,
        bool (*keybind_f) (struct keybind_t * args, wlc_handle view)) {

    // if the keysym/mods combination is already bound, we update it
    for (uint32_t i = 0; i < commands->length; i++) {
        struct keybind_t *kb_i =  (struct keybind_t *) (commands->items[i]);
        if (kb_i->keysym == keysym && kb_i->mods == mods) {
            kb_i->args = args;
            kb_i->keybind_f = keybind_f;
            return;
        }
    }

    // otherwise create a new keybinding
    struct keybind_t *new_kb = malloc(sizeof(struct keybind_t));
    if (!new_kb) {
        return;
    }

    new_kb->keysym = keysym;
    new_kb->mods = mods;
    new_kb->args = args;
    new_kb->keybind_f = keybind_f;
    vector_add(commands, new_kb);
}

bool eval_keypress(wlc_handle view, uint32_t mods, uint32_t keysym) {
    wavy_log(LOG_DEBUG, "Keypress: keysym = 0x%x, modifiers = 0x%x",
            keysym, mods);

    struct keybind_t *kb = NULL;
    for (uint32_t i = 0; i < commands->length; i++) {
        struct keybind_t *kb_i =  (struct keybind_t *) (commands->items[i]);
        if (kb_i->keysym == keysym && kb_i->mods == mods) {
            kb = kb_i;
            break;
        }
    }

    if (!kb) {
        return false;
    }

    return kb->keybind_f(kb, view);
}
