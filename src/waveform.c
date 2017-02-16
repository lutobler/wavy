#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <xkbcommon/xkbcommon.h>

#include "commands.h"
#include "bar.h"
#include "log.h"
#include "utils.h"
#include "layout.h"

static struct keybind_arg_t kb_null = {0, 0, 0, 0, 0};

static uint32_t string_to_mod(const char *str) {
    if (!str) {
        return 0;
    }

    if (!strcmp(str, "shift")) {
        return WLC_BIT_MOD_SHIFT;
    } else if (!strcmp(str, "super")) {
        return WLC_BIT_MOD_LOGO;
    } else if (!strcmp(str, "alt")) {
        return WLC_BIT_MOD_ALT;
    } else if (!strcmp(str, "ctrl")) {
        return WLC_BIT_MOD_CTRL;
    } else if (!strcmp(str, "caps")) {
        return WLC_BIT_MOD_CAPS;
    } else if (!strcmp(str, "mod2")) {
        return WLC_BIT_MOD_MOD2;
    } else if (!strcmp(str, "mod3")) {
        return WLC_BIT_MOD_MOD3;
    } else if (!strcmp(str, "mod5")) {
        return WLC_BIT_MOD_MOD5;
    } else {
        return 0;
    }
}

/*
 * Checks if there are the right number of arguments on the virtual lua stack
 * and if not, terminates with a lua error.
 */
static void check_argc(lua_State *L, uint32_t argc, const char *func_name) {
    uint32_t stacksize = lua_gettop(L);
    if (stacksize != argc) {
        luaL_error(L, "config.lua: wrong number of arguments supplied to %s",
                    func_name);
    }
}

/*
 * The first argument when registering a keybinding is always a table.
 * This turns all the strings in the table into a WLC_BIT_MOD_* bit pattern.
 */
static uint32_t get_mod(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE); // stack position no. 1: modifier table
    uint32_t mod = 0;
    uint32_t len = lua_rawlen(L, 1);
    for (uint32_t i = 0; i < len; i++) {
        lua_geti(L, 1, i+1);
        mod |= string_to_mod(lua_tostring(L, -1));
    }
    return mod;
}

static uint32_t get_sym(lua_State *L) {
    luaL_checktype(L, 2, LUA_TSTRING);
    return xkb_keysym_from_name(lua_tostring(L, 2),
                                XKB_KEYSYM_CASE_INSENSITIVE);
}

static enum direction_t get_dir(lua_State *L, int32_t index) {
    luaL_checktype(L, index, LUA_TSTRING);
    enum direction_t dir;
    const char *str = lua_tostring(L, index);
    if (!strcmp(str, "left")) {
        dir = DIR_LEFT;
    } else if (!strcmp(str, "right")) {
        dir = DIR_RIGHT;
    } else if (!strcmp(str, "up")) {
        dir = DIR_UP;
    } else {
        dir = DIR_DOWN;
    }
    return dir;
}

static uint32_t get_num(lua_State *L, int32_t index) {
    luaL_checktype(L, index, LUA_TSTRING);
    return lua_tointeger(L, index);
}

static float get_float(lua_State *L, int32_t index) {
    luaL_checktype(L, index, LUA_TNUMBER);
    return lua_tonumber(L, index);
}

// store a function in the lua registry so it can be called later
static uint32_t reg_lua_function(lua_State *L, int32_t index) {
    luaL_checktype(L, index, LUA_TFUNCTION);
    lua_settop(L, index);
    return luaL_ref(L, LUA_REGISTRYINDEX);
}

static int register_exit_cmd(lua_State *L) {
    check_argc(L, 2, "kb_exit");
    cmd_update(get_mod(L), get_sym(L), kb_null, exit_cmd);
    return 1;
}

static int register_lua_cmd(lua_State *L) {
    check_argc(L, 3, "kb_lua");
    struct keybind_arg_t kba = kb_null;
    kba.num = reg_lua_function(L, 3);
    cmd_update(get_mod(L), get_sym(L), kba, lua_cmd);
    return 1;
}

static int register_spawn_cmd(lua_State *L) {
    check_argc(L, 3, "kb_spawn");
    struct keybind_arg_t kba = kb_null;
    kba.ptr = (void **) table_to_str_array(L, 3);
    cmd_update(get_mod(L), get_sym(L), kba, spawn_cmd);
    return 1;
}

static int register_cycle_workspace_cmd(lua_State *L) {
    check_argc(L, 3, "kb_cylce_workspace");
    luaL_checktype(L, 3, LUA_TSTRING);
    const char *next_bkwd = lua_tostring(L, 3);
    struct keybind_arg_t kba = kb_null;
    if (!strcmp(next_bkwd, "previous")) {
        kba.num = 0;
    } else { // default to next
        kba.num = 1;
    }
    cmd_update(get_mod(L), get_sym(L), kba, cycle_workspace_cmd);
    return 1;
}

static int register_new_frame_right_cmd(lua_State *L) {
    check_argc(L, 2, "kb_new_frame_right");
    struct keybind_arg_t kba = kb_null;
    kba.dir = DIR_RIGHT;
    cmd_update(get_mod(L), get_sym(L), kba, new_frame_cmd);
    return 1;
}

static int register_new_frame_down_cmd(lua_State *L) {
    check_argc(L, 2, "kb_new_frame_down");
    struct keybind_arg_t kba = kb_null;
    kba.dir = DIR_DOWN;
    cmd_update(get_mod(L), get_sym(L), kba, new_frame_cmd);
    return 1;
}

