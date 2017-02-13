#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <cairo/cairo.h>
#include <wlc/wlc.h>
#include <wlc/wlc-render.h>

#include "log.h"
#include "layout.h"
#include "vector.h"
#include "border.h"
#include "bar.h"

// find the index of a view in a frame
static uint32_t frame_get_index_of_view(struct frame *fr, wlc_handle view) {
    assert(fr->children->length > 0);
    uint32_t i;
    for (i = 0; *((wlc_handle *) fr->children->items[i]) != view; i++);
    return i;
}

// returns the i-th wlc_handle attached to a frame
static wlc_handle frame_get_view_i(struct frame *fr, uint32_t index) {
    return (*(wlc_handle *) fr->children->items[index]);
}

// find the index of a given output
static uint32_t output_get_index(wlc_handle handle) {
    assert(outputs->length > 0);
    uint32_t i;
    for (i = 0; ((struct output *) outputs->items[i])->output_handle != handle;
            i++);
    return i;
}

static struct frame *alloc_frame(uint32_t x, uint32_t y,
        uint32_t width, uint32_t height) {

    struct frame *fr = calloc(sizeof(struct frame), 1);
    if (!fr) {
        return NULL;
    }

    fr->split = SPLIT_NONE;
    fr->tile = 0;
    fr->parent = NULL;
    fr->left = NULL;
    fr->right = NULL;
    fr->last_focused = NULL;
    fr->children = vector_init();
    fr->border.buffer = NULL;
    fr->active_view = 0;
    fr->border.g.origin.x = x;
    fr->border.g.origin.y = y;
    fr->border.g.size.w = width;
    fr->border.g.size.h = height;
    fr->rel_size = 1.0;

    return fr;
}

// TODO this is very bad
static void frame_apply_gaps(struct frame *fr) {
    uint32_t gap = config->frame_gaps_size;
    uint32_t g1 = gap/2;
    uint32_t g2 = 2*gap - gap/2;
    fr->border.g_gaps = fr->border.g;
    if (!fr->parent) {
        return;
    }

    bool left = (fr->parent->left == fr);
    if (fr->parent->split == SPLIT_HORIZONTAL) {
        fr->border.g_gaps = fr->border.g;
        if (left) {
            fr->border.g_gaps.origin.x += g1;
            fr->border.g_gaps.origin.y += g1;
            fr->border.g_gaps.size.h -= gap;
            fr->border.g_gaps.size.w -= g1;
        } else {
            fr->border.g_gaps.origin.x += g2;
            fr->border.g_gaps.origin.y += g1;
            fr->border.g_gaps.size.h -= gap;
            fr->border.g_gaps.size.w -= g2;
        }
    } else if (fr->parent->split == SPLIT_VERTICAL) {
        if (left) { // top
            fr->border.g_gaps.origin.x += g1;
            fr->border.g_gaps.origin.y += g1;
            fr->border.g_gaps.size.h -= g1;
            fr->border.g_gaps.size.w -= gap;
        } else { // bottom
            fr->border.g_gaps.origin.x += gap;
            fr->border.g_gaps.origin.y += g2;
            fr->border.g_gaps.size.h -= g2 + gap;
            fr->border.g_gaps.size.w -= gap;
        }
    }
}

static void free_frame(struct frame *fr) {
    vector_foreach(fr->children, free);
    vector_free(fr->children);
    free(fr->border.buffer);
    free(fr);
}

static void free_frame_tree(struct frame *fr) {
    if (!fr) {
        return;
    }

    free_frame_tree(fr->left);
    free_frame_tree(fr->right);
    free_frame(fr);
}

// apply a mask to all views in a frame and recursively to all subframes
static void frame_views_set_mask(struct frame *fr, uint32_t mask) {
    if (!fr) {
        return;
    }

    if (fr->split == SPLIT_NONE) {
        // if a frame is in fullscreen tiling mode, only the active view
        // is to be set visible.
        if (mask == 1 && config->tile_layouts[fr->tile] == TILE_FULLSCREEN) {
            wlc_view_set_mask(fr->active_view, mask);
            return;
        }

        for (uint32_t i = 0; i < fr->children->length; i++) {
            wlc_view_set_mask(frame_get_view_i(fr, i), mask);
        }
    } else {
        frame_views_set_mask(fr->left, mask);
        frame_views_set_mask(fr->right, mask);
    }
}

struct workspace *get_active_ws() {
    if (active_output) {
        return active_output->active_ws;
    }
    return NULL;
}

struct frame *get_active_frame() {
    if (active_output && active_output->active_ws) {
        return active_output->active_ws->active_frame;
    }
    return NULL;
}

wlc_handle get_active_view() {
    if (active_output && active_output->active_ws
        && active_output->active_ws->active_frame) {
        return active_output->active_ws->active_frame->active_view;
    }
    return 0;
}

struct frame *get_root_frame_by_output(wlc_handle output) {
    for (uint32_t i = 0; i < outputs->length; i++) {
        struct output *cur_out = outputs->items[i];
        if (cur_out->output_handle == output) {
            return cur_out->active_ws->root_frame;
        }
    }
    return NULL;
}

struct vector_t *get_workspaces() {
    return workspaces;
}

void schedule_render_all_outputs() {
    for (uint32_t i = 0; i < outputs->length; i++) {
        struct output *out = outputs->items[i];
        wlc_output_schedule_render(out->output_handle);
    }
}

