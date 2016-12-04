#ifndef __BAR_H
#define __BAR_H

#include <stdint.h>
#include <cairo/cairo.h>
#include <wlc/wlc.h>
#include <pthread.h>
#include <time.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "layout.h"

enum hook_t {
    HOOK_PERIODIC_SLOW,
    HOOK_PERIODIC_FAST,
    HOOK_VIEW_UPDATE,
    HOOK_USER,
    HOOK_UNKNOWN
};

enum side_t {
    SIDE_RIGHT,
    SIDE_LEFT,
    SIDE_UNKNOWN
};

struct status_entry_t {
    enum hook_t hook;
    enum side_t side;
    char *entry;
    uint32_t bg_color;
    uint32_t fg_color;
    uint32_t lua_reg_idx; // idx of a function registerd at LUA_REGISTRYINDEX
};

extern struct wavy_config_t *config;
extern lua_State *L_config;

static struct vector_t *status_entries;

// threads that periodically trigger the fast/slow hooks
static pthread_t hook_thread_fast;
static pthread_t hook_thread_slow;

// mutex that makes sure only one hook is updating data at a time.
// this prevents concurrent use of the lua state.
static pthread_mutex_t hook_lock;

void add_widget(enum side_t side, enum hook_t hook, int lua_ref);

// triggers the specified hook and updates all data associated with it by
// calling the lua callback function.
void trigger_hook(enum hook_t hook);

// updates the buffer with the available data
void update_bar(struct output *out);

// this is called from a wlc render callback
void render_bar(struct output *out);

void init_bar_config();
void init_bar_threads();
void init_bar(struct output *out);
void free_bar(struct bar_t *bar);
void stop_bar_threads();

#endif
