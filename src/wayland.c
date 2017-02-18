#include <stdlib.h>
#include <string.h>
#include <wayland-client.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "wayland-background-client-protocol.h"
#include "wayland.h"
#include "log.h"
#include "vector.h"

static void display_handle_geometry(void *data, struct wl_output *wl_output,
        int32_t x, int32_t y, int32_t physical_width, int32_t physical_height,
        int32_t subpixel, const char *make, const char *model,
        int32_t transform) {
    (void) data; (void) wl_output; (void) x; (void) y; (void) physical_width;
    (void) physical_height; (void) subpixel; (void) make; (void) model;
    (void) transform;
    // Intentionally left blank
}

static void display_handle_mode(void *data, struct wl_output *wl_output,
        uint32_t flags, int32_t width, int32_t height, int32_t refresh) {

    (void) wl_output;
    (void) refresh;
    struct wl_output_state *state = data;
    if (flags & WL_OUTPUT_MODE_CURRENT) {
        state->flags = flags;
        state->width = width;
        state->height = height;
        wavy_log(LOG_DEBUG, "Got mode %dx%x:0x%X for output %p",
                width, height, flags, data);
    }
}

static void display_handle_done(void *data, struct wl_output *wl_output) {
    (void) data;
    (void) wl_output;
    // Intentionally left blank
}

static void display_handle_scale(void *data, struct wl_output *wl_output,
        int32_t scale) {

    (void) wl_output;

    struct wl_output_state *state = data;
    state->scale = scale;
    wavy_log(LOG_DEBUG, "Got scale factor %d for output %p", scale, data);
}


static const struct wl_output_listener output_listener = {
   .geometry = display_handle_geometry,
   .mode = display_handle_mode,
   .done = display_handle_done,
   .scale = display_handle_scale
};

static void registry_global(void *data, struct wl_registry *registry,
		uint32_t id, const char *interface, uint32_t version) {

	struct registry *reg = data;

    if (strcmp(interface, "wl_compositor") == 0) {
        reg->compositor = wl_registry_bind(registry, id,
                &wl_compositor_interface, version);
    } else if (strcmp(interface, "wl_shm") == 0) {
		reg->shm = wl_registry_bind(registry, id, &wl_shm_interface, version);
    } else if (strcmp(interface, "wl_output") == 0) {
        struct wl_output_state *state = malloc(sizeof(struct wl_output_state));
        state->output = wl_registry_bind(registry, id,
                &wl_output_interface, version);
        state->scale = 1;
        vector_add(reg->wl_outputs, state);
        wl_output_add_listener(state->output, &output_listener, state);
    } else if (strcmp(interface, "wl_shell") == 0) {
        reg->shell = wl_registry_bind(registry, id,
                &wl_shell_interface, version);
    } else if (strcmp(interface, "background") == 0) {
        reg->background = wl_registry_bind(registry, id,
                &background_interface, 1);
    }
}

static void registry_global_remove(void *data, struct wl_registry *registry,
        uint32_t name) {
    (void) data;
    (void) registry;
    (void) name;
	// this space intentionally left blank
}

static const struct wl_registry_listener registry_listener = {
	.global = registry_global,
	.global_remove = registry_global_remove
};

struct registry *registry_poll() {
    struct registry *reg = calloc(1, sizeof(struct registry));
    if (!reg) {
        wavy_log(LOG_ERROR, "Unable to allocate memory for registry struct");
        return NULL;
    }

    reg->wl_outputs = vector_init();

    reg->display = wl_display_connect(NULL);
    if (!reg->display) {
        wavy_log(LOG_ERROR, "Unable to connect to display");
        free(reg);
        return NULL;
    }

    struct wl_registry *r = wl_display_get_registry(reg->display);
    wl_registry_add_listener(r, &registry_listener, reg);
    wl_display_dispatch(reg->display);
    wl_display_roundtrip(reg->display);
    wl_registry_destroy(r);

    return reg;
}

static void destroy_outpus(void *_state) {
    struct wl_output_state *state = _state;
    wl_output_destroy(state->output);
}

void free_registry(struct registry *reg) {
    if (reg->compositor) {
        wl_compositor_destroy(reg->compositor);
    }
    if (reg->display) {
        wl_display_disconnect(reg->display);
    }
    if (reg->shm) {
        wl_shm_destroy(reg->shm);
    }
    if (reg->wl_outputs) {
        vector_foreach(reg->wl_outputs, destroy_outpus);
        vector_free(reg->wl_outputs);
    }
    free(reg);
}

static int create_tmpfile_cloexec(char *tmpname) {
    int fd;
    if ((fd = mkostemp(tmpname, O_CLOEXEC)) >= 0) {
        unlink(tmpname);
    }
    return fd;
}

static int create_anonymous_file(off_t size) {
    int fd;
    static const char *template = "/wavy-shared-XXXXXX";
    uint32_t t_len = strlen(template);
    const char *xdg_dir = getenv("XDG_RUNTIME_DIR");
    if (!xdg_dir) {
        return -1;
    }

    uint32_t x_len = strlen(xdg_dir);
    size_t p_len = x_len + t_len + 1;
    char *tmpfile = calloc(1, p_len);
    if (!tmpfile) {
        return -1;
    }
    strncpy(tmpfile, xdg_dir, x_len);
    strncat(tmpfile, template, t_len);

    fd = create_tmpfile_cloexec(tmpfile);
    free(tmpfile);

    if (fd < 0) {
        return -1;
    }

    if (ftruncate(fd, size) < 0) {
        close(fd);
        return -1;
    }

    return fd;
}

struct wl_buffer *create_shm_buffer(struct wl_shm *shm, uint32_t width,
        uint32_t height, void **shm_data) {

    struct wl_shm_pool *pool;
    struct wl_buffer *shm_buf;
    int stride = width * 4;
    int size = height * stride;
    int fd;

    if ((fd = create_anonymous_file(size)) < 0) {
        wavy_log(LOG_ERROR, "Failed to create anonymous file");
        return NULL;
    }

    *shm_data = mmap(NULL, size, PROT_READ | PROT_WRITE,
            MAP_SHARED, fd, 0);

    if (shm_data == MAP_FAILED) {
        wavy_log(LOG_ERROR, "Memory mapping failed");
        close(fd);
        return NULL;
    }

    pool = wl_shm_create_pool(shm, fd, size);
    shm_buf = wl_shm_pool_create_buffer(pool, 0, width, height, stride,
            WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);

    return shm_buf;
}

void create_window(struct wl_surface *surface, struct wl_buffer *buffer,
        uint32_t width, uint32_t height) {

    wl_surface_attach(surface, buffer, 0, 0);
    wl_surface_damage(surface, 0, 0, width, height);
    wl_surface_commit(surface);
}
