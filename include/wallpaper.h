#ifndef __WALLPAPER_H
#define __WALLPAPER_H

#include <cairo/cairo.h>

#include "layout.h"

// TODO make this configurable
/* static char *wp_file = "/home/luke/test.png"; */
static char *wp_file = "/home/luke/wp.png";

void init_wallpaper(struct output *out, uint32_t width, uint32_t height);
void render_wallpaper(struct output *out);

#endif
