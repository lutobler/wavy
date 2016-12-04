#ifndef __COMMANDS_H
#define __COMMANDS_H
#include <stdbool.h>
#include <stdint.h>
#include <wlc/wlc.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "vector.h"
#include "layout.h"

struct keybind_arg_t {
    void **ptr;
    uint32_t count_ptr; // used to free ptr
    enum direction_t dir;
    uint32_t num;
    float f;
};

struct keybind_t {
	uint32_t keysym;
	uint32_t mods;
    bool (*keybind_f) (struct keybind_t * args, wlc_handle view);
    struct keybind_arg_t args;
};

extern bool debug_enabled;
extern lua_State *L_config;

static struct vector_t *commands;

// Commands that can be bound to a keypress
bool close_view_cmd(struct keybind_t *kb, wlc_handle view);
bool exit_cmd(struct keybind_t *kb, wlc_handle view);
bool spawn_cmd(struct keybind_t *kb, wlc_handle view);
bool lua_cmd(struct keybind_t *kb, wlc_handle view);
bool cycle_tiling_mode_cmd(struct keybind_t *kb, wlc_handle view);
bool cycle_view_cmd(struct keybind_t *kb, wlc_handle view);
bool new_frame_cmd(struct keybind_t *kb, wlc_handle view);
bool delete_frame_cmd(struct keybind_t *kb, wlc_handle view);
bool select_cmd(struct keybind_t *kb, wlc_handle view);
bool resize_cmd(struct keybind_t *kb, wlc_handle view);
bool switch_workspace_cmd(struct keybind_t *kb, wlc_handle view);
bool prev_workspace_cmd(struct keybind_t *kb, wlc_handle view);
bool next_workspace_cmd(struct keybind_t *kb, wlc_handle view);
bool move_to_ws_cmd(struct keybind_t *kb, wlc_handle view);
bool add_ws_cmd(struct keybind_t *kb, wlc_handle view);
bool move_cmd(struct keybind_t *kb, wlc_handle view);

void init_commands();
void free_commands();

// inserts/updates commands in the command list.
void cmd_update(uint32_t mods, uint32_t keysym, struct keybind_arg_t args,
        bool (*keybind_f) (struct keybind_t * args, wlc_handle view));

// gets called every time a key is pressed.
bool eval_keypress(wlc_handle view, uint32_t mods, uint32_t keysym);

#endif
