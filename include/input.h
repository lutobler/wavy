#ifndef __INPUT_H
#define __INPUT_H

#include <libinput.h>
#include <lua.h>

#include "config.h"

struct input_config {
    const char *name;

    int     tap_state;
    int     scroll_method;
    int     accel_profile;
    int     tap_and_drag;
    int     tap_and_drag_lock;
    int     disable_while_typing;
    int     middle_emulation;
    int     click_method;
    double  accel_speed; // range: [-1, 1]
};

extern struct wavy_config_t *config;

void input_configs_init(lua_State *L);
void configure_input(struct libinput_device *device);
void unconfigure_input(struct libinput_device *device);

#endif