struct vector_t *get_outputs() {
    return outputs;
}

// Allocates a new workspace
static struct workspace *alloc_next_workspace() {
    struct workspace *ws_new = malloc(sizeof(struct workspace));
    if (!ws_new) {
        wavy_log(LOG_ERROR, "Failed to allocate new workspace");
        return NULL;
    }

    /*
     * Allocate a new root frame for the workspace.
     * Initialize with width/height of 0, the real size will be calculated
     * once it is first assigned an output.
     */
    struct frame *fr_new = alloc_frame(0, 0, 0, 0);
    if (!fr_new) {
        wavy_log(LOG_ERROR, "Failed to allocate new frame");
        free(ws_new);
        return NULL;
    }

    // index of new workspace
    uint32_t num = workspaces->length;

    ws_new->root_frame = fr_new;
    ws_new->active_frame = fr_new;
    ws_new->number = num;
    ws_new->is_visible = false;
    ws_new->assigned_output = NULL;
    return ws_new;
}

void init_layout() {
    outputs = vector_init();
    workspaces = vector_init();

    for (uint32_t i = 0; i < 9; i++) {
        struct workspace *ws = alloc_next_workspace();
        if (ws) {
            vector_add(workspaces, ws);
        }
    }
}

static struct workspace *find_inactive_workspace() {
    for (uint32_t i = 0; i < workspaces->length; i++) {
        struct workspace *ws_i = ((struct workspace **) workspaces->items)[i];
        if (!ws_i->is_visible) {
            return ws_i;
        }
    }

    // no invisible workspaces are left (this is for people with > 9 monitors)
    return alloc_next_workspace();
}

static void free_output(struct output *out) {
    free_bar(&out->bar);
    free(out);
}

static struct wlc_geometry output_geometry_with_bar(uint32_t width,
        uint32_t height) {

    struct wlc_geometry g;
    g.origin.x = 0;
    g.origin.y = (config->statusbar_position == POS_BOTTOM) ? 0 :
         config->statusbar_height;
    g.size.w = width;
    g.size.h = height;
    return g;
}

static void workspace_assign_output(struct workspace *ws, struct output *out) {
    struct wlc_geometry g = output_geometry_with_bar(out->g.size.w,
            out->g.size.h);
    ws->assigned_output = out;
    frame_recalc_geometries(ws->root_frame, g);
    frame_redraw(ws->root_frame, true);
    wlc_output_schedule_render(out->output_handle);
}

static void output_update_resolution(struct output *out, uint32_t width,
        uint32_t height) {

    uint32_t bar_height = config->statusbar_height;
    out->g.size.w = width;
    out->g.size.h = height - bar_height;

    struct wlc_geometry g = output_geometry_with_bar(out->g.size.w,
            out->g.size.h);

    /*
     * All workspaces assigned to this output need to be recalculated, even
     * if not visible.
     */
    for (uint32_t i = 0; i < workspaces->length; i++) {
        struct workspace *ws = workspaces->items[i];
        if (ws->assigned_output == out) {
            frame_recalc_geometries(ws->root_frame, g);
        }
    }

    frame_redraw(out->active_ws->root_frame, true);
    out->bar.g.size.w = width;
    out->bar.dirty = true;
    update_bar(out);
    wlc_output_schedule_render(out->output_handle);
}

void add_output(wlc_handle output) {
    struct output *new_out = malloc(sizeof(struct output));
    if (!new_out) {
        wavy_log(LOG_ERROR, "Failed to allocate new output");
        return;
    }

    /*
     * Everytime the tty wavy runs on is activated, wlc recreates the output.
     * We test if the associated handle is already in an output struct, and
     * if so, we don't add the output again.
     */
    struct output *out = get_output_by_handle(output);
    if (out) {
        return;
    }

    const struct wlc_size *virt_res = wlc_output_get_virtual_resolution(output);
    new_out->output_handle = output;
    new_out->active_ws = find_inactive_workspace();
    new_out->active_ws->is_visible = true;
    init_bar(new_out);
    active_output = new_out;
    vector_add(outputs, new_out);
    workspace_assign_output(new_out->active_ws, new_out);
    output_update_resolution(new_out, virt_res->w, virt_res->h);
    wlc_output_focus(output);

    wavy_log(LOG_DEBUG, "%dx%d output added, handle = %lu",
            virt_res->w, virt_res->h, output);
}

void delete_output(wlc_handle output) {
    wavy_log(LOG_DEBUG, "Output %x deleted", output);
    struct output *out = get_output_by_handle(output);
    out->active_ws->is_visible = 0;
    out->active_ws->assigned_output = NULL;
    vector_del(outputs, output_get_index(out->output_handle));

    // set another output as the active output
    if (outputs->length == 0) {
        active_output = NULL;
    } else {
        // set the active output as the first output
        active_output = (struct output *) outputs->items[0];
    }

    free_output(out);
}

void output_set_resolution(wlc_handle output, const struct wlc_size *size) {
    wavy_log(LOG_DEBUG, "Setting resultion of output %lu to %ux%u", output,
            size->w, size->h);

    struct output *out = get_output_by_handle(output);
    if (!out) {
        return;
    }

    output_update_resolution(out, size->w, size->h);
}

