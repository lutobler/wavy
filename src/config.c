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

static void set_input_config(lua_State *L, int32_t idx,
        struct input_config *ic) {

    lua_pushnil(L);
    while (lua_next(L, idx) != 0) {
        if (!(lua_type(L, -2) == LUA_TSTRING)) {
            luaL_error(L, "Input configuration option name must be a string");
        }
        if (!(lua_type(L, -1) == LUA_TSTRING)) {
            luaL_error(L, "Input configuration option value must be a string");
        }

        const char *key = lua_tostring(L, -2);
        const char *value = lua_tostring(L, -1);

        if (!strcmp(key, "tap_to_click")) {
            if (!strcmp(value, "enabled")) {
                ic->tap_state = LIBINPUT_CONFIG_TAP_ENABLED;
            } else if (!strcmp(value, "disabled")) {
                ic->tap_state = LIBINPUT_CONFIG_TAP_DISABLED;
            } else {
                luaL_error(L, "Invalid option for \'tap_to_click\': %s", value);
            }
        } else if (!strcmp(key, "scroll_method")) {
            if (!strcmp(value, "no_scroll")) {
                ic->scroll_method = LIBINPUT_CONFIG_SCROLL_NO_SCROLL;
            } else if (!strcmp(value, "twofinger")) {
                ic->scroll_method = LIBINPUT_CONFIG_SCROLL_2FG;
            } else if (!strcmp(value, "edge")) {
                ic->scroll_method = LIBINPUT_CONFIG_SCROLL_EDGE;
            } else {
                luaL_error(L, "Invalid option for \'scroll_method\': %s",
                        value);
            }
        } else if (!strcmp(key, "acceleration_profile")) {
            if (!strcmp(value, "flat")) {
                ic->accel_profile = LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT;
            } else if (!strcmp(value, "adaptive")) {
                ic->accel_profile = LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE;
            } else if (!strcmp(value, "none")) {
                ic->accel_profile = LIBINPUT_CONFIG_ACCEL_PROFILE_NONE;
            } else {
                luaL_error(L, "Invalid option for \'acceleration_profile\': %s",
                        value);
            }
        } else if (!strcmp(key, "acceleration_speed")) {
            double speed = (double) lua_tonumber(L, -1);
            if (!(speed >= -1.0 && speed <= 1.0)) {
                luaL_error(L, "Invalid option for \'acceleration_speed\': %f "
                            "(must be in range [-1, 1])", speed);
            }
            ic->accel_speed = speed;
        } else if (!strcmp(key, "tap_and_drag")) {
            if (!strcmp(value, "enabled")) {
                ic->tap_and_drag = LIBINPUT_CONFIG_DRAG_ENABLED;
            } else if (!strcmp(value, "disabled")) {
                ic->tap_and_drag = LIBINPUT_CONFIG_DRAG_DISABLED;
            } else {
                luaL_error(L, "Invalid option for \'tap_and_drag\': %s", value);
            }
        } else if (!strcmp(key, "tap_and_drag_lock")) {
            if (!strcmp(value, "enabled")) {
                ic->tap_and_drag_lock = LIBINPUT_CONFIG_DRAG_LOCK_ENABLED;
            } else if (!strcmp(value, "disabled")) {
                ic->tap_and_drag_lock = LIBINPUT_CONFIG_DRAG_LOCK_DISABLED;
            } else {
                luaL_error(L, "Invalid option for \'tap_and_drag_lock\': %s",
                        value);
            }
        } else if (!strcmp(key, "disable_while_typing")) {
            if (!strcmp(value, "enabled")) {
                ic->disable_while_typing = LIBINPUT_CONFIG_DWT_ENABLED;
            } else if (!strcmp(value, "disabled")) {
                ic->disable_while_typing = LIBINPUT_CONFIG_DWT_DISABLED;
            } else {
                luaL_error(L, "Invalid option for \'disable_while_typing\': %s",
                        value);
            }
        } else if (!strcmp(key, "middle_emulation")) {
            if (!strcmp(value, "enabled")) {
                ic->middle_emulation = LIBINPUT_CONFIG_MIDDLE_EMULATION_ENABLED;
            } else if (!strcmp(value, "disabled")) {
                ic->middle_emulation
                    = LIBINPUT_CONFIG_MIDDLE_EMULATION_DISABLED;
            } else {
                luaL_error(L, "Invalid option for \'middle_emulation\': %s",
                        value);
            }
        } else if (!strcmp(key, "click_method")) {
            if (!strcmp(value, "clickfinger")) {
                ic->click_method = LIBINPUT_CONFIG_CLICK_METHOD_CLICKFINGER;
            } else if (!strcmp(value, "button_areas")) {
                ic->click_method = LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS;
            } else if (!strcmp(value, "none")) {
                ic->click_method = LIBINPUT_CONFIG_CLICK_METHOD_NONE;
            }
        }

        lua_pop(L, 1);
    }
    lua_settop(L, idx);
}

