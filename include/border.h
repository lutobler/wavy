#ifndef __BORDER_H
#define __BORDER_H
#include "layout.h"

extern struct wavy_config_t *config;

void free_border(struct border_t *border);
void update_frame_border(struct frame *fr, bool realloc);

/*
 * This should only be called if the buffer was cleared by update_frame_border
 * beforehand.
 */
void update_view_border(struct frame *fr, wlc_handle view,
        struct wlc_geometry *g);

void output_render_frame_borders(wlc_handle output);
void render_frame_borders(struct frame *fr);

#endif