void workspace_switch_to(uint32_t num) {
    struct workspace *cur_ws = get_active_ws();

    if (num > (workspaces->length - 1) ||  cur_ws->number == num) {
        return;
    }

    wavy_log(LOG_DEBUG, "Switching to workspace number %d", num + 1);

    frame_views_set_mask(cur_ws->root_frame, 0);
    cur_ws->is_visible = false;
    active_output->active_ws = workspaces->items[num];

    if (!(active_output->active_ws->assigned_output == active_output)) {
        workspace_assign_output(active_output->active_ws, active_output);
    }

    active_output->active_ws->is_visible = true;
    update_bar(active_output);
    frame_redraw(active_output->active_ws->root_frame, true);
    frame_views_set_mask(active_output->active_ws->root_frame, 1);
    wlc_view_focus(get_active_view());
    wlc_output_schedule_render(active_output->output_handle);
}

void frame_recalc_geometries(struct frame *fr, struct wlc_geometry g) {
    if (!fr) {
        return;
    }

    fr->border.g = g;
    frame_apply_gaps(fr);

    struct wlc_geometry g_left;
    struct wlc_geometry g_right;

    if (!fr->left || !fr->right) {
        return;
    }

    if (fr->split == SPLIT_NONE) {
        return;
    } else if (fr->split == SPLIT_HORIZONTAL) {
        uint32_t split = fr->left->rel_size * g.size.w;
        g_left.origin.x = g.origin.x;
        g_left.origin.y = g.origin.y;
        g_left.size.w = split;
        g_left.size.h = g.size.h;
        g_right.origin.x = g.origin.x + split;
        g_right.origin.y = g.origin.y;
        g_right.size.w = g.size.w - split;
        g_right.size.h = g.size.h;
    } else {
        uint32_t split = fr->left->rel_size * g.size.h;
        g_left.origin.x = g.origin.x;
        g_left.origin.y = g.origin.y;
        g_left.size.w = g.size.w;
        g_left.size.h = split;
        g_right.origin.x = g.origin.x;
        g_right.origin.y = g.origin.y + split;
        g_right.size.w = g.size.w;
        g_right.size.h = g.size.h - split;
    }

    frame_recalc_geometries(fr->left, g_left);
    frame_recalc_geometries(fr->right, g_right);
}

static void set_view(struct frame *fr, wlc_handle view,
        struct wlc_geometry *g) {

    uint32_t view_border = config->view_border_size;
    struct wlc_geometry g_border;

    // Geometry for the view border. Offset inside the frame buffer by x and y,
    // relative to the frame, not the output!
    g_border.origin.x = g->origin.x - fr->border.g_gaps.origin.x;
    g_border.origin.y = g->origin.y - fr->border.g_gaps.origin.y;
    g_border.size.w = g->size.w;
    g_border.size.h = g->size.h;

    // Geometry for the view itself, adjusted to fit inside the view frame.
    g->origin.x += view_border;
    g->origin.y += view_border;
    g->size.w -= 2*view_border;
    g->size.h -= 2*view_border;

    wlc_view_set_mask(view, 1);
    update_view_border(fr, view, &g_border);
    wlc_view_set_geometry(view, 0, g);
}

