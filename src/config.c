#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <wordexp.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <wlc/wlc.h>
#include <pthread.h>

#include "commands.h"
#include "config.h"
#include "log.h"
#include "bar.h"
#include "utils.h"

// global config pointer
struct wavy_config_t *config = NULL;

// global lua_State
lua_State *L_config = NULL;

// global mutex that prevents concurrent access to the lua_State.
// needs to be reentrant because a lua function can can call trigger_hook,
// which takes the lock again.
pthread_mutex_t lua_lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

static void default_config() {
    config->frame_gaps_size                     = 5;
    config->frame_border_size                   = 0;
    config->frame_border_empty_size             = 3;
    config->frame_border_active_color           = 0x475b74ff;
    config->frame_border_inactive_color         = 0x475b74ff;
    config->frame_border_empty_active_color     = 0x0c1cffff;
    config->frame_border_empty_inactive_color   = 0x6b6c7fff;

    config->view_border_size                    = 2;
    config->view_border_active_color            = 0x4897cfff;
    config->view_border_inactive_color          = 0x475b74ff;

    config->statusbar_height                    = 17;
    config->statusbar_font                      = "DejaVu Sans";
    config->statusbar_gap                       = 4;
    config->statusbar_padding                   = 10;
    config->statusbar_position                  = POS_TOP;
    config->statusbar_bg_color                  = 0x282828ff;
    config->statusbar_active_ws_color           = 0x70407fff;
    config->statusbar_inactive_ws_color         = 0x404055ff;
    config->statusbar_active_ws_font_color      = 0xffffffff;
    config->statusbar_inactive_ws_font_color    = 0xccccccff;

    for (uint32_t i = 0; i < 5; i++) {
        config->tile_layouts[i] = i;
    }
    config->num_layouts = 5;
}

static void set_conf_int(lua_State *L, const char *name, uint32_t *conf,
        int32_t idx) {

    if (lua_getfield(L, idx, name) == LUA_TNUMBER) {
        *conf = lua_tointeger(L, -1);
    }
    lua_pop(L, 1);
}

static void set_conf_str(lua_State *L, const char *name, char **conf,
        int32_t idx) {

    if (lua_getfield(L, idx, name) == LUA_TSTRING) {
        *conf = strdup(lua_tostring(L, -1));
    }
    lua_pop(L, 1);
}

static void set_layouts(lua_State *L) {
    if (lua_getglobal(L, "layouts") != LUA_TTABLE) {
        return;
    }

    config->num_layouts = lua_rawlen(L, -1);
    for (uint32_t i = 0; i < config->num_layouts; i++) {
        if (lua_geti(L, -1, i+1) == LUA_TTABLE &&
            lua_geti(L, -1, 1) == LUA_TSTRING &&
            lua_geti(L, -2, 2) == LUA_TSTRING) {

            // store the layout symbol
            const char *layout_sym_str = lua_tostring(L, -1);
            config->tile_layout_strs[i] = strdup(layout_sym_str);

            // store the tiling mode
            const char *str = lua_tostring(L, -2);
            config->tile_layouts[i] = tiling_layout_str_to_enum(str);
            lua_pop(L, 3);
        } else {
            luaL_error(L, "Invalid entry in layouts subtable");
        }
    }
    lua_pop(L, 1);
}

static void set_autostart(lua_State *L) {
    if (lua_getglobal(L, "autostart") != LUA_TTABLE) {
        return;
    }

    int32_t len = lua_rawlen(L, -1);
    for (int32_t i = 0; i < len; i++) {
        if (lua_geti(L, -1, i+1) == LUA_TTABLE) {
            const char **strs = table_to_str_array(L, -1);
            vector_add(config->autostart, strs);
            lua_pop(L, 1);
        }
    }
}

static enum position_t pos_str_to_enum(const char *str) {
    enum position_t pos;
    if (!strcmp(str, "top")) {
        pos = POS_TOP;
    } else if (!strcmp(str, "bottom")) {
        pos = POS_BOTTOM;
    } else {
        pos = POS_UNKNOWN;
    }
    return pos;
}

static enum side_t side_str_to_enum(const char *str) {
    enum side_t s;
    if (!strcmp(str, "right")) {
        s = SIDE_RIGHT;
    } else if (!strcmp(str, "left")) {
        s = SIDE_LEFT;
    } else {
        s = SIDE_UNKNOWN;
    }
    return s;
}

