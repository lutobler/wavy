#ifndef __CONFIG_H
#define __CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "vector.h"

enum auto_tile_t {
    TILE_UNKNOWN = -1,
    TILE_VERTICAL = 0,
    TILE_HORIZONTAL = 1,
    TILE_GRID = 2,
    TILE_FULLSCREEN = 3,
    TILE_FIBONACCI = 4
};

enum position_t {
    POS_TOP,
    POS_BOTTOM,
    POS_UNKNOWN
};

struct wavy_config_t {
    uint32_t statusbar_height;
    char *statusbar_font;
    uint32_t statusbar_gap;
    uint32_t statusbar_padding;
    enum position_t statusbar_position;
    uint32_t statusbar_bg_color;
    uint32_t statusbar_active_ws_color;
    uint32_t statusbar_inactive_ws_color;
    uint32_t statusbar_active_ws_font_color;
    uint32_t statusbar_inactive_ws_font_color;

    uint32_t frame_gaps_size;
    uint32_t frame_border_size;
    uint32_t frame_border_empty_size;
    uint32_t frame_border_active_color;
    uint32_t frame_border_inactive_color;
    uint32_t frame_border_empty_active_color;
    uint32_t frame_border_empty_inactive_color;

    uint32_t view_border_size;
    uint32_t view_border_active_color;
    uint32_t view_border_inactive_color;

    char *wallpaper; // file path

    enum auto_tile_t tile_layouts[5];
    char *tile_layout_strs[5];
    uint32_t num_layouts;

    // vector of **char's
    struct vector_t *autostart;
};

void init_config();
void free_config();

#endif