void frame_redraw(struct frame *fr, bool realloc) {
    if (!fr) {
        return;
    }

    // special case for the root frame: it has SPLIT_NONE but no parent
    if (fr->split != SPLIT_NONE || (fr->split == SPLIT_NONE && !(fr->parent))) {
        frame_redraw(fr->left, realloc);
        frame_redraw(fr->right, realloc);
    }

    if (fr->split == SPLIT_NONE) {
        update_frame_border(fr, realloc);
    }

    // do nothing when there are no child views
    if (!fr->children || fr->children->length == 0) {
        // the hook needs to be triggered so left-over bar titles or layout
        // indicators can be redrawn (removed) properly
        trigger_hook(HOOK_VIEW_UPDATE);
        return;
    }

    uint32_t frame_border = config->frame_border_size;

    // a geometry adjusted for frame borders to work with
    struct wlc_geometry fr_g = {
        .origin = {
            .x = fr->border.g_gaps.origin.x + frame_border,
            .y = fr->border.g_gaps.origin.y + frame_border
        },
        .size = {
            .w = fr->border.g_gaps.size.w - 2 * frame_border,
            .h = fr->border.g_gaps.size.h - 2 * frame_border
        }
    };

    if (config->tile_layouts[fr->tile]  == TILE_VERTICAL) {
        uint32_t h_div = fr_g.size.h / fr->children->length;

        struct wlc_geometry g;
        for (uint32_t i = 0; i < fr->children->length; i++) {
            wlc_handle v = frame_get_view_i(fr, i);
            g.origin.x = fr_g.origin.x;
            g.origin.y = fr_g.origin.y + i*h_div;
            g.size.w = fr_g.size.w;
            g.size.h = (i+1 == fr->children->length) ?
                       (fr_g.size.h - i*h_div) : h_div;
            set_view(fr, v, &g);
        }

    } else if (config->tile_layouts[fr->tile] == TILE_HORIZONTAL) {
        uint32_t v_div = fr_g.size.w / fr->children->length;

        struct wlc_geometry g;
        for (uint32_t i = 0; i < fr->children->length; i++) {
            wlc_handle v = frame_get_view_i(fr, i);
            g.origin.y = fr_g.origin.y;
            g.origin.x = fr_g.origin.x + i*v_div;
            g.size.h = fr_g.size.h;
            g.size.w = (i+1 == fr->children->length) ?
                       (fr_g.size.w - i*v_div) : v_div;
            set_view(fr, v, &g);
        }

    } else if (config->tile_layouts[fr->tile] == TILE_GRID) {
        uint32_t len = fr->children->length;
        uint32_t cols = (uint32_t) ceilf(sqrtf(len));
        uint32_t rows = (len / cols) + (len % cols ? 1 : 0);
        uint32_t div_w = fr_g.size.w / cols;
        uint32_t div_h = fr_g.size.h / rows;

        uint32_t c = 0;
        struct wlc_geometry g;
        for (uint32_t i = 0; i < rows; i++) {
            for (uint32_t j = 0; j < cols && c < len; j++) {
                wlc_handle v = frame_get_view_i(fr, j + i*cols);
                g.origin.x = fr_g.origin.x + j*div_w;
                g.origin.y = fr_g.origin.y + i*div_h;
                g.size.h = (i == rows - 1) ? fr_g.size.h - i*div_h : div_h;
                g.size.w = (c == len - 1) ? fr_g.size.w - j*div_w :
                           ((j+1) % cols == 0) ?
                           fr_g.size.w - (cols - 1)*div_w : div_w;
                set_view(fr, v, &g);
                c++;
            }
        }

    } else if (config->tile_layouts[fr->tile] == TILE_FULLSCREEN) {
        wlc_handle active = fr->active_view;
        for (uint32_t i = 0; i < fr->children->length; i++) {
            wlc_handle v = frame_get_view_i(fr, i);
            if (v == active) {
                struct wlc_geometry g;
                g.origin.x = fr_g.origin.x;
                g.origin.y = fr_g.origin.y;
                g.size.w = fr_g.size.w;
                g.size.h = fr_g.size.h;
                set_view(fr, v, &g);
            } else {
                wlc_view_set_mask(v, 0);
            }
        }

    // TODO: fix off-by-one errors
    } else if (config->tile_layouts[fr->tile] == TILE_FIBONACCI) {
        struct wlc_geometry g;
        uint32_t n = fr->children->length;
        uint32_t nx = fr_g.origin.x;
        uint32_t ny = fr_g.origin.y;
        uint32_t nw = fr_g.size.w;
        uint32_t nh = fr_g.size.h;
        uint32_t w_adjust = 0;
        uint32_t h_adjust = 0;

        for (uint32_t i = 0; i < n; i++) {
            wlc_handle v = frame_get_view_i(fr, i);

            if (i < n-1 && i > 0) {
                if (i % 2 == 1) {
                    (nh % 2) && (h_adjust = 1);
                    nh /= 2;
                } else {
                    (nw % 2) && (w_adjust = 1);
                    nw /= 2;
                }

                if (i % 4 == 2) {
                    nx += nw;
                } else if (i % 4 == 3) {
                    ny += nh;
                }
            }

            if (i % 4 == 0) {
                ny -= nh;
            } else if (i % 4 == 1) {
                nx += nw;
            } else if (i % 4 == 2) {
                ny += nh;
            } else {
                nx -= nw;
            }

            if (i == 0) {
                if (n != 1) {
                    nw = fr_g.size.w * 0.5;
                }
                ny = fr_g.origin.y;
            } else if (i == 1) {
                nw = fr_g.size.w - nw;
            }

            if (w_adjust) {
                nw += w_adjust;
                w_adjust = 0;
            }

            if (h_adjust) {
                nh += h_adjust;
                h_adjust = 0;
            }

            g.origin.x = nx;
            g.origin.y = ny;
            g.size.w = nw;
            g.size.h = nh;
            set_view(fr, v, &g);
        }
    }

    trigger_hook(HOOK_VIEW_UPDATE);
}

void frame_add(enum direction_t s) {
    struct frame *fr = get_active_frame();

    // Allocate new left leaf. We don't use alloc_frame because we copy the
    // previous frame to this.
    struct frame *new_left = calloc(sizeof(struct frame), 1);
    if (!new_left) {
        wavy_log(LOG_ERROR, "Failed to allocate new frame");
        return;
    }

    // copy fr over to the new left leaf
    *new_left = *fr;
    new_left->parent = fr;

    // these are used to initialize the new right child
    int32_t new_x, new_y, new_width, new_height;
    int32_t halved;

    // horizontal axis is split
    if (s == DIR_RIGHT) {
        halved = fr->border.g.size.w / 2;
        new_x = fr->border.g.origin.x + halved;
        new_y = fr->border.g.origin.y;
        new_width = fr->border.g.size.w - halved;
        new_height = fr->border.g.size.h;

        // x,y position of left frame stays the same
        new_left->border.g.size.w = halved;
        new_left->rel_size = 0.5;

        // fr->split is no longer NONE
        fr->split = SPLIT_HORIZONTAL;

    // vertical axis is split
    } else {
        halved = fr->border.g.size.h / 2;
        new_x = fr->border.g.origin.x;
        new_y = fr->border.g.origin.y + halved;
        new_width = fr->border.g.size.w;
        new_height = fr->border.g.size.h - halved;

        // x,y position of left frame stays the same
        new_left->border.g.size.h = halved;
        new_left->rel_size = 0.5;

        // fr->split is no longer NONE
        fr->split = SPLIT_VERTICAL;
    }

    // setup of left leaf that applies to both cases
    new_left->border.buffer = NULL;
    new_left->border.g = new_left->border.g;
    new_left->last_focused = NULL;

    // setup the new right leaf
    struct frame *new_right = alloc_frame(new_x, new_y, new_width, new_height);
    if (!new_right) {
        wavy_log(LOG_ERROR, "Failed to allocate new frame");
        free(new_left); // clean up the already allocated memory on failure
        return;
    }

    new_right->border.g = new_right->border.g;
    new_right->parent = fr;
    new_right->rel_size = 0.5;

    // adjust the old leaf frame for it's new purpose as a node.
    free(fr->border.buffer);
    fr->border.buffer = NULL;
    fr->left = new_left;
    fr->right = new_right;
    fr->children = NULL;

    // set the gaps
    frame_apply_gaps(new_right);
    frame_apply_gaps(new_left);

    // set the new active frame. make this configurable in the future?
    // (this currently keeps focus on the old frame)
    active_output->active_ws->active_frame = fr->left;
    fr->last_focused = fr->left;

    frame_redraw(fr, true);
    wlc_output_schedule_render(active_output->output_handle);

    if (debug_enabled) {
        print_frame_tree(active_output->active_ws->root_frame);
    }
}

