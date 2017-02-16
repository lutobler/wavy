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
 * lua functions implemented in lib/waveform.c.
 */

void close_view_cmd(struct keybind_t *kb, wlc_handle view) {
    (void) kb;
    if (view) {
        wlc_view_close(view);
    }
}

void exit_cmd(struct keybind_t *kb, wlc_handle view) {
    (void) view; (void) kb;
    wavy_log(LOG_WAVY, "Wavy is being terminated ...");
    wlc_terminate();
}

void spawn_cmd(struct keybind_t *kb, wlc_handle view) {
    (void) view;
    char *const *str_arr = (char *const *) kb->args.ptr;
    cmd_exec(str_arr[0], str_arr);
}

void lua_cmd(struct keybind_t *kb, wlc_handle view) {
    (void) view;
    pthread_mutex_lock(&lua_lock);
    lua_rawgeti(L_config, LUA_REGISTRYINDEX, kb->args.num);
    int err;
    if ((err = lua_pcall(L_config, 0, 0, 0)) != LUA_OK) {
        wavy_log(LOG_ERROR, "Error %d occured in lua keybinding function", err);
    }
    pthread_mutex_unlock(&lua_lock);
}

void cycle_tiling_mode_cmd(struct keybind_t *kb, wlc_handle view) {
    (void) view; (void) kb;
    wavy_log(LOG_DEBUG, "Cycling tiling mode");
    cycle_tiling_mode();
}

void cycle_view_cmd(struct keybind_t *kb, wlc_handle view) {
    (void) view;
    cycle_view_in_frame(kb->args.num); // 1 := forward, 0 := backward
}

void new_frame_cmd(struct keybind_t *kb, wlc_handle view) {
    (void) view;
    frame_add(kb->args.dir);
}

void delete_frame_cmd(struct keybind_t *kb, wlc_handle view) {
    (void) view; (void) kb;
    frame_delete();
}

void select_cmd(struct keybind_t *kb, wlc_handle view) {
    (void) view;
    focus_direction(kb->args.dir);
}

void resize_cmd(struct keybind_t *kb, wlc_handle view) {
    (void) view;
    wavy_log(LOG_DEBUG, "Resizing by %f", kb->args.f);
    frame_resize_percent(kb->args.dir, kb->args.f);
}

void switch_workspace_cmd(struct keybind_t *kb, wlc_handle view) {
    (void) view;
    workspace_switch_to(kb->args.num - 1);
}

void cycle_workspace_cmd(struct keybind_t *kb, wlc_handle view) {
    (void) view;
    cycle_workspace(kb->args.num);
}

void move_to_ws_cmd(struct keybind_t *kb, wlc_handle view) {
    (void) view;
    move_to_workspace(kb->args.num - 1);
}

void add_ws_cmd(struct keybind_t *kb, wlc_handle view) {
    (void) view; (void) kb;
    workspace_add();
}

void move_cmd(struct keybind_t *kb, wlc_handle view) {
    (void) view;
    move_direction(kb->args.dir);
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
        void (*keybind_f) (struct keybind_t * args, wlc_handle view)) {

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

    kb->keybind_f(kb, view);
    return true;
}
