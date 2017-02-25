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
    // statusbar related functions
    lua_register(L, "get_tiling_symbol", get_tiling_symbol);
    lua_register(L, "get_view_title", get_view_title);
    lua_register(L, "trigger_hook", trigger_hook_lua);
    return 0;
}