// returns the frame to be selected next by following the last_focused pointers.
static struct frame *find_frame_selection(struct frame *fr,
        enum direction_t dir) {

    if (!fr) {
        return NULL;
    }

    struct frame *sel = (dir == DIR_RIGHT) ? fr->right : fr->left;
    for (; sel && sel->split != SPLIT_NONE; sel = sel->last_focused);
    return sel;
}

/*
 * Finds the parent frame that has the given split type. "dir" specifies the
 * direction to come from, so we can avoid finding the frame that would lead
 * back to the starting frame, when we traverse it back down later.
 */
static struct frame *find_parent_by_split(struct frame *fr,
        enum frame_split_t sp, enum direction_t dir) {

    if (!fr || !fr->parent) {
        return NULL;
    }

    struct frame *sel = NULL;
    struct frame *cur = fr;
    while (cur && cur->parent) {
        bool applicable = !(cur == cur->parent->left && dir == DIR_LEFT) &&
                          !(cur == cur->parent->right && dir == DIR_RIGHT);
        cur = cur->parent;
        if (cur->split == sp && applicable) {
            sel = cur;
            break;
        }
    }

    return sel;
}

// finds a parent with the given split type.
static struct frame *find_parent_by_split_simple(struct frame *fr,
        enum frame_split_t sp) {

    if (!fr || !fr->parent) {
        return NULL;
    }

    struct frame *cur = fr->parent;
    for (; cur && cur->split != sp; cur = cur->parent);
    return cur;
}

void frame_resize_percent(enum direction_t dir, float percent) {
    struct frame *fr = get_active_frame();
    if (!fr->parent) {
        return;
    }

    struct frame *resize_p;

    switch (dir) {
    case DIR_UP:
        resize_p = find_parent_by_split_simple(fr, SPLIT_VERTICAL);
        if (!resize_p) {
            return;
        }
        resize_p->left->rel_size -= percent;
        resize_p->right->rel_size = 1 - resize_p->left->rel_size;
        break;
    case DIR_DOWN:
        resize_p = find_parent_by_split_simple(fr, SPLIT_VERTICAL);
        if (!resize_p) {
            return;
        }
        resize_p->left->rel_size += percent;
        resize_p->right->rel_size = 1 - resize_p->left->rel_size;
        break;
    case DIR_LEFT:
        resize_p = find_parent_by_split_simple(fr, SPLIT_HORIZONTAL);
        if (!resize_p) {
            return;
        }
        resize_p->left->rel_size -= percent;
        resize_p->right->rel_size = 1 - resize_p->left->rel_size;
        break;
    case DIR_RIGHT:
        resize_p = find_parent_by_split_simple(fr, SPLIT_HORIZONTAL);
        if (!resize_p) {
            return;
        }
        resize_p->left->rel_size += percent;
        resize_p->right->rel_size = 1 - resize_p->left->rel_size;
        break;
    }

    frame_recalc_geometries(resize_p, resize_p->border.g);
    frame_redraw(resize_p, true);
    wlc_output_schedule_render(active_output->output_handle);
}