static void apply_input_config(struct input_config *ic,
        struct libinput_device *device) {

    if (ic->tap_state != INT_MIN) {
        wavy_log(LOG_DEBUG, "Set tap_enabled(%d) for \'%s\'",
                ic->tap_state, ic->name);
        libinput_device_config_tap_set_enabled(device, ic->tap_state);
    }
    if (ic->scroll_method != INT_MIN) {
        wavy_log(LOG_DEBUG, "Set scroll_method(%d) for \'%s\'",
                ic->scroll_method, ic->name);
        libinput_device_config_scroll_set_method(device, ic->scroll_method);
    }
    if (ic->accel_profile != INT_MIN) {
        wavy_log(LOG_DEBUG, "Set accel_profile(%d) for \'%s\'",
                ic->accel_profile, ic->name);
        libinput_device_config_accel_set_profile(device, ic->accel_profile);
    }
    if (ic->tap_and_drag != INT_MIN) {
        wavy_log(LOG_DEBUG, "Set tap_and_drag(%d) for \'%s\'",
                ic->tap_and_drag, ic->name);
        libinput_device_config_accel_set_profile(device, ic->tap_and_drag);
    }
    if (ic->tap_and_drag_lock != INT_MIN) {
        wavy_log(LOG_DEBUG, "Set tap_and_drag_lock(%d) for \'%s\'",
                ic->tap_and_drag_lock, ic->name);
        libinput_device_config_accel_set_profile(device, ic->tap_and_drag_lock);
    }
    if (ic->disable_while_typing != INT_MIN) {
        wavy_log(LOG_DEBUG, "Set disable_while_typing(%d) for \'%s\'",
                ic->disable_while_typing, ic->name);
        libinput_device_config_accel_set_profile(device,
                ic->disable_while_typing);
    }
    if (ic->middle_emulation != INT_MIN) {
        wavy_log(LOG_DEBUG, "Set middle_emulation(%d) for \'%s\'",
                ic->middle_emulation, ic->name);
        libinput_device_config_accel_set_profile(device, ic->middle_emulation);
    }
    if (ic->click_method != INT_MIN) {
        wavy_log(LOG_DEBUG, "Set click_method(%d) for \'%s\'",
                ic->click_method, ic->name);
        libinput_device_config_accel_set_profile(device, ic->click_method);
    }
    if (!isnan(ic->accel_speed)) {
        wavy_log(LOG_DEBUG, "Set accel_speed(%f) for \'%s\'",
                ic->accel_speed, ic->name);
        libinput_device_config_accel_set_speed(device, ic->accel_speed);
    }
}

static void input_configs_init(lua_State *L) {
    if (lua_getglobal(L, "input") != LUA_TTABLE) {
        lua_settop(L, 0);
        return;
    }

    uint32_t inputs = lua_gettop(L);
    lua_pushnil(L);
    while (lua_next(L, inputs) != 0) {
        if (!(lua_type(L, -2) == LUA_TSTRING)) {
            luaL_error(L, "Input device name must be a string");
        }
        if (!(lua_type(L, -1) == LUA_TTABLE)) {
            luaL_error(L, "Input device configuration must be a table");
        }

        // initialize an empty input configuration
        const char *dev_name = strdup(lua_tostring(L, -2));
        struct input_config *ic = malloc(sizeof(struct input_config));
        if (!ic) {
            wavy_log(LOG_ERROR, "Failed to allocate input config");
            continue;
        }
        vector_add(config->input_configs, ic);
        ic->name = dev_name;

        // INT_MIN signifies an unset variable
        ic->tap_state = INT_MIN;
        ic->scroll_method = INT_MIN;
        ic->accel_profile = INT_MIN;
        ic->tap_and_drag = INT_MIN;
        ic->tap_and_drag_lock = INT_MIN;
        ic->disable_while_typing = INT_MIN;
        ic->middle_emulation = INT_MIN;
        ic->click_method = INT_MIN;
        ic->accel_speed = NAN;

        int32_t conf_table = lua_gettop(L);
        set_input_config(L, conf_table, ic);
        lua_pop(L, 1);
    }
    lua_settop(L, 0);
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
    input_configs_init(L_config);
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

void configure_input(struct libinput_device *device) {
    const char *device_name = libinput_device_get_name(device);
    for (uint32_t i = 0; i < config->input_configs->length; i++) {
        struct input_config *ic = config->input_configs->items[i];
        if (!strcmp(device_name, ic->name)) {
            wavy_log(LOG_DEBUG, "Configuring device: \'%s\'", device_name);
            apply_input_config(ic, device);
        }
    }
}

void unconfigure_input(struct libinput_device *device) {
    const char *device_name = libinput_device_get_name(device);
    for (uint32_t i = 0; i < config->input_configs->length; i++) {
        struct input_config *ic = config->input_configs->items[i];
        if (!strcmp(device_name, ic->name)) {
            wavy_log(LOG_DEBUG, "Input device deleted: \'%s\'", device_name);
            vector_del(config->input_configs, i);
            free((void *) ic->name);
            free(ic);
        }
    }
}
