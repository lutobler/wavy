#ifndef __LAYOUT_H
#define __LAYOUT_H
#include <wlc/wlc.h>
#include <wlc/wlc-wayland.h>
#include <stdbool.h>
#include <cairo/cairo.h>
#include <pthread.h>

#include "vector.h"
#include "config.h"

/*
 * SPLIT_HORIZONTAL: Horizontal axis is split.
 * SPLIT_VERTICAL: Vertical axis is split.
 * SPLIT_NONE: No split, frame is a tree leaf.
 */
enum frame_split_t {
    SPLIT_NONE,
    SPLIT_HORIZONTAL,
    SPLIT_VERTICAL
};

enum direction_t {
    DIR_UP,
    DIR_DOWN,
    DIR_LEFT,
    DIR_RIGHT
};

struct output {
    struct workspace *active_ws;
    wlc_handle output_handle;
    struct wlc_geometry g; // geometry adjusted for the statusbar

    struct bar_t {
        unsigned char *buffer;
        struct wlc_geometry g;
        cairo_t *cr;
        cairo_surface_t *surface;
        pthread_mutex_t draw_lock;

        // If set, the buffer will be reallocated on the next update.
        // This is needed for changing the output resolution.
        bool dirty;
    } bar;
};

struct workspace {
    struct frame *root_frame;
    struct frame *active_frame;
    struct output *assigned_output;
    uint32_t number;
    bool is_visible;
};

struct border_t {
    unsigned char *buffer;
    cairo_t *cr;
    cairo_surface_t *surface;
    size_t buf_size;
    struct wlc_geometry g_gaps; // real geometry without gaps
    struct wlc_geometry g; // real geometry with gaps applied
};

struct frame {
    enum frame_split_t split;

    // Index of the array of tiling layouts in the tile_layouts array.
    uint32_t tile;

    struct frame *parent;

    // SPLIT_HORIZONTAL: left child is on the left, right child on the right.
    // SPLIT_VERTICAL: right child is at the bottom, left child at the top.
    struct frame *left;
    struct frame *right;

    // Most recently used left/right child frame
    struct frame *last_focused;

    // Vector of pointers to wlc_handle's
    struct vector_t *children;

    // Frame border. Is also used to hold the geometry of the frame.
    struct border_t border;

    wlc_handle active_view;

    // Relative size compared to parent. Always 1.0 for the root.
    // Used to recalculate width/height when output dimension change.
    float rel_size;
};

extern bool debug_enabled;
extern struct wavy_config_t *config;

static struct vector_t *outputs;
static struct vector_t *workspaces;
static struct output *active_output;

// Helper functions
struct workspace *get_active_ws();
struct frame *get_active_frame();
wlc_handle get_active_view();
struct frame *get_root_frame_by_output(wlc_handle output);
struct frame *frame_by_view(wlc_handle view); // on active workspace
struct frame *frame_by_view_global(wlc_handle view); // on all workspaces
struct output *get_output_by_handle(wlc_handle view);
struct vector_t *get_workspaces(); // returns the workspace list
void schedule_render_all_outputs();
struct vector_t *get_outputs();

// Called on startup. Initializes the outputs, workspaces vectors and sets
// up the workspaces and a root frame each.
void init_layout();

// Called when a new output appears in wlc.
void add_output(wlc_handle output);

// Deletes an output and frees up its resources.
void delete_output(wlc_handle output);

// Sets a new resolution for an output
void output_set_resolution(wlc_handle output, const struct wlc_size *size);

// Brings up the numbered workspace on the currently active output.
// "num" should be the index in the workspace array, not the actual number.
void workspace_switch_to(uint32_t num);

// Recalculates frame geometries recursively.
void frame_recalc_geometries(struct frame *fr, struct wlc_geometry g);

// Redraws the views inside a frame by applying the right tiling mode.
void frame_redraw(struct frame *fr, bool realloc);

// Splits the currently focused frame and attaches two children.
void frame_add(enum direction_t side);

// Selects a frame in a direction.
void frame_select(struct frame *fr, enum direction_t dir);

// Resizes a frame by a percentage.
void frame_resize_percent(enum direction_t dir, float percent);

// Deletes a frame and move the orphaned views to a new parent.
void frame_delete();

// Called when a new child appears in wlc. Adds the new view to the currently
// active frame.
bool child_add(wlc_handle view);

// Called when a child is deleted. Removes the view from the currently active
// frame and focuses the next view.
void child_delete(wlc_handle view);

// Focuses the next closest view/frame in a given direction.
void focus_direction(enum direction_t dir);

// Focuses a specific view.
void focus_view(wlc_handle view);

// Moves a view in a direction.
void move_direction(enum direction_t dir);

// Moves a view to a target workspace
void move_to_workspace(uint32_t num);

// Add a new workspace, but don't switch to it.
void workspace_add();

// Cylce through workspaces
void cycle_workspace(uint32_t next);

// Cycles through the view tiling modes.
void cycle_tiling_mode();

// Cycles the focus through the list of views in the currently active frame.
void cycle_view_in_frame();

void print_frame_tree();

void free_all_outputs();
void free_workspaces();

#endif