void frame_delete() {
    struct frame *fr = get_active_frame();

    if (!fr || !fr->parent) {
        return;
    }

    struct frame *brother = (fr->parent->left == fr) ?
                            fr->parent->right :
                            fr->parent->left;

    // brother node is also a leaf
    if (brother->split == SPLIT_NONE) {
        for (uint64_t i = 0; i < fr->children->length; i++) {
            vector_add(brother->children, fr->children->items[i]);
        }

        brother->border.g = fr->parent->border.g;
        frame_apply_gaps(brother);
        brother->rel_size = fr->parent->rel_size;
        brother->parent = fr->parent->parent;

        *fr->parent = *brother;
        active_output->active_ws->active_frame = fr->parent;
        wlc_output_schedule_render(active_output->output_handle);

        // the brother node might have never had a view attached
        if (!get_active_view()) {
            if (fr->children->length > 0) {
                fr->parent->active_view = fr->active_view;
            }
        }

        frame_redraw(fr->parent, true);
        wlc_view_focus(get_active_view());

        vector_free(fr->children);
        free(fr->border.buffer);
        free(fr);
        free(brother);

    /*
     * Brother node is a subtree: the subtree needs to take the position of
     * fr's parent and all sub-geometries need to be recalculated.
     */
    } else {
        enum direction_t dir = (fr->parent->left == fr) ? DIR_RIGHT : DIR_LEFT;
        struct frame *new_leaf = find_frame_selection(fr->parent, dir);

        for (uint64_t i = 0; i < fr->children->length; i++) {
            vector_add(new_leaf->children, fr->children->items[i]);
        }

        brother->border.g = fr->parent->border.g;
        frame_apply_gaps(brother);
        brother->rel_size = fr->parent->rel_size;
        brother->parent = fr->parent->parent;
        brother->left->parent = fr->parent;
        brother->right->parent = fr->parent;

        *fr->parent = *brother;
        active_output->active_ws->active_frame = new_leaf;
        frame_recalc_geometries(fr->parent, fr->parent->border.g);
        wlc_output_schedule_render(active_output->output_handle);
        frame_redraw(fr->parent, true);
        wlc_view_focus(get_active_view());

        vector_free(fr->children);
        free(fr->border.buffer);
        free(fr);
        free(brother);
    }

    if (debug_enabled) {
        print_frame_tree(active_output->active_ws->root_frame);
    }
}

// internal recursive function
static struct frame *_frame_by_view(struct frame *fr, wlc_handle view) {
    if (!fr) {
        return NULL;
    }

    // fr is a leaf node
    if (fr->split == SPLIT_NONE) {
        if (fr->children && fr->children->length != 0) {
            for (uint32_t i = 0; i < fr->children->length; i++) {
                if (frame_get_view_i(fr, i) == view) {
                    return fr;
                }
            }
        }

        // fr is empty
        return NULL;
    }

    struct frame *fr_left = _frame_by_view(fr->left, view);
    struct frame *fr_right = _frame_by_view(fr->right, view);

    // a view is never in two frame at once
    return fr_left ? fr_left : fr_right;
}

struct frame *frame_by_view(wlc_handle view) {
    wlc_handle out_handle = wlc_view_get_output(view);
    struct output *out;

    for (uint32_t i = 0; i < outputs->length; i++) {
        out = outputs->items[i];
        if (out_handle == out->output_handle) {
            break;
        }
    }

    return _frame_by_view(out->active_ws->root_frame, view);
}

struct frame *frame_by_view_global(wlc_handle view) {
    struct frame *fr = NULL;
    for (uint32_t i = 0; i < workspaces->length; i++) {
        struct workspace *ws = workspaces->items[i];
        fr = _frame_by_view(ws->root_frame, view);
        if (fr) {
            break;
        }
    }

    return fr;
}


struct output *get_output_by_handle(wlc_handle view) {
    for (uint32_t i = 0; i < outputs->length; i++) {
        if (((struct output *) outputs->items[i])->output_handle == view) {
            return (struct output *) outputs->items[i];
        }
    }
    return NULL;
}

bool child_add(wlc_handle view) {
    struct frame *fr = get_active_frame();

    wlc_handle *view_ptr = malloc(sizeof(wlc_handle *));
    if (!view_ptr) {
        wavy_log(LOG_ERROR, "Failed to allocate memory for new view");
        return false;
    }

    *view_ptr = view;

    uint32_t i = (fr->children->length == 0) ? 0 :
        frame_get_index_of_view(fr, fr->active_view) + 1;
    vector_insert(fr->children, view_ptr, i);

    fr->active_view = view;
    wlc_view_focus(view);
    frame_redraw(fr, false);
    return true;
}

void child_delete(wlc_handle view) {
    wlc_handle next_view;

    /*
     * The deleted view is the currently focused view. We need to find another
     * view to be the active view.
     */
    if (view ==  get_active_view()) {
        struct frame *fr = get_active_frame();

        // find predecessor of view
        uint32_t i = frame_get_index_of_view(fr, view);
        next_view = fr->children->length == 1 ? 0 :
                    (i > 0) ? frame_get_view_i(fr, i -  1) :
                    frame_get_view_i(fr, 1);

        vector_del(fr->children, i);
        fr->active_view = next_view;
        frame_redraw(fr, false);

    /*
     * View is closed by crash or killing the process, possibly on an invisible
     * workspace. The same active view is kept.
     */
    } else {
        struct frame *fr = frame_by_view_global(view);
        uint32_t i = frame_get_index_of_view(fr, view);
        next_view = fr->active_view; // keep the same active view
        vector_del(fr->children, i);
        frame_redraw(fr, false);
    }

    wlc_view_focus(next_view);
}

/*
 * Finds the frame that is adjacent in direction dir to the frame fr and
 * returns NULL if there is none.
 */
