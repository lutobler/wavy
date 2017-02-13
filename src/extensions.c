#include <stdint.h>
#include <wlc/wlc.h>
#include <wlc/wlc-wayland.h>
#include <wayland-server.h>

#include "extensions.h"
#include "wayland-gamma-control-server-protocol.h"
#include "log.h"

static void gamma_control_destroy(struct wl_client *client,
        struct wl_resource *res) {

    (void) client;
    wl_resource_destroy(res);
}

static void gamma_control_set_gamma(struct wl_client *client,
        struct wl_resource *res, struct wl_array *red,
        struct wl_array *green, struct wl_array *blue) {

    (void) client;
    if (red->size != green->size || red->size != blue->size) {
        wl_resource_post_error(res, GAMMA_CONTROL_ERROR_INVALID_GAMMA,
                "The gamma ramps don't have the same size");
        return;
    }

    uint16_t *r = (uint16_t *) red->data;
    uint16_t *g = (uint16_t *) green->data;
    uint16_t *b = (uint16_t *) blue->data;
    wlc_handle output = wlc_handle_from_wl_output_resource(
            wl_resource_get_user_data(res));

    if (!output) {
        return;
    }

    wavy_log(LOG_DEBUG, "Setting gamma for output");
    wlc_output_set_gamma(output, red->size / sizeof(uint16_t), r, g, b);
}

static void gamma_control_reset_gamma(struct wl_client *client,
        struct wl_resource *resource) {
    (void) client;
    (void) resource;
    // This space is intentionally left blank
}

static struct gamma_control_interface gamma_control_implementation = {
    .destroy = gamma_control_destroy,
    .set_gamma = gamma_control_set_gamma,
    .reset_gamma = gamma_control_reset_gamma
};

static void gamma_control_manager_destroy(struct wl_client *client,
        struct wl_resource *res) {

    (void) client;
    wl_resource_destroy(res);
}

static void gamma_control_manager_get(struct wl_client *client,
        struct wl_resource *res, uint32_t id, struct wl_resource *_output) {

    struct wl_resource *manager_res = wl_resource_create(client,
            &gamma_control_interface, wl_resource_get_version(res), id);
    wlc_handle output = wlc_handle_from_wl_output_resource(_output);
    if (!output) {
        return;
    }

    wl_resource_set_implementation(manager_res, &gamma_control_implementation,
            _output, NULL);
    gamma_control_send_gamma_size(manager_res,
            wlc_output_get_gamma_size(output));
}

static struct gamma_control_manager_interface gamma_manager_implementation = {
    .destroy = gamma_control_manager_destroy,
    .get_gamma_control = gamma_control_manager_get
};

static void gamma_control_manager_bind(struct wl_client *client, void *data,
        unsigned int version, unsigned int fd) {

    (void) data;
    if (version > 1) {
        return; // unsupported version
    }

    struct wl_resource *resource = wl_resource_create(client,
            &gamma_control_manager_interface, version, fd);
    if (!resource) {
        wl_client_post_no_memory(client);
    }

    wl_resource_set_implementation(resource, &gamma_manager_implementation,
            NULL, NULL);
}

void register_extensions(void) {
    wl_global_create(wlc_get_wl_display(), &gamma_control_manager_interface, 1,
            NULL, gamma_control_manager_bind);
}
