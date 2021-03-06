#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <cairo/cairo.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <wlc/wlc.h>

#include "bar.h"
#include "utils.h"
#include "log.h"
#include "layout.h"

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
        luaL_checktype(L, -1, LUA_TSTRING);
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
    uint32_t bytes = 0;
    for (len = 0; args[len]; len++) {
        bytes += strlen(args[len]);
    }
    size_t size = bytes + len;

    char *cmd = malloc(size);
    if (!cmd) {
        wavy_log(LOG_ERROR, "Failed to allocate memory for spawning commands");
    }
    uint32_t c = 0;
    for (uint32_t i = 0; i < len; i++) {
        strcpy(cmd+c, args[i]);
        c += strlen(args[i]);
        cmd[c++] = ' ';
    }
    cmd[size - 1] = 0;

    pid_t p;
    if ((p = fork()) == 0) {
        setsid();
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        execl("/bin/sh", "/bin/sh", "-c", cmd, NULL);
        _exit(EXIT_FAILURE);
    } else if (p < 0) {
        wavy_log(LOG_ERROR, "Failed to fork for \'%s\'", bin);
    }
    free(cmd);
}

// internal recursive function that prints the frame tree
static void _print_frame_tree(struct frame *fr, int indent) {
    if (fr) {
        if (fr->right) {
            _print_frame_tree(fr->right, indent + 4);
        }
        if (indent) {
            for (int i=0; i<indent; i++) {
                printf(" ");
            }
        }
        if (fr->right) {
            printf(" /\n");
            for (int i=0; i<indent; i++) {
                printf(" ");
            }
        }
        if (fr->split == SPLIT_HORIZONTAL) {
            printf("H (%p)\n", (void *) fr);
        } else if (fr->split == SPLIT_VERTICAL) {
            printf("V (%p)\n", (void *) fr);
        } else {
            printf("(%p)\n", (void *) fr);
        }
        if (fr->left) {
            for (int i=0; i<indent; i++) {
                printf(" ");
            }
            printf(" \\\n");
            _print_frame_tree(fr->left, indent + 4);
        }
    }
}

void print_frame_tree(struct frame *fr) {
    printf("\nCurrent frame tree (printed sideways): \n\n");
    _print_frame_tree(fr, 0);
    printf("\n");
}

void print_wlc_geometry(struct wlc_geometry *g) {
    printf("x = %u, y = %u, w = %u, h = %u\n", g->origin.x, g->origin.y,
            g->size.w, g->size.h);
}
