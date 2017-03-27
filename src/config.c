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
#include <libinput.h>
#include <xkbcommon/xkbcommon.h>

#include "commands.h"
#include "config.h"
#include "log.h"
#include "bar.h"
#include "utils.h"
#include "input.h"

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
    config->statusbar_font                      = "monospace 10";
    config->statusbar_gap                       = 4;
    config->statusbar_padding                   = 10;
    config->statusbar_position                  = POS_TOP;
    config->statusbar_bg_color                  = 0x282828ff;
    config->statusbar_active_ws_color           = 0x70407fff;
    config->statusbar_inactive_ws_color         = 0x404055ff;
    config->statusbar_active_ws_font_color      = 0xffffffff;
    config->statusbar_inactive_ws_font_color    = 0xccccccff;
    config->statusbar_separator_enabled         = false;
    config->statusbar_separator_color           = 0x2d95efff;
    config->statusbar_separator_width           = 1;

    for (uint32_t i = 0; i < 5; i++) {
        config->tile_layouts[i] = i;
    }
    config->num_layouts = 5;
}

static void check_argc(lua_State *L, uint32_t argc, uint32_t argc_expected,
        const char *func_name) {

    if (argc_expected != argc) {
        luaL_error(L, "Wrong number of arguments for a \'%s\' keybinding: "
                      "expected %I, got %I",
                      func_name, (lua_Integer) argc_expected,
                      (lua_Integer) argc);
    }
}

static uint32_t reg_lua_function(lua_State *L, int32_t idx_table, int32_t idx) {
    if (lua_geti(L, idx_table, idx) != LUA_TFUNCTION) {
        luaL_error(L, "Argument to \'lua\' keybinding must be function, got %s",
                lua_typename(L, lua_type(L, -1)));
    }
    uint32_t ref = luaL_ref(L, LUA_REGISTRYINDEX); // pops stack
    return ref;
}

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