static struct frame *find_adjacent_frame(struct frame *fr,
        enum direction_t dir) {

    if (!fr) {
        return NULL;
    }

    struct frame *fr_p;
    struct frame *new_sel_fr;

    switch (dir) {
    case DIR_UP:
        fr_p = find_parent_by_split(fr, SPLIT_VERTICAL, DIR_LEFT);
        new_sel_fr = find_frame_selection(fr_p, DIR_LEFT);
        break;
    case DIR_DOWN:
        fr_p = find_parent_by_split(fr, SPLIT_VERTICAL, DIR_RIGHT);
        new_sel_fr = find_frame_selection(fr_p, DIR_RIGHT);
        break;
    case DIR_LEFT:
        fr_p = find_parent_by_split(fr, SPLIT_HORIZONTAL, DIR_LEFT);
        new_sel_fr = find_frame_selection(fr_p, DIR_LEFT);
        break;
    case DIR_RIGHT:
        fr_p = find_parent_by_split(fr, SPLIT_HORIZONTAL, DIR_RIGHT);
        new_sel_fr = find_frame_selection(fr_p, DIR_RIGHT);
        break;
    }

    return new_sel_fr;
}

/*
 * Finds the view that is adjacent in a direction dir to the currentyl active
 * view in frame fr and returns 0 if there is none.
 */
static wlc_handle find_adjacent_view(struct frame *fr, enum direction_t dir) {
    if (!fr) {
        return 0;
    }

    wlc_handle adj = 0;
    switch (config->tile_layouts[fr->tile]) {
    case TILE_VERTICAL:
        switch (dir) {
        case DIR_UP:
            if (fr->children->length != 0) {
                uint32_t i = frame_get_index_of_view(fr, fr->active_view);
                if (i > 0) {
                    adj = frame_get_view_i(fr, i - 1);
                }
            }
            goto exit_case;
        case DIR_DOWN:
            if (fr->children->length != 0) {
                uint32_t i = frame_get_index_of_view(fr, fr->active_view);
                if (i < fr->children->length - 1) {
                    adj = frame_get_view_i(fr, i + 1);
                }
            }
            goto exit_case;
        case DIR_LEFT:
        case DIR_RIGHT:
            goto exit_case;
        }
    case TILE_HORIZONTAL:
        switch (dir) {
        case DIR_UP:
        case DIR_DOWN:
            goto exit_case;
        case DIR_LEFT:
            if (fr->children->length != 0) {
                uint32_t i = frame_get_index_of_view(fr, fr->active_view);
                if (i > 0) {
                    adj = frame_get_view_i(fr, i - 1);
                }
            }
            goto exit_case;
        case DIR_RIGHT:
            if (fr->children->length != 0) {
                uint32_t i = frame_get_index_of_view(fr, fr->active_view);
                if (i < fr->children->length - 1) {
                    adj = frame_get_view_i(fr, i + 1);
                }
            }
            goto exit_case;
        }
    case TILE_FULLSCREEN:
        goto exit_case;
    case TILE_GRID:
        {
        uint32_t len = fr->children->length;
        uint32_t cols = (uint32_t) ceilf(sqrtf(len));
        switch (dir) {
        case DIR_UP:
            if (fr->children->length != 0) {
                uint32_t i = frame_get_index_of_view(fr, fr->active_view);
                if (i >= cols) {
                    adj = frame_get_view_i(fr, i - cols);
                }
            }
            goto exit_case;
        case DIR_DOWN:
            if (fr->children->length != 0) {
                uint32_t i = frame_get_index_of_view(fr, fr->active_view);
                if (i <= (len - cols)) {
                    adj = ((i + cols) < len) ? frame_get_view_i(fr, i + cols) :
                                               frame_get_view_i(fr, len - 1);
                }
            }
            goto exit_case;
        case DIR_LEFT:
            if (fr->children->length != 0) {
                uint32_t i = frame_get_index_of_view(fr, fr->active_view);
                if (i % cols > 0) {
                    adj = frame_get_view_i(fr, i - 1);
                }
            }
            goto exit_case;
        case DIR_RIGHT:
            if (fr->children->length != 0) {
                uint32_t i = frame_get_index_of_view(fr, fr->active_view);
                if (i % cols < (cols - 1) && i < len - 1) {
                    adj = frame_get_view_i(fr, i + 1);
                }
            }
            goto exit_case;
        }
        }
    default: // TODO: fibonacci
        goto exit_case;
    }

exit_case:
    return adj;
}

void focus_direction(enum direction_t dir) {
    struct frame *fr = get_active_frame();
    wlc_handle adj_view = find_adjacent_view(fr, dir);

    // a target view was found
    if (adj_view) {
        fr->active_view = adj_view;
        wlc_view_focus(fr->active_view);
        frame_redraw(fr, false);
        wlc_output_schedule_render(active_output->output_handle);
        return;
    }

    // try to find a target frame instead
    struct frame *adj_fr = find_adjacent_frame(fr, dir);
    if (adj_fr) {
        active_output->active_ws->active_frame = adj_fr;
        adj_fr->parent->last_focused = adj_fr;
        frame_redraw(fr, false);
        frame_redraw(adj_fr, false);
        wlc_output_schedule_render(active_output->output_handle);
        wlc_view_focus(get_active_view());
    }
}

void focus_view(wlc_handle view) {
    if (!view) {
        return;
    }

    if (get_active_view() == view) {
        return;
    }

    struct frame *old_fr = get_active_frame();
    struct frame *new_fr = frame_by_view_global(view);
    if (!new_fr)  {
        return;
    }
    wlc_handle output = wlc_view_get_output(view);
    struct output *out = get_output_by_handle(output);
    active_output = out;
    out->active_ws->active_frame = new_fr;
    new_fr->active_view = view;
    wlc_view_focus(view);
    frame_redraw(new_fr, false);
    if (old_fr != new_fr) {
        frame_redraw(old_fr, false);
    }
}