static void bar_config(lua_State *L) {
    if (lua_getglobal(L, "bar") != LUA_TTABLE) {
        return;
    }

    int32_t bar_idx = lua_gettop(L);
    set_conf_int(L, "height", &config->statusbar_height, bar_idx);
    set_conf_str(L, "font", &config->statusbar_font, bar_idx);
    set_conf_int(L, "gap", &config->statusbar_gap, bar_idx);
    set_conf_int(L, "padding", &config->statusbar_padding, bar_idx);

    if (lua_getfield(L, bar_idx, "position") == LUA_TSTRING) {
        enum position_t p = pos_str_to_enum(lua_tostring(L, -1));
        if (p != POS_UNKNOWN) {
            config->statusbar_position = p;
        } else {
            luaL_error(L,
                "Invalid bar position: Only \'top\', \'bottom\' are allowed");
        }
    }
    lua_pop(L, 1);

    if (lua_getfield(L, bar_idx, "colors") == LUA_TTABLE) {
        int32_t colors = lua_gettop(L);
        set_conf_int(L, "background", &config->statusbar_bg_color, colors);
        set_conf_int(L, "active_workspace",
                &config->statusbar_active_ws_color, colors);
        set_conf_int(L, "inactive_workspace",
                &config->statusbar_inactive_ws_color, colors);
        set_conf_int(L, "inactive_workspace_font",
                &config->statusbar_inactive_ws_font_color, colors);
    }

    if (lua_getfield(L, bar_idx, "widgets") == LUA_TTABLE) {
        int32_t widgets = lua_gettop(L);
        uint32_t len = lua_rawlen(L, widgets);

        for (uint32_t i = 0; i < len; i++) {
            if (lua_geti(L, widgets, i+1) == LUA_TTABLE) {
                int ref;
                enum hook_t hook;
                enum side_t side;

                if (lua_geti(L, -1, 1) == LUA_TSTRING &&
                    lua_geti(L, -2, 2) == LUA_TSTRING &&
                    lua_geti(L, -3, 3) == LUA_TFUNCTION) {

                    // register the lua callback function (pops stack)
                    ref = luaL_ref(L, LUA_REGISTRYINDEX);

                    // hook type (see utils.c)
                    const char *hook_str = lua_tostring(L, -1);
                    hook = hook_str_to_enum(hook_str);
                    if (hook == HOOK_UNKNOWN) {
                        luaL_error(L, "Invalid hook type: %s", hook_str);
                    }

                    // side
                    const char *side_str = lua_tostring(L, -2);
                    side = side_str_to_enum(side_str);
                    if (side == SIDE_UNKNOWN) {
                        luaL_error(L, "Invalid placement of bar element: %s",
                                side_str);
                    }

                    add_widget(side, hook, ref);
                    lua_pop(L, 2);
                } else {
                    luaL_error(L, "Invalid entry in widgets subtable");
                }
            }
            lua_pop(L, 1);
        }
        lua_pop(L, 1);
    }
    lua_pop(L, 2);
}

static void read_config(lua_State *L) {
    if (lua_getglobal(L_config, "config") != LUA_TTABLE) {
        return;
    }

    set_conf_int(L, "frame_gaps_size", &config->frame_gaps_size, 1);
    set_conf_int(L, "frame_border_size", &config->frame_border_size, 1);
    set_conf_int(L, "frame_border_empty_size",
            &config->frame_border_empty_size, 1);
    set_conf_int(L, "frame_border_active_color",
            &config->frame_border_active_color, 1);
    set_conf_int(L, "frame_border_inactive_color",
            &config->frame_border_inactive_color, 1);
    set_conf_int(L, "frame_border_empty_active_color",
            &config->frame_border_empty_active_color, 1);
    set_conf_int(L, "frame_border_empty_inactive_color",
            &config->frame_border_empty_inactive_color, 1);

    set_conf_int(L, "view_border_size", &config->view_border_size, 1);
    set_conf_int(L, "view_border_active_color",
            &config->view_border_active_color, 1);
    set_conf_int(L, "view_border_inactive_color",
            &config->view_border_inactive_color, 1);

    set_conf_str(L, "wallpaper", &config->wallpaper, 1);

    set_layouts(L);
    set_autostart(L);
    lua_pop(L, 1);
}

static char *get_config_file_path() {
    static const char *files[] = {
        "$XDG_CONFIG_HOME/wavy/config.lua",
        "$HOME/.config/wavy/config.lua"
    };

    char *c_file = NULL;
    for (uint32_t i = 0; i < 2; i++) {
        wordexp_t f;
        if (wordexp(files[i], &f, 0) == 0) {
            if (access(f.we_wordv[0], F_OK) == 0) {
                c_file = strdup(f.we_wordv[0]);
                break;
            }
        }
        wordfree(&f);
    }
    return c_file;
}

void init_config() {
    config = calloc(1, sizeof(struct wavy_config_t));
    if (!config) {
        wavy_log(LOG_DEBUG, "Failed to allocate memory for configuration");
        exit(EXIT_FAILURE);
    }
    config->autostart = vector_init();

    const char *config_file = get_config_file_path();
    if (!config_file) {
        wavy_log(LOG_ERROR, "No config file found");
        exit(EXIT_FAILURE);
    }
    wavy_log(LOG_DEBUG, "Loading config file: %s", config_file);

    L_config = luaL_newstate();
    luaL_openlibs(L_config);
    int32_t err_load = luaL_loadfile(L_config, config_file);
    if (err_load != LUA_OK) {
        if (err_load == LUA_ERRFILE) {
            wavy_log(LOG_ERROR, "Error loading config.lua");
        } else if (err_load == LUA_ERRSYNTAX) {
            wavy_log(LOG_ERROR, "Syntax error in config.lua");
        }
        exit(EXIT_FAILURE);
    }

    // execute the script and initialize its global variables
    int32_t err = lua_pcall(L_config, 0, 0, 0);
    if (err == LUA_ERRRUN) {
        luaL_error(L_config, "Runtime error");
    }

    default_config();
    read_config(L_config);
    bar_config(L_config);
    free((void *) config_file);
}

void free_config() {
    lua_close(L_config);
    free(config);
}