// idx: positive index of subtable of 'keys' table
// modifier table is always the 2nd element in the table
static uint32_t get_mod(lua_State *L, int32_t idx) {
    if (lua_geti(L, idx, 2) != LUA_TTABLE) {
        luaL_error(L, "Invalid modifier entry, expected table");
    }
    uint32_t mod = 0;
    uint32_t len = lua_rawlen(L, -1);
    for (uint32_t i = 0; i < len; i++) {
        lua_geti(L, -1, i+1);
        mod |= string_to_mod(lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
    return mod;
}

// idx: positive index of subtable of 'keys' table
// sym is always the 3rd element in the table
static uint32_t get_sym(lua_State *L, int32_t idx) {
    if (lua_geti(L, idx, 3) != LUA_TSTRING) {
        luaL_error(L, "Invalid key symbol, expected string");
    }
    uint32_t keysym = xkb_keysym_from_name(lua_tostring(L, -1),
            XKB_KEYSYM_CASE_INSENSITIVE);
    lua_pop(L, 1);
    return keysym;
}

static enum direction_t get_dir(lua_State *L, int32_t idx_table, int32_t idx) {
    if (lua_geti(L, idx_table, idx) != LUA_TSTRING) {
        luaL_error(L, "Direction argument must be a string");
    }
    enum direction_t dir;
    const char *str = lua_tostring(L, -1);
    if (!strcmp(str, "left")) {
        dir = DIR_LEFT;
    } else if (!strcmp(str, "right")) {
        dir = DIR_RIGHT;
    } else if (!strcmp(str, "up")) {
        dir = DIR_UP;
    } else {
        dir = DIR_DOWN;
    }
    lua_pop(L, 1);
    return dir;
}

static float get_float(lua_State *L, int32_t idx_table, int32_t idx) {
    if (lua_geti(L, idx_table, idx) != LUA_TNUMBER) {
        luaL_error(L, "Expected float argument, got %s",
                lua_typename(L, lua_type(L, -1)));
    }
    float n = lua_tonumber(L, -1);
    lua_pop(L, 1);
    return n;
}

static uint32_t get_num(lua_State *L, int32_t idx_table, int32_t idx) {
    if (lua_geti(L, idx_table, idx) != LUA_TNUMBER) {
        luaL_error(L, "Expected number argument, got %s",
                lua_typename(L, lua_type(L, -1)));
    }
    uint32_t n = lua_tointeger(L, -1);
    lua_pop(L, 1);
    return n;
}

// TODO
// idx: positive index of subtable of 'keys' table
static void keybind_string_filter(lua_State *L, int32_t idx) {
    uint32_t argc = lua_rawlen(L, idx);
    if (lua_geti(L, idx, 1) != LUA_TSTRING) {
        luaL_error(L, "Invalid keybinding type: must be string, got %s",
                lua_typename(L, lua_type(L, -1)));
    }
    const char *kb_str = lua_tostring(L, -1);

    struct keybind_arg_t kb_null = {0, 0, 0, 0, 0};

    if (!strcmp(kb_str, "spawn")) {
        check_argc(L, argc, 4, "spawn");
        if (lua_geti(L, idx, 4) != LUA_TTABLE) {
            luaL_error(L, "Argument to \'spawn\' keybinding must be a table, "
                          "got %s", lua_typename(L, lua_type(L, -1)));
        }
        struct keybind_arg_t kba = {.ptr = (void **) table_to_str_array(L, -1)};
        cmd_update(get_mod(L, idx), get_sym(L, idx), kba, spawn_cmd);
        lua_pop(L, 1);
    } else if (!strcmp(kb_str, "lua")) {
        check_argc(L, argc, 4, "lua");
        struct keybind_arg_t kba = {
            .num = reg_lua_function(L, idx, 4)
        };
        cmd_update(get_mod(L, idx), get_sym(L, idx), kba, lua_cmd);
    } else if (!strcmp(kb_str, "exit")) {
        check_argc(L, argc, 3, "exit");
        cmd_update(get_mod(L, idx), get_sym(L, idx), kb_null, exit_cmd);
    } else if (!strcmp(kb_str, "close_view")) {
        check_argc(L, argc, 3, "close_view");
        cmd_update(get_mod(L, idx), get_sym(L, idx), kb_null, close_view_cmd);
    } else if (!strcmp(kb_str, "cycle_tiling_mode")) {
        check_argc(L, argc, 3, "cycle_tiling_mode");
        cmd_update(get_mod(L, idx), get_sym(L, idx), kb_null,
                cycle_tiling_mode_cmd);
    } else if (!strcmp(kb_str, "cycle_view")) {
        check_argc(L, argc, 4, "cycle_view");
        if (lua_geti(L, idx, 4) != LUA_TSTRING) {
            luaL_error(L, "Direction argument to \'cycle_view\' keybinding "
                          "must be a string, got %s",
                          lua_typename(L, lua_type(L, -1)));
        }
        const char *next_bkwd = lua_tostring(L, -1);
        struct keybind_arg_t kba = {
            .num = strcmp(next_bkwd, "previous") ? 1 : 0
        };
        cmd_update(get_mod(L, idx), get_sym(L, idx), kba, cycle_view_cmd);
        lua_pop(L, 1);
    } else if (!strcmp(kb_str, "select")) {
        check_argc(L, argc, 4, "select");
        struct keybind_arg_t kba = {.dir = get_dir(L, idx, 4)};
        cmd_update(get_mod(L, idx), get_sym(L, idx), kba, select_cmd);
    } else if (!strcmp(kb_str, "move")) {
        check_argc(L, argc, 4, "move");
        struct keybind_arg_t kba = {.dir = get_dir(L, idx, 4)};
        cmd_update(get_mod(L, idx), get_sym(L, idx), kba, move_cmd);
    } else if (!strcmp(kb_str, "new_frame")) {
        check_argc(L, argc, 4, "new_frame");
        if (lua_geti(L, idx, 4) != LUA_TSTRING) {
            luaL_error(L, "Direction argument to \'new_frame\' must be string, "
                          "got %s", lua_typename(L, lua_type(L, -1)));
        }
        const char *d = lua_tostring(L, -1);
        struct keybind_arg_t kba = {
            .dir = strcmp(d, "right") ? DIR_LEFT : DIR_RIGHT
        };
        cmd_update(get_mod(L, idx), get_sym(L, idx), kba, new_frame_cmd);
        lua_pop(L, 1);
    } else if (!strcmp(kb_str, "delete_frame")) {
        check_argc(L, argc, 3, "delete_frame");
        cmd_update(get_mod(L, idx), get_sym(L, idx), kb_null, delete_frame_cmd);
    } else if (!strcmp(kb_str, "resize")) {
        check_argc(L, argc, 5, "resize");
        struct keybind_arg_t kba = {
            .dir = get_dir(L, idx, 4),
            .f = get_float(L, idx, 5)
        };
        cmd_update(get_mod(L, idx), get_sym(L, idx), kba, resize_cmd);
    } else if (!strcmp(kb_str, "cycle_workspace")) {
        check_argc(L, argc, 4, "cycle_workspace");
        if (lua_geti(L, idx, 4) != LUA_TSTRING) {
            luaL_error(L, "Invalid argument to \'cycle_workspace\', must be "
                          "string, got %s", lua_typename(L, lua_type(L, -1)));
        }
        const char *next_bkwd = lua_tostring(L, -1);
        struct keybind_arg_t kba = {
            .num = strcmp(next_bkwd, "previous") ? 1 : 0
        };
        cmd_update(get_mod(L, idx), get_sym(L, idx), kba, cycle_workspace_cmd);
        lua_pop(L, 1);
    } else if (!strcmp(kb_str, "add_workspace")) {
        check_argc(L, argc, 3, "add_workspace");
        cmd_update(get_mod(L, idx), get_sym(L, idx), kb_null, add_ws_cmd);
    } else if (!strcmp(kb_str, "select_workspace")) {
        check_argc(L, argc, 4, "select_workspace");
        struct keybind_arg_t kba = {.num = get_num(L, idx, 4)};
        cmd_update(get_mod(L, idx), get_sym(L, idx), kba, switch_workspace_cmd);
    } else if (!strcmp(kb_str, "move_to_workspace")) {
        check_argc(L, argc, 4, "move_to_workspace");
        struct keybind_arg_t kba = {.num = get_num(L, idx, 4)};
        cmd_update(get_mod(L, idx), get_sym(L, idx), kba, move_to_ws_cmd);
    } else {
        luaL_error(L, "Unknown keybinding type: \'%s\'", kb_str);
    }

    lua_pop(L, 1);
}

static void keybind_config(lua_State *L) {
    if (lua_getglobal(L, "keys") != LUA_TTABLE) {
        wavy_log(LOG_WAVY, "Warning: no keybindings specified!");
        return;
    }
    int32_t keys = lua_gettop(L);
    int32_t len = lua_rawlen(L, keys);
    for (int32_t i = 0; i < len; i++) {
        int32_t idx = i+1;
        if (lua_geti(L, keys, idx) != LUA_TTABLE) {
            luaL_error(L, "Invalid entry in \'keys\' table, must be table");
        }
        keybind_string_filter(L, lua_gettop(L));
        lua_pop(L, 1);
    }
    lua_settop(L, 0);
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

static void set_conf_bool(lua_State *L, const char *name, bool *conf,
        int32_t idx) {

    if (lua_getfield(L, idx, name) == LUA_TBOOLEAN) {
        *conf = lua_toboolean(L, -1);
    }
    lua_pop(L, 1);
}

static void set_layouts(lua_State *L) {
    if (lua_getglobal(L, "layouts") != LUA_TTABLE) {
        lua_settop(L, 0);
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
    lua_settop(L, 0);
}

static void set_autostart(lua_State *L) {
    if (lua_getglobal(L, "autostart") != LUA_TTABLE) {
        lua_settop(L, 0);
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
    lua_settop(L, 0);
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
        lua_settop(L, 0);
        return;
    }

    int32_t bar_idx = lua_gettop(L);
    set_conf_int(L, "height", &config->statusbar_height, bar_idx);
    set_conf_str(L, "font", &config->statusbar_font, bar_idx);
    set_conf_int(L, "gap", &config->statusbar_gap, bar_idx);
    set_conf_int(L, "padding", &config->statusbar_padding, bar_idx);
    set_conf_bool(L, "separator", &config->statusbar_separator_enabled,
            bar_idx);
    set_conf_int(L, "separator_color", &config->statusbar_separator_color,
            bar_idx);
    set_conf_int(L, "separator_width", &config->statusbar_separator_width,
            bar_idx);

    if (lua_getfield(L, bar_idx, "position") == LUA_TSTRING) {
        enum position_t p = pos_str_to_enum(lua_tostring(L, -1));
        if (p != POS_UNKNOWN) {
            config->statusbar_position = p;
        } else {
            luaL_error(L,
                "Invalid bar position: Only \'top\', \'bottom\' are allowed");
        }
    }

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
    lua_settop(L, 0);
}

static void read_config(lua_State *L) {
    if (lua_getglobal(L_config, "config") != LUA_TTABLE) {
        lua_settop(L, 0);
        return;
    }

    set_conf_int(L, "frame_gaps_size", &config->frame_gaps_size, -1);
    set_conf_int(L, "frame_border_size", &config->frame_border_size, -1);
    set_conf_int(L, "frame_border_empty_size",
            &config->frame_border_empty_size, -1);
    set_conf_int(L, "frame_border_active_color",
            &config->frame_border_active_color, -1);
    set_conf_int(L, "frame_border_inactive_color",
            &config->frame_border_inactive_color, -1);
    set_conf_int(L, "frame_border_empty_active_color",
            &config->frame_border_empty_active_color, -1);
    set_conf_int(L, "frame_border_empty_inactive_color",
            &config->frame_border_empty_inactive_color, -1);

    set_conf_int(L, "view_border_size", &config->view_border_size, -1);
    set_conf_int(L, "view_border_active_color",
            &config->view_border_active_color, -1);
    set_conf_int(L, "view_border_inactive_color",
            &config->view_border_inactive_color, -1);

    set_conf_str(L, "wallpaper", &config->wallpaper, -1);

    // expand file path
    wordexp_t f;
    wordexp(config->wallpaper, &f, 0);
    config->wallpaper = f.we_wordv[0];

    set_layouts(L);
    set_autostart(L);
    lua_settop(L, 0);
}

static char *get_config_file_path() {
    static const char *files[] = {
        "$XDG_CONFIG_HOME/wavy/config.lua",
        "$HOME/.config/wavy/config.lua"
    };

    char *c_file = NULL;
    for (uint32_t i = 0; i < 2; i++) {
        wordexp_t f;
        const char *file;
        if (cmdline_config_file && (i == 1)) {
            file = cmdline_config_file;
        } else {
            file = files[i];
        }
        if (wordexp(file, &f, 0) == 0) {
            if (access(f.we_wordv[0], F_OK) == 0) {
                c_file = strdup(f.we_wordv[0]);
                break;
            }
        }
        wordfree(&f);
    }
    return c_file;
}

// function to be called when an error in lua_pcall occurs
static int msghandler(lua_State *L) {
    const char *msg = lua_tostring(L, -1);
    if (!msg) {
        return 1;
    }
    luaL_traceback(L, L, msg, 1);
    const char *trace = lua_tostring(L, -1);
    lua_writestringerror("%s\n", trace);
    return 1;
}

void init_config() {
    config = calloc(1, sizeof(struct wavy_config_t));
    if (!config) {
        wavy_log(LOG_DEBUG, "Failed to allocate memory for configuration");
        exit(EXIT_FAILURE);
    }
    config->autostart = vector_init();
    config->input_configs = vector_init();

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
            const char *msg = lua_tostring(L_config, -1);
            if (msg) {
                wavy_log(LOG_ERROR, "%s", msg);
            } else {
                wavy_log(LOG_ERROR, "Syntax error in config.lua");
            }
        }
        exit(EXIT_FAILURE);
    }

    // push a msghandler on the stack which prints a stacktrace when
    // lua_pcall fails
    int32_t base = lua_gettop(L_config);
    lua_pushcfunction(L_config, msghandler);
    lua_insert(L_config, base);

    // execute the script and initialize its global variables
    int32_t status = lua_pcall(L_config, 0, 0, base);

    if (status != LUA_OK) {
        luaL_error(L_config, "Runtime error");
    }

    /* lua_settop(L_config, 1); */

    default_config();
    read_config(L_config);
    bar_config(L_config);
    input_configs_init(L_config);
    keybind_config(L_config);
    free((void *) config_file);
}

static void free_input_config(void *_ic) {
    struct input_config *ic = _ic;
    free((void *) ic->name);
}

// needs a terminating null pointer!
static void free_char_char(void *_cc) {
    char **cc = _cc;
    int i;
    char *c;
    for (i = 0, c = *cc; c; i++, c = cc[i]) {
        free(c);
    }
    free(cc);
}

void free_config() {
    lua_close(L_config);
    vector_foreach(config->autostart, free_char_char);
    vector_free(config->autostart);
    vector_foreach(config->input_configs, free_input_config);
    vector_free(config->input_configs);
    free(config);
}
