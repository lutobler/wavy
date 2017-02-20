#include <libinput.h>
#include <string.h>
#include <math.h>

#include "input.h"
#include "log.h"
#include "vector.h"

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

void input_configs_init(lua_State *L) {
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