void move_direction(enum direction_t dir) {
    struct frame *fr = get_active_frame();
    wlc_handle adj_view = find_adjacent_view(fr, dir);

    // a target view was found
    if (adj_view != 0) {
        uint32_t a = frame_get_index_of_view(fr, get_active_view());
        uint32_t b = frame_get_index_of_view(fr, adj_view);

        wlc_handle *tmp = fr->children->items[a];
        fr->children->items[a] = fr->children->items[b];
        fr->children->items[b] = tmp;

        fr->active_view = *tmp;
        frame_redraw(fr, false);
        wlc_view_focus(fr->active_view);
        return;
    }

    // try to find a target frame instead
    struct frame *adj_fr = find_adjacent_frame(get_active_frame(), dir);
    if (adj_fr) {
        wlc_handle v = get_active_view();
        child_delete(v);
        active_output->active_ws->active_frame = adj_fr;
        frame_redraw(fr, false);
        child_add(v);
        wlc_output_schedule_render(active_output->output_handle);
    }
}

void move_to_workspace(uint32_t num) {
    if (active_output->active_ws->number == num) {
        return;
    }

    struct frame *fr = get_active_frame();
    if (!fr || fr->children->length == 0) {
        return;
    }

    uint32_t i = frame_get_index_of_view(fr, get_active_view());
    wlc_handle *v = fr->children->items[i];

    // add the view to the target workspace and make it the active view of it
    struct workspace *target_ws = workspaces->items[num];
    vector_add(target_ws->active_frame->children, v);
    target_ws->active_frame->active_view = *v;

    // find the next view to focus
    wlc_handle next_view = fr->children->length == 1 ? 0 :
                           (i > 0) ? frame_get_view_i(fr, i -  1) :
                           frame_get_view_i(fr, 1);
    fr->active_view = next_view;

    // delete view from the current frame and make it invisible
    vector_del(fr->children, i);
    wlc_view_set_mask(*v, 0);

    frame_redraw(fr, false);
    wlc_view_focus(next_view);
    wlc_output_schedule_render(active_output->output_handle);
}

void workspace_add() {
    struct workspace *ws = alloc_next_workspace();
    if (ws) {
        vector_add(workspaces, ws);
    }
    update_bar(active_output);
    wlc_output_schedule_render(active_output->output_handle);
}

void workspace_next() {
    if (active_output->active_ws->number == workspaces->length) {
        return;
    }
    wavy_log(LOG_DEBUG, "Selecting the next workspace");
    workspace_switch_to(active_output->active_ws->number + 1);
}

void workspace_prev() {
    if (active_output->active_ws->number == 0) {
        return;
    }
    wavy_log(LOG_DEBUG, "Selecting the previous workspace");
    workspace_switch_to(active_output->active_ws->number - 1);
}

void cycle_tiling_mode() {
    struct frame *fr = get_active_frame();
    fr->tile = (fr->tile + 1) % config->num_layouts;
    frame_redraw(fr, false);
}

// fwd: 1 := forward, 0 := backward
void cycle_view_in_frame(uint32_t fwd) {
    struct frame *fr = get_active_frame();
    uint32_t len = fr->children->length;
    if (len == 0) {
        return;
    }

    int32_t step = fwd ? 1 : -1;
    int32_t cur_index = frame_get_index_of_view(fr, fr->active_view);
    int32_t next_index;

    // careful: % can return a negative value.
    if (cur_index + step < 0) {
        next_index = ((cur_index + step) + len) % len;
    } else {
        next_index = (cur_index + step) % len;
    }

    fr->active_view = frame_get_view_i(fr, next_index);
    wlc_view_focus(fr->active_view);
    frame_redraw(fr, false);
    wlc_output_schedule_render(active_output->output_handle);
}

// internal recursive function that prints the frame tree
static void _print_frame_tree(struct frame *fr, int indent) {
    if (fr) {
        if (fr->right) {
            _print_frame_tree(fr->right, indent + 4);
        }
        if (indent) {
            for (int i=0; i<indent; i++) {
                printf(" ");
            }
        }
        if (fr->right) {
            printf(" /\n");
            for (int i=0; i<indent; i++) {
                printf(" ");
            }
        }
        if (fr->split == SPLIT_HORIZONTAL) {
            printf("H (%p)\n", (void *) fr);
        } else if (fr->split == SPLIT_VERTICAL) {
            printf("V (%p)\n", (void *) fr);
        } else {
            printf("(%p)\n", (void *) fr);
        }
        if (fr->left) {
            for (int i=0; i<indent; i++) {
                printf(" ");
            }
            printf(" \\\n");
            _print_frame_tree(fr->left, indent + 4);
        }
    }
}

void print_frame_tree(struct frame *fr) {
    printf("\nCurrent frame tree (printed sideways): \n\n");
    _print_frame_tree(fr, 0);
    printf("\n");
}

void free_all_outputs() {
    for (uint32_t i = 0; i < outputs->length; i++) {
        struct output *out = outputs->items[i];
        free_output(out);
    }
    vector_free(outputs);
}

void free_workspaces(){
    for (uint32_t i = 0; i < workspaces->length; i++) {
        struct workspace *ws = workspaces->items[i];
        free_frame_tree(ws->root_frame);
    }
    vector_free(workspaces);
}
