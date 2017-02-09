#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <cairo/cairo.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "bar.h"
#include "utils.h"
#include "log.h"

void cr_set_argb_color(cairo_t *cr, uint32_t rgba) {
    cairo_set_source_rgba(cr, (double) ((rgba >> 8)  & 0xff) / 255.0,
                              (double) ((rgba >> 16) & 0xff) / 255.0,
                              (double) ((rgba >> 24) & 0xff) / 255.0,
                              (double) ((rgba >> 0)  & 0xff) / 255.0);
}

const char **table_to_str_array(lua_State *L, int32_t index) {
    luaL_checktype(L, index, LUA_TTABLE);
    uint32_t len = lua_rawlen(L, index);
    const char **str_arr = malloc(sizeof(const char *) * len + 1);

    for (uint32_t i = 0; i < len; i++) {
        lua_geti(L, index, i+1);
        const char *stack_str = lua_tostring(L, -1);

        // the lua runtime will garbage collect the original string
        str_arr[i] = strdup(stack_str);
        lua_pop(L, 1);
    }

    // wlc_exec expects a terminating NULL pointer
    str_arr[len] = NULL;

    return str_arr;
}

enum hook_t hook_str_to_enum(const char *str) {
    enum hook_t hook;

    if (!strcmp(str, "hook_periodic_fast")) {
        hook = HOOK_PERIODIC_FAST;
    } else if (!strcmp(str, "hook_periodic_slow")) {
        hook = HOOK_PERIODIC_SLOW;
    } else if (!strcmp(str, "hook_view_update")) {
        hook = HOOK_VIEW_UPDATE;
    } else if (!strcmp(str, "hook_user")) {
        hook = HOOK_USER;
    } else {
        hook = HOOK_UNKNOWN;
    }

    return hook;
}

enum auto_tile_t tiling_layout_str_to_enum(const char *str) {
    enum auto_tile_t tile;
    if (!strcmp(str, "vertical")) {
        tile = TILE_VERTICAL;
    } else if (!strcmp(str, "horizontal")) {
        tile = TILE_HORIZONTAL;
    } else if (!strcmp(str, "grid")) {
        tile = TILE_GRID;
    } else if (!strcmp(str, "fullscreen")) {
        tile = TILE_FULLSCREEN;
    } else if (!strcmp(str, "fibonacci")) {
        tile = TILE_FIBONACCI;
    } else {
        tile = TILE_UNKNOWN;
    }

    return tile;
}

// adapted from wlc_exec, supposed to be drop-in replacement
void cmd_exec(const char *bin, char *const *args) {
    assert(bin && args && (bin == args[0]));
    wavy_log(LOG_DEBUG, "Spawning \"%s\"", bin);

    uint32_t len;
    for (len = 0; args[len]; len++);
    char **argv = malloc(sizeof(char *const) * (len + 3));
    if (!argv) {
        wavy_log(LOG_ERROR, "Failed to allocate memory for spawning commands");
    }
    memcpy(argv+2, args, len * sizeof(char *const *));
    argv[0] = "/bin/sh";
    argv[1] = "-c";
    argv[len+2] = NULL;

    pid_t p;
    if ((p = fork()) == 0) {
        setsid();
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        execvp("/bin/sh", (char *const *) argv);
        _exit(EXIT_FAILURE);
    } else if (p < 0) {
        wavy_log(LOG_ERROR, "Failed to fork for \'%s\'", bin);
    }
    free(argv);
}
