#ifndef __EXTENSIONS_H
#define __EXTENSIONS_H

#include <wlc/wlc.h>
#include <wayland-server.h>

#include "wayland-background-server-protocol.h"
#include "vector.h"

struct background_config {
    wlc_handle output;
    wlc_resource surface;
    struct wl_resource *wl_surface_res;
    struct wl_client *client;
    wlc_handle handle;
};

struct vector_t *backgrounds;

void register_extensions(void);

#endif