static int register_select_cmd(lua_State *L) {
    check_argc(L, 3, "kb_select");
    struct keybind_arg_t kba = kb_null;
    kba.dir = get_dir(L, 3);
    cmd_update(get_mod(L), get_sym(L), kba, select_cmd);
    return 1;
}

static int register_move_cmd(lua_State *L) {
    check_argc(L, 3, "kb_move");
    struct keybind_arg_t kba = kb_null;
    kba.dir = get_dir(L, 3);
    cmd_update(get_mod(L), get_sym(L), kba, move_cmd);
    return 1;
}

static int register_close_view_cmd(lua_State *L) {
    check_argc(L, 2, "kb_close_view");
    cmd_update(get_mod(L), get_sym(L), kb_null, close_view_cmd);
    return 1;
}

static int register_delete_frame_cmd(lua_State *L) {
    check_argc(L, 2, "kb_delete_frame");
    cmd_update(get_mod(L), get_sym(L), kb_null, delete_frame_cmd);
    return 1;
}

static int register_cycle_tiling_mode_cmd(lua_State *L) {
    check_argc(L, 2, "kb_cylce_tiling_mode");
    cmd_update(get_mod(L), get_sym(L), kb_null, cycle_tiling_mode_cmd);
    return 1;
}

static int register_cycle_view_cmd(lua_State *L) {
    check_argc(L, 3, "kb_cylce_view");
    luaL_checktype(L, 3, LUA_TSTRING);
    const char *next_bkwd = lua_tostring(L, 3);
    struct keybind_arg_t kba = kb_null;
    if (!strcmp(next_bkwd, "previous")) {
        kba.num = 0;
    } else { // default to next
        kba.num = 1;
    }
    cmd_update(get_mod(L), get_sym(L), kba, cycle_view_cmd);
    return 1;
}

static int register_switch_to_workspace_cmd(lua_State *L) {
    check_argc(L, 2, "kb_select_workspace");
    struct keybind_arg_t kba = kb_null;
    kba.num = get_num(L, 2);
    cmd_update(get_mod(L), get_sym(L), kba, switch_workspace_cmd);
    return 1;
}

static int register_move_to_ws_cmd(lua_State *L) {
    check_argc(L, 2, "kb_move_to_workspace");
    struct keybind_arg_t kba = kb_null;
    kba.num = get_num(L, 2);
    cmd_update(get_mod(L), get_sym(L), kba, move_to_ws_cmd);
    return 1;
}

static int register_resize_cmd(lua_State *L) {
    check_argc(L, 4, "kb_resize");
    struct keybind_arg_t kba = kb_null;
    kba.dir = get_dir(L, 3);
    kba.f = get_float(L, 4);
    cmd_update(get_mod(L), get_sym(L), kba, resize_cmd);
    return 1;
}

static int register_add_ws_cmd(lua_State *L) {
    check_argc(L, 2, "kb_add_workspace");
    cmd_update(get_mod(L), get_sym(L), kb_null, add_ws_cmd);
    return 1;
}

static int get_tiling_symbol(lua_State *L) {
    struct frame *fr = get_active_frame();
    if (fr) {
        char *str = config->tile_layout_strs[fr->tile];
        lua_pushstring(L, str);
        return 1;
    }
    lua_pushstring(L, "");
    return 1;
}

static int get_view_title(lua_State *L) {
    wlc_handle v = get_active_view();
    if (v) {
        const char *str = wlc_view_get_title(v);
        lua_pushstring(L, str);
        return 1;
    }
    lua_pushstring(L, "");
    return 1;
}

static int trigger_hook_lua(lua_State *L) {
    if (lua_type(L, -1)) {
        enum hook_t h = hook_str_to_enum(lua_tostring(L, -1));
        if (h != HOOK_UNKNOWN) {
            trigger_hook(h);
        }
    }
    return 1;
}

int luaopen_libwaveform(lua_State *L) {
    // functions to register keybindings
    lua_register(L, "kb_exit", register_exit_cmd);
    lua_register(L, "kb_lua", register_lua_cmd);
    lua_register(L, "kb_spawn", register_spawn_cmd);
    lua_register(L, "kb_cycle_workspace", register_cycle_workspace_cmd);
    lua_register(L, "kb_new_frame_right", register_new_frame_right_cmd);
    lua_register(L, "kb_new_frame_down", register_new_frame_down_cmd);
    lua_register(L, "kb_select", register_select_cmd);
    lua_register(L, "kb_move", register_move_cmd);
    lua_register(L, "kb_close_view", register_close_view_cmd);
    lua_register(L, "kb_delete_frame", register_delete_frame_cmd);
    lua_register(L, "kb_cycle_tiling_mode", register_cycle_tiling_mode_cmd);
    lua_register(L, "kb_cycle_view", register_cycle_view_cmd);
    lua_register(L, "kb_select_workspace", register_switch_to_workspace_cmd);
    lua_register(L, "kb_move_to_workspace", register_move_to_ws_cmd);
    lua_register(L, "kb_resize", register_resize_cmd);
    lua_register(L, "kb_add_workspace", register_add_ws_cmd);

    // statusbar related functions
    lua_register(L, "get_tiling_symbol", get_tiling_symbol);
    lua_register(L, "get_view_title", get_view_title);
    lua_register(L, "trigger_hook", trigger_hook_lua);
    return 0;
}
