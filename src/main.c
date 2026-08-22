#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fastgraph_font.h"

#define APP_TITLE L"Shaped -Shape Editor- Carl N Graham -Argonaut Software- 1991-1992"
#define MAX_DOTS 500
#define MAX_POLYS 500
#define MAX_POLY_VERTS 16
#define MAX_FRAMES 128
#define MAX_FILE_ENTRIES 240
#define MAX_SNES_ENTRIES 24
#define ID_FILE_OPEN 100
#define ID_FILE_SAVE 101
#define ID_FILE_EXIT 102
#define ID_FILE_SAVE_INTERNAL 103
#define ID_FILE_SAVE_INTERNAL_AS 104
#define ID_EDIT_UNDO 110
#define ID_EDIT_SELECT_ALL 111
#define ID_EDIT_SELECT_NONE 112
#define ID_EDIT_DELETE 113
#define ID_SHAPE_ADD_DOT 120
#define ID_SHAPE_MIRROR_X 121
#define ID_SHAPE_MIRROR_Y 122
#define ID_SHAPE_MIRROR_Z 123
#define ID_VIEW_GRID_1 131
#define ID_VIEW_GRID_10 132
#define ID_VIEW_GRID_100 133
#define ID_VIEW_ZOOM_IN 134
#define ID_VIEW_ZOOM_OUT 135
#define ID_ANIM_PREV 150
#define ID_ANIM_NEXT 151
#define ID_EDIT_MOVE 160
#define ID_EDIT_COPY 161
#define ID_EDIT_SIZE 162
#define ID_EDIT_ROTATE 163
#define ID_EDIT_COMPACT 164
#define ID_ANIM_SHIFT 165
#define ID_DOT_MODE 166
#define ID_SHAPE_MIRROR_DIALOG 167
#define ID_MIRROR_X 2001
#define ID_MIRROR_Y 2002
#define ID_MIRROR_Z 2003
#define ID_MIRROR_SELECTED 2004
#define ID_MIRROR_ADD 2005
#define ID_NUMBER_EDIT 2010
#define ID_SELECTBY_LIST 2020
#define ID_SCALE_ALL 2040
#define ID_SCALE_X 2041
#define ID_SCALE_Y 2042
#define ID_SCALE_Z 2043
#define ID_SCALE_SELECTED 2044
#define ID_SCALE_FRAMES 2045
#define ID_FRAMES_LIST 2050

typedef struct {
    double x, y, z;
    uint8_t selected;
} Dot;
typedef struct {
    double x, y, z;
    uint16_t active;
} FrameDot;
typedef struct {
    uint16_t count, index[MAX_POLY_VERTS], colour, type, flags;
    uint8_t selected;
} Poly;
typedef struct {
    Dot dots[MAX_DOTS];
    Poly polys[MAX_POLYS];
    FrameDot *frames[MAX_FRAMES];
    size_t dot_count, poly_count, frame_count;
} Shape;
typedef struct {
    RECT r;
    int axes[2], signs[2];
    const wchar_t *name;
} View;
typedef struct {
    const wchar_t *label;
    int command;
} MainButton;
typedef struct {
    int poly, front, back, leaf;
} BSPNode;
typedef struct {
    wchar_t name[MAX_PATH];
    uint8_t directory;
} FileEntry;
typedef struct {
    char name[50];
    wchar_t label[50];
    int value;
} SNESEntry;
enum { MENU_NONE,
       MENU_PLANE,
       MENU_GRID,
       MENU_GROUP,
       MENU_POLY,
       MENU_ZOOM,
       MENU_SNES,
       MENU_SHOW,
       MENU_SAVE,
       MENU_ANIM,
       MENU_TEST,
       MENU_TYPE,
       MENU_SETGROUP,
       MENU_SHOWGROUP,
       MENU_FRAMES,
       MENU_BSPTEST,
       MENU_PALNUM,
       MENU_SELECTBY,
       MENU_COLTAB,
       MENU_PALETTE,
       MENU_TEXTURE };
enum { PROMPT_NONE,
       PROMPT_QUIT,
       PROMPT_ROTATE_ADD,
       PROMPT_NEW,
       PROMPT_MIRROR,
       PROMPT_SCALE,
       PROMPT_FRAMES,
       PROMPT_SELECT_POLYS,
       PROMPT_NUMBER };
enum { NUMBER_NONE,
       NUMBER_DEFAULT_COLOUR,
       NUMBER_POLYGON_COLOUR };

static HWND g_hwnd;
static Shape g_shape, g_undo;
static int g_has_undo;
static int g_last_load_opened;
static double g_zoom = 1.0;
static double g_grid = 10.0;
static double g_origin[3];
static size_t g_current_frame;
static int g_active_menu, g_add_dot_mode;
static int g_tool, g_dragging, g_drag_view;
static int g_rotate_stage;
static int g_preview_mode;
static double g_preview_angles[3];
static double g_preview_scale = 500.0, g_preview_distance = 400.0;
static size_t g_preview_frame;
static int g_preview_hidden = 1;
static int g_show_all_frames;
static int g_dos_prompt;
static int g_cursor_overlay;
static double g_cursor_coords[3];
/* DOS c4: the projection paired with the pane currently driving the 3-D cursor.
   1=ZY (upper left), 2=ZX (lower left), 3=XY (upper right). */
static int g_cursor_projection;
static uint8_t g_mirror_options[5];
static wchar_t g_scale_text[4][19];
static uint8_t g_scale_options[2];
static int g_scale_active_edit = -1, g_scale_replace_text;
static wchar_t g_frames_text[3][19];
static int g_frames_action, g_frames_active_edit = -1, g_frames_replace_text;
static wchar_t g_selectpolys_labels[11][32];
static int g_selectpolys_action;
static wchar_t g_number_title[32], g_number_label[32], g_number_text[19];
static int g_number_mode, g_number_active_edit, g_number_replace_text;
static FileEntry g_file_entries[MAX_FILE_ENTRIES];
static size_t g_file_count;
static int g_file_selector, g_file_result, g_file_save, g_file_page, g_file_active_edit, g_file_replace_text;
static wchar_t g_file_title[64], g_file_directory[MAX_PATH], g_file_name[MAX_PATH], g_file_output[MAX_PATH];
static BSPNode g_bsp_nodes[MAX_POLYS];
static int g_bsp_coplanar_head[MAX_POLYS], g_bsp_coplanar_next[MAX_POLYS];
static int g_bsp_root = -1, g_bsp_count, g_bsp_valid, g_bsp_spanning, g_bsp_flat;
static int g_bsp_debug, g_bsp_diag_mode;
static uint16_t g_snes_palette[256];
static int g_palette_loaded, g_palette_number, g_palette_index = -1, g_texture_index = -1, g_coltab_index;
static int g_smooth_shade;
static int g_snes_mode;
static wchar_t g_texture_path[MAX_PATH];
static wchar_t g_snes_data_dir[MAX_PATH];
static SNESEntry g_coltab_entries[MAX_SNES_ENTRIES], g_palette_entries[MAX_SNES_ENTRIES], g_texture_entries[MAX_SNES_ENTRIES];
static const wchar_t *g_coltab_labels[MAX_SNES_ENTRIES], *g_palette_labels[MAX_SNES_ENTRIES], *g_texture_labels[MAX_SNES_ENTRIES];
static int g_coltab_count, g_palette_count, g_texture_count;
static POINT g_drag_start;
static double g_rotate_center[3], g_rotate_start_angle;
static uint8_t g_transform_selected[MAX_DOTS];
static uint16_t g_copy_source[MAX_DOTS];
static size_t g_transform_count, g_copy_start, g_copy_count;
static uint16_t g_select_order[MAX_DOTS];
static size_t g_select_count;
static uint16_t g_undo_select_order[MAX_DOTS];
static size_t g_undo_select_count;
static uint16_t g_poly_type = 7, g_poly_colour = 1, g_current_group = 1, g_shape_display_mask = 255;
static double g_plane_weight = 1.0;
static wchar_t g_path[MAX_PATH];
static wchar_t g_status[256] = L"Ready";
static void command(int id);
static size_t selected_dot_count(void);
static void ensure_first_frame(void);
static double *axis_ptr(Dot *d, int a);
static void build_bsp(void);
static int begin_export_bsp(Shape *saved, Shape *working);
static void end_export_bsp(Shape *saved, Shape *working);
static int polygon_plane(size_t pi, double plane[4]);
static const MainButton g_main_buttons[24] = {
    {L"Select", ID_DOT_MODE}, {L"Plane Wt", -101}, {L"Grid", -102}, {L"Groups", -103}, {L"Polygons", -104}, {L"Move", ID_EDIT_MOVE}, {L"Copy", ID_EDIT_COPY}, {L"Clear", ID_EDIT_SELECT_NONE}, {L"All Dots", ID_EDIT_SELECT_ALL}, {L"Del Dot", ID_EDIT_DELETE}, {L"Size", ID_EDIT_SIZE}, {L"Zoom", -105}, {L"SNES", -106}, {L"Show", -107}, {L"Load", ID_FILE_OPEN}, {L"Save", -108}, {L"Animate", -109}, {L"Mirror", ID_SHAPE_MIRROR_DIALOG}, {L"Rotate", ID_EDIT_ROTATE}, {L"Compact", ID_EDIT_COMPACT}, {L"Test", -110}, {L"Undo", ID_EDIT_UNDO}, {L"NEW", -1}, {L"QUIT", ID_FILE_EXIT}};
static const uint16_t g_main_button_flags[24] = {0, 0, 0, 0, 0x100, 0xc00, 0xc00, 0xc00, 0x100, 0x400, 0x100, 0, 0x2000, 0x200, 0, 0x100, 0, 0x100, 0xc00, 0x100, 0, 0, 0, 0};
static const wchar_t *g_plane_labels[] = {L"0.001", L"0.01", L"0.1", L"0.5", L"1", L"2", L"5", L"10", L"100"};
static const wchar_t *g_grid_labels[] = {L"Grid 1", L"Grid 2", L"Grid 5", L"Grid 10", L"Grid 15", L"Grid 20", L"Grid 30", L"Grid 50", L"Grid 100", L"Grid 150", L"Grid 200", L"Grid 300", L"Grid 500", L"Grid 1000", L"Grid 2000"};
static const wchar_t *g_group_labels[] = {L"Regroup", L"Displayed", L"Set"};
static const wchar_t *g_poly_labels[] = {L"Create", L"Select", L"Deselect", L"Type", L"Prev", L"Next", L"Select by", L"Flip", L"Rot vert", L"Delete", L"Dft Col", L"colour", L"Sort", L"Draw last", L"Sel vert"};
static const wchar_t *g_zoom_labels[] = {L"Zoom Up", L"Zoom Dwn", L"Auto Zoom"};
static const wchar_t *g_snes_labels[] = {L"Send SNES", L"Col Table", L"Palette", L"Pal num", L"Textures", L"BSP DEBUG"};
static const wchar_t *g_show_labels[] = {L"Preview", L"3D sys"};
static const wchar_t *g_save_labels[] = {L"Internal", L"3DCG", L"3DG1", L"ASM GZS", L"ASM BSP", L"ASM PC"};
static const wchar_t *g_anim_labels[] = {L"Frames", L"Key Frame", L"Shift An", L"Next", L"Prev", L"Add"};
static const wchar_t *g_test_labels[] = {L"BSP", L"Twist"};
static const wchar_t *g_type_labels[] = {L"Poly", L"Light S", L"Vis Tst", L"Z Clip", L"Plane", L"BSP Node"};
static const wchar_t *g_setgroup_labels[] = {L"1", L"2", L"3", L"4", L"5", L"6", L"7", L"8"};
static const wchar_t *g_showgroup_labels[] = {L"All", L"1", L"2", L"3", L"4", L"5", L"6", L"7", L"8"};
static const wchar_t *g_frames_labels[] = {L"Next", L"Previous", L"Add (frames)", L"Delete (frames)", L"Copy to frame", L"Show all"};
static const wchar_t *g_bsptest_labels[] = {L"Nodes", L"Xing polys", L"Need 2 cut"};
static const wchar_t *g_palnum_labels[] = {L"0", L"1", L"2", L"3", L"4", L"5", L"6", L"7", L"8", L"9", L"A", L"B", L"C", L"D", L"E", L"F"};
static const wchar_t *g_selectby_labels[] = {L"All polys", L"Dots (1)", L"Lines (2)", L"Triangles (3)", L"Quads (4)", L"5 verts", L"6 verts", L"7+ verts"};
static const uint16_t g_anim_flags[] = {0, 0x400, 0x1000, 0x1000, 0x1000, 0};
static const uint16_t g_bsptest_flags[] = {0x200, 0x200, 0x200};
static const uint16_t g_group_flags[] = {0x800, 0, 0};
static const uint16_t g_poly_flags[] = {0x400, 0x200, 0x800, 0x800, 0x200, 0x200, 0x200, 0x800, 0x800, 0x800, 0, 0x800, 0x200, 0x800, 0x800};
static const uint16_t g_snes_flags[] = {0x2200, 0x2000, 0x2000, 0x2000, 0x2000, 0x2010};
static const uint16_t g_save_flags[] = {0x100, 0x100, 0x100, 0x2100, 0x2100, 0x100};
static const uint16_t g_show_flags[] = {0x200, 0x200};
static const uint16_t g_test_flags[] = {0x200, 0x200};

static void free_frames(Shape *shape) {
    for (size_t i = 0; i < MAX_FRAMES; i++) {
        free(shape->frames[i]);
        shape->frames[i] = NULL;
    }
    shape->frame_count = 0;
}
static int copy_shape(Shape *dst, const Shape *src) {
    free_frames(dst);
    memcpy(dst->dots, src->dots, sizeof(dst->dots));
    memcpy(dst->polys, src->polys, sizeof(dst->polys));
    dst->dot_count = src->dot_count;
    dst->poly_count = src->poly_count;
    dst->frame_count = src->frame_count;
    for (size_t i = 0; i < src->frame_count; i++) {
        dst->frames[i] = (FrameDot *)malloc(src->dot_count * sizeof(FrameDot));
        if (!dst->frames[i]) {
            free_frames(dst);
            return 0;
        }
        memcpy(dst->frames[i], src->frames[i], src->dot_count * sizeof(FrameDot));
    }
    return 1;
}
static void snapshot(void) {
    if (copy_shape(&g_undo, &g_shape)) {
        g_has_undo = 1;
        g_undo_select_count = g_select_count;
        memcpy(g_undo_select_order, g_select_order, g_select_count * sizeof(g_select_order[0]));
    }
    g_bsp_valid = 0;
}
static int restore_undo_copy(void) {
    if (!copy_shape(&g_shape, &g_undo))
        return 0;
    g_select_count = g_undo_select_count;
    memcpy(g_select_order, g_undo_select_order, g_select_count * sizeof(g_select_order[0]));
    return 1;
}
static void statusf(const wchar_t *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    _vsnwprintf(g_status, 255, fmt, ap);
    va_end(ap);
    g_status[255] = 0;
    InvalidateRect(g_hwnd, NULL, FALSE);
}

static COLORREF ega(int n) {
    static const COLORREF c[16] = {
        RGB(0, 0, 0), RGB(0, 0, 170), RGB(0, 170, 0), RGB(0, 170, 170), RGB(170, 0, 0), RGB(170, 0, 170), RGB(128, 128, 0), RGB(170, 170, 170),
        RGB(85, 85, 85), RGB(85, 85, 255), RGB(85, 255, 85), RGB(85, 255, 255), RGB(255, 85, 85), RGB(255, 85, 255), RGB(255, 255, 85), RGB(255, 255, 255)};
    return c[n & 15];
}
static COLORREF snes_renderer_colour(int n) {
    if (g_palette_loaded) {
        uint16_t p = g_snes_palette[n & 255];
        return RGB((p & 31) * 255 / 31, ((p >> 5) & 31) * 255 / 31, ((p >> 10) & 31) * 255 / 31);
    }
    return ega(n);
}
static uint8_t snes_palette_number_command(int index) { return (uint8_t)(0xd0 + (index & 15)); }

static double dos_coord(double value) {
    if (!isfinite(value))
        return 0.0;
    int64_t whole = (int64_t)trunc(value);
    uint16_t bits = (uint16_t)((uint64_t)whole & 0xffffu);
    return (double)(int16_t)bits;
}

static double coord(const Dot *d, int axis) { return axis == 0 ? d->x : axis == 1 ? d->y
                                                                                  : d->z; }
static Dot display_dot_at_frame(size_t index, size_t frame) {
    Dot dot = g_shape.dots[index];
    if (g_shape.frame_count && frame < g_shape.frame_count && g_shape.frames[frame]) {
        FrameDot *p = &g_shape.frames[frame][index];
        dot.x = p->x;
        dot.y = p->y;
        dot.z = p->z;
    }
    return dot;
}
static Dot display_dot(size_t index) { return display_dot_at_frame(index, g_current_frame); }
static int dot_active_at_frame(size_t index, size_t frame) { return !g_shape.frame_count || frame >= g_shape.frame_count || !g_shape.frames[frame] || g_shape.frames[frame][index].active != 0; }
static int dot_selected_at_frame(size_t index, size_t frame) { return g_shape.frame_count && frame < g_shape.frame_count && g_shape.frames[frame] ? (g_shape.frames[frame][index].active & 0x0100) != 0 : g_shape.dots[index].selected != 0; }
static int dot_active(size_t index) { return dot_active_at_frame(index, g_current_frame); }
static int shape_is_compacted(void) {
    for (size_t i = 0; i < g_shape.dot_count; i++)
        if (!dot_active(i))
            return 0;
    for (size_t i = 0; i < g_shape.poly_count; i++)
        if (!g_shape.polys[i].flags)
            return 0;
    return 1;
}
static void remap_selection_to_current_frame(void) {
    if (!g_shape.frame_count || g_current_frame >= g_shape.frame_count || !g_shape.frames[g_current_frame])
        return;
    for (size_t i = 0; i < g_shape.dot_count; i++) {
        g_shape.dots[i].selected = 0;
        g_shape.frames[g_current_frame][i].active &= (uint16_t)~0x0100;
    }
    for (size_t q = 0; q < g_select_count; q++) {
        size_t i = g_select_order[q];
        if (i < g_shape.dot_count) {
            g_shape.dots[i].selected = 1;
            g_shape.frames[g_current_frame][i].active |= 0x0100;
        }
    }
}
static void spread_current_frame_flags(void) {
    if (!g_shape.frame_count || g_current_frame >= g_shape.frame_count || !g_shape.frames[g_current_frame])
        return;
    for (size_t f = 0; f < g_shape.frame_count; f++)
        if (f != g_current_frame && g_shape.frames[f])
            for (size_t i = 0; i < g_shape.dot_count; i++)
                g_shape.frames[f][i].active = g_shape.frames[g_current_frame][i].active;
}
static void tmp_mark_selected(void) {
    if (!g_shape.frame_count || g_current_frame >= g_shape.frame_count || !g_shape.frames[g_current_frame])
        return;
    FrameDot *current = g_shape.frames[g_current_frame];
    for (size_t i = 0; i < g_shape.dot_count; i++)
        if (current[i].active) {
            if (current[i].active & 0x0100)
                current[i].active |= 0x0200;
            else
                current[i].active &= (uint16_t)~0x0200;
        }
    for (size_t p = 0; p < g_shape.poly_count; p++)
        if (g_shape.polys[p].flags) {
            if (g_shape.polys[p].selected) {
                g_shape.polys[p].flags |= 0x0200;
                for (unsigned j = 0; j < g_shape.polys[p].count; j++) {
                    size_t i = g_shape.polys[p].index[j];
                    if (i < g_shape.dot_count)
                        current[i].active |= 0x0200;
                }
            } else
                g_shape.polys[p].flags &= (uint16_t)~0x0200;
        }
}
static void tmp_mark_all(void) {
    if (!g_shape.frame_count || g_current_frame >= g_shape.frame_count || !g_shape.frames[g_current_frame])
        return;
    for (size_t i = 0; i < g_shape.dot_count; i++)
        if (g_shape.frames[g_current_frame][i].active)
            g_shape.frames[g_current_frame][i].active |= 0x0200;
    for (size_t p = 0; p < g_shape.poly_count; p++)
        if (g_shape.polys[p].flags)
            g_shape.polys[p].flags |= 0x0200;
}
static void clear_dot_selection(int clear_polygons) {
    for (size_t i = 0; i < g_shape.dot_count; i++) {
        g_shape.dots[i].selected = 0;
        if (g_shape.frame_count && g_current_frame < g_shape.frame_count && g_shape.frames[g_current_frame])
            g_shape.frames[g_current_frame][i].active &= (uint16_t)~0x0100;
    }
    g_select_count = 0;
    if (clear_polygons)
        for (size_t i = 0; i < g_shape.poly_count; i++)
            g_shape.polys[i].selected = 0;
}
static void set_dot_selected(size_t index, int selected) {
    if (index >= g_shape.dot_count)
        return;
    if (selected) {
        if (g_shape.dots[index].selected)
            return;
        g_shape.dots[index].selected = 1;
        if (g_shape.frame_count && g_current_frame < g_shape.frame_count && g_shape.frames[g_current_frame])
            g_shape.frames[g_current_frame][index].active |= 0x0100;
        if (g_select_count < MAX_DOTS)
            g_select_order[g_select_count++] = (uint16_t)index;
        return;
    }
    if (!g_shape.dots[index].selected)
        return;
    g_shape.dots[index].selected = 0;
    if (g_shape.frame_count && g_current_frame < g_shape.frame_count && g_shape.frames[g_current_frame])
        g_shape.frames[g_current_frame][index].active &= (uint16_t)~0x0100;
    for (size_t i = 0; i < g_select_count; i++)
        if (g_select_order[i] == index) {
            memmove(&g_select_order[i], &g_select_order[i + 1], (g_select_count - i - 1) * sizeof(g_select_order[0]));
            g_select_count--;
            break;
        }
}
static void rebuild_dot_selection_order(void) {
    g_select_count = 0;
    for (size_t i = 0; i < g_shape.dot_count && g_select_count < MAX_DOTS; i++)
        if (g_shape.dots[i].selected)
            g_select_order[g_select_count++] = (uint16_t)i;
}
static POINT project(const View *v, const Dot *d) {
    POINT p;
    double scale = g_zoom;
    p.x = (v->r.left + v->r.right) / 2 + v->signs[0] * (int)((coord(d, v->axes[0]) - g_origin[v->axes[0]]) * scale);
    p.y = (v->r.top + v->r.bottom) / 2 - v->signs[1] * (int)((coord(d, v->axes[1]) - g_origin[v->axes[1]]) * scale);
    return p;
}

static double view_world_coordinate(const View *v, int lane, double pixel) {
    double center = lane ? (v->r.top + v->r.bottom) / 2.0 : (v->r.left + v->r.right) / 2.0;
    double offset = (pixel - center) / g_zoom;
    return g_origin[v->axes[lane]] + (lane ? -v->signs[lane] : v->signs[lane]) * offset;
}

static void make_views(RECT client, View v[4]) {
    int right = client.right * 560 / 640, mx = client.right * 280 / 640, my = client.bottom * 240 / 480;
    v[0] = (View){{0, 0, mx, my}, {2, 1}, {1, 1}, L""};
    v[1] = (View){{mx, 0, right, my}, {0, 1}, {1, 1}, L""};
    v[2] = (View){{0, my, mx, client.bottom}, {2, 0}, {1, -1}, L""};
    v[3] = (View){{mx, my, right, client.bottom}, {0, 1}, {1, 1}, L""};
}

static RECT button_rect(RECT client, int index) {
    int left = client.right * 560 / 640;
    return (RECT){left, client.bottom * index / 24, client.right, client.bottom * (index + 1) / 24};
}
static const wchar_t **menu_labels(int menu_id, int *count, const wchar_t **title) {
    switch (menu_id) {
    case MENU_PLANE:
        *count = 9;
        *title = L"Plane weight";
        return g_plane_labels;
    case MENU_GRID:
        *count = 15;
        *title = L"Grid";
        return g_grid_labels;
    case MENU_GROUP:
        *count = 3;
        *title = L"Groups";
        return g_group_labels;
    case MENU_POLY:
        *count = 15;
        *title = L"Polygons";
        return g_poly_labels;
    case MENU_ZOOM:
        *count = 3;
        *title = L"Zoom";
        return g_zoom_labels;
    case MENU_SNES:
        *count = 6;
        *title = L"SNES";
        return g_snes_labels;
    case MENU_SHOW:
        *count = 2;
        *title = L"Show";
        return g_show_labels;
    case MENU_SAVE:
        *count = 6;
        *title = L"Save";
        return g_save_labels;
    case MENU_ANIM:
        *count = 6;
        *title = L"Animate";
        return g_anim_labels;
    case MENU_TEST:
        *count = 2;
        *title = L"Test";
        return g_test_labels;
    case MENU_TYPE:
        *count = 6;
        *title = L"Type";
        return g_type_labels;
    case MENU_SETGROUP:
        *count = 8;
        *title = L"Set group";
        return g_setgroup_labels;
    case MENU_SHOWGROUP:
        *count = 9;
        *title = L"Displayed";
        return g_showgroup_labels;
    case MENU_FRAMES:
        *count = 6;
        *title = L"Animation Frames";
        return g_frames_labels;
    case MENU_BSPTEST:
        *count = 3;
        *title = L"BSP Test";
        return g_bsptest_labels;
    case MENU_PALNUM:
        *count = 16;
        *title = L"Palette number";
        return g_palnum_labels;
    case MENU_SELECTBY:
        *count = 8;
        *title = L"Select by vertices";
        return g_selectby_labels;
    case MENU_COLTAB:
        *count = g_coltab_count;
        *title = L"Colour Table";
        return g_coltab_labels;
    case MENU_PALETTE:
        *count = g_palette_count;
        *title = L"Palette";
        return g_palette_labels;
    case MENU_TEXTURE:
        *count = g_texture_count;
        *title = L"Textures";
        return g_texture_labels;
    default:
        *count = 0;
        *title = L"SHAPED";
        return NULL;
    }
}
static RECT submenu_rect(RECT client, int index, int count) {
    (void)count;
    int left = client.right * 560 / 640;
    return (RECT){left, client.bottom * index / 24, client.right, client.bottom * (index + 1) / 24};
}

static void draw_fastgraph_text(HDC dc, int x, int y, const wchar_t *text, COLORREF colour) {
    for (; *text; text++, x += 8) {
        unsigned ch = (unsigned)*text;
        if (ch < 32 || ch > 126)
            ch = '?';
        const unsigned char *glyph = g_fastgraph_ascii[ch - 32];
        for (int row = 0; row < 14; row++) {
            unsigned char bits = glyph[row];
            for (int bit = 0; bit < 8; bit++)
                if (bits & (0x80u >> bit))
                    SetPixelV(dc, x + bit, y + row, colour);
        }
    }
}

static uint16_t menu_enable_mask(void) {
    uint16_t mask = g_snes_mode ? 0x2000 : 0;
    for (size_t i = 0; i < g_shape.dot_count; i++)
        if (dot_active(i)) {
            mask |= 0x100;
            if (g_shape.dots[i].selected)
                mask |= 0x400;
        }
    for (size_t i = 0; i < g_shape.poly_count; i++)
        if (g_shape.polys[i].flags) {
            mask |= 0x200;
            if (g_shape.polys[i].selected)
                mask |= 0x800;
        }
    if (g_shape.frame_count > 1)
        mask |= 0x1000;
    return mask;
}
static int flags_enabled(uint16_t flags) {
    uint16_t conditions = flags & 0x3f00;
    return !conditions || (menu_enable_mask() & conditions) != 0;
}
static uint16_t submenu_flags(int menu, int index) {
    switch (menu) {
    case MENU_ANIM:
        return g_anim_flags[index];
    case MENU_BSPTEST:
        return g_bsptest_flags[index];
    case MENU_GROUP:
        return g_group_flags[index];
    case MENU_POLY:
        return g_poly_flags[index];
    case MENU_SNES:
        return g_snes_flags[index];
    case MENU_SAVE:
        return g_save_flags[index];
    case MENU_SHOW:
        return g_show_flags[index];
    case MENU_TEST:
        return g_test_flags[index];
    default:
        return 0;
    }
}
static int submenu_selected(int menu, int index) {
    static const double planes[] = {.001, .01, .1, .5, 1, 2, 5, 10, 100};
    static const double grids[] = {1, 2, 5, 10, 15, 20, 30, 50, 100, 150, 200, 300, 500, 1000, 2000};
    if (menu == MENU_PLANE)
        return fabs(g_plane_weight - planes[index]) < 1e-12;
    if (menu == MENU_GRID)
        return fabs(g_grid - grids[index]) < 1e-12;
    if (menu == MENU_TYPE)
        return (g_poly_type & (1u << index)) != 0;
    if (menu == MENU_SETGROUP)
        return g_current_group == (uint16_t)(1u << index);
    if (menu == MENU_SHOWGROUP)
        return index ? g_shape_display_mask == (uint16_t)(1u << (index - 1)) : g_shape_display_mask == 255;
    if (menu == MENU_PALNUM)
        return g_palette_number == index;
    if (menu == MENU_COLTAB)
        return g_coltab_index == index;
    if (menu == MENU_PALETTE)
        return g_palette_index == index;
    if (menu == MENU_TEXTURE)
        return g_texture_index == index;
    return 0;
}
static int main_button_enabled(int index) { return flags_enabled(g_main_button_flags[index]); }

static void draw_panels(HDC dc, RECT client) {
    HPEN edge = CreatePen(PS_SOLID, 1, RGB(0, 255, 255));
    HBRUSH blue = CreateSolidBrush(RGB(0, 0, 255)), green = CreateSolidBrush(RGB(0, 255, 0));
    HGDIOBJ oldpen = SelectObject(dc, edge), oldbrush = SelectObject(dc, blue);
    if (g_active_menu) {
        int count;
        const wchar_t *title;
        const wchar_t **labels = menu_labels(g_active_menu, &count, &title);
        (void)title;
        for (int i = 0; i < count; i++) {
            RECT r = submenu_rect(client, i, count);
            SelectObject(dc, submenu_selected(g_active_menu, i) ? green : blue);
            Rectangle(dc, r.left, r.top, r.right, r.bottom);
            draw_fastgraph_text(dc, r.left, r.top + 2, labels[i], flags_enabled(submenu_flags(g_active_menu, i)) ? RGB(0, 255, 255) : RGB(255, 0, 0));
        }
    } else
        for (int i = 0; i < 24; i++) {
            RECT r = button_rect(client, i);
            const wchar_t *label = i == 0 && g_add_dot_mode ? L"Add Dot" : g_main_buttons[i].label;
            Rectangle(dc, r.left, r.top, r.right, r.bottom);
            draw_fastgraph_text(dc, r.left, r.top + 2, label, main_button_enabled(i) ? RGB(0, 255, 255) : RGB(255, 0, 0));
        }
    SelectObject(dc, oldbrush);
    SelectObject(dc, oldpen);
    DeleteObject(green);
    DeleteObject(blue);
    DeleteObject(edge);
}

static void update_cursor_coords(int x, int y) {
    RECT client;
    View v[4];
    POINT mouse = {x, y};
    double old[3] = {g_cursor_coords[0], g_cursor_coords[1], g_cursor_coords[2]};
    GetClientRect(g_hwnd, &client);
    make_views(client, v);
    for (int i = 0; i < 3; i++)
        if (PtInRect(&v[i].r, mouse)) {
            int own_projection = i == 0 ? 1 : i == 1 ? 3
                                                     : 2;
            g_cursor_coords[v[i].axes[0]] = view_world_coordinate(&v[i], 0, x);
            g_cursor_coords[v[i].axes[1]] = view_world_coordinate(&v[i], 1, y);
            if (g_cursor_projection != own_projection) {
                double dx = fabs(g_cursor_coords[0] - old[0]), dy = fabs(g_cursor_coords[1] - old[1]), dz = fabs(g_cursor_coords[2] - old[2]);
                if (i == 0)
                    g_cursor_projection = dy < dz ? 2 : 3; /* ZY: follow Z in ZX, Y in XY */
                else if (i == 1)
                    g_cursor_projection = dy < dx ? 2 : 1; /* XY: follow X in ZX, Y in ZY */
                else
                    g_cursor_projection = dx < dz ? 1 : 3; /* ZX: follow Z in ZY, X in XY */
            }
            break;
        }
}

static void move_origin_key(WPARAM key) {
    double delta = 5.0 / g_zoom;
    if (key == VK_HOME) {
        g_origin[0] = g_origin[1] = g_origin[2] = 0;
        statusf(L"Origin home");
        return;
    }
    if (g_cursor_projection < 1 || g_cursor_projection > 3)
        return;
    if (key == VK_LEFT) {
        if (g_cursor_projection == 3)
            g_origin[0] += delta;
        else
            g_origin[2] -= delta;
    } else if (key == VK_RIGHT) {
        if (g_cursor_projection == 3)
            g_origin[0] -= delta;
        else
            g_origin[2] += delta;
    } else if (key == VK_UP) {
        if (g_cursor_projection == 2)
            g_origin[0] += delta;
        else
            g_origin[1] += delta;
    } else if (key == VK_DOWN) {
        if (g_cursor_projection == 2)
            g_origin[0] -= delta;
        else
            g_origin[1] -= delta;
    }
    statusf(L"Origin %.3g %.3g %.3g", g_origin[0], g_origin[1], g_origin[2]);
}
static void draw_cursor_coords(HDC dc, RECT client) {
    if (!g_cursor_overlay)
        return;
    RECT r = {client.right * 284 / 640, client.bottom * 399 / 480, client.right * 541 / 640, client.bottom * 416 / 480};
    HBRUSH yellow = CreateSolidBrush(RGB(255, 255, 0));
    FillRect(dc, &r, yellow);
    DeleteObject(yellow);
    wchar_t text[64];
    _snwprintf(text, 63, L" %6.0f %6.0f %6.0f", g_cursor_coords[0], g_cursor_coords[1], g_cursor_coords[2]);
    text[63] = 0;
    draw_fastgraph_text(dc, client.right * 320 / 640, client.bottom * 400 / 480, text, RGB(0, 255, 255));
}

static void draw_dos_prompt(HDC dc, RECT client) {
    if (!g_dos_prompt)
        return;
    if (g_dos_prompt == PROMPT_NUMBER) {
        int x0 = client.right * 50 / 640, x1 = client.right * 210 / 640, x2 = client.right * 370 / 640, y0 = client.bottom * 50 / 480, y2 = client.bottom * 122 / 480;
        HBRUSH blue = CreateSolidBrush(RGB(0, 0, 255));
        FillRect(dc, &(RECT){x0, y0, x2 + 1, y2}, blue);
        DeleteObject(blue);
        HPEN cyan = CreatePen(PS_SOLID, 1, RGB(0, 255, 255));
        HGDIOBJ op = SelectObject(dc, cyan);
        MoveToEx(dc, x0, y0, NULL);
        LineTo(dc, x2 + 1, y0);
        MoveToEx(dc, x0, y2 - 1, NULL);
        LineTo(dc, x2 + 1, y2 - 1);
        MoveToEx(dc, x0, y0, NULL);
        LineTo(dc, x0, y2);
        MoveToEx(dc, x2, y0, NULL);
        LineTo(dc, x2, y2);
        MoveToEx(dc, x0, client.bottom * 72 / 480, NULL);
        LineTo(dc, x2 + 1, client.bottom * 72 / 480);
        MoveToEx(dc, x0, client.bottom * 96 / 480, NULL);
        LineTo(dc, x2 + 1, client.bottom * 96 / 480);
        MoveToEx(dc, x1, client.bottom * 96 / 480, NULL);
        LineTo(dc, x1, y2);
        SelectObject(dc, op);
        DeleteObject(cyan);
        draw_fastgraph_text(dc, client.right * 59 / 640, client.bottom * 76 / 480, g_number_label, RGB(0, 255, 255));
        draw_fastgraph_text(dc, client.right * 219 / 640, client.bottom * 76 / 480, g_number_text, RGB(0, 255, 255));
        if (g_number_active_edit)
            draw_fastgraph_text(dc, client.right * (219 + (int)wcslen(g_number_text) * 8) / 640, client.bottom * 76 / 480, L"_", RGB(0, 255, 255));
        draw_fastgraph_text(dc, client.right * 159 / 640, client.bottom * 52 / 480, g_number_title, RGB(0, 255, 255));
        draw_fastgraph_text(dc, client.right * 69 / 640, client.bottom * 100 / 480, L"OK", RGB(0, 255, 255));
        draw_fastgraph_text(dc, client.right * 229 / 640, client.bottom * 100 / 480, L"CANCEL", RGB(0, 255, 255));
        return;
    }
    if (g_dos_prompt == PROMPT_SELECT_POLYS) {
        int x0 = client.right * 50 / 640, x1 = client.right * 210 / 640, x2 = client.right * 370 / 640, y0 = client.bottom * 50 / 480, y2 = client.bottom * 362 / 480;
        HBRUSH blue = CreateSolidBrush(RGB(0, 0, 255));
        FillRect(dc, &(RECT){x0, y0, x2 + 1, y2}, blue);
        DeleteObject(blue);
        HPEN cyan = CreatePen(PS_SOLID, 1, RGB(0, 255, 255));
        HGDIOBJ op = SelectObject(dc, cyan);
        MoveToEx(dc, x0, y0, NULL);
        LineTo(dc, x2 + 1, y0);
        MoveToEx(dc, x0, y2 - 1, NULL);
        LineTo(dc, x2 + 1, y2 - 1);
        MoveToEx(dc, x0, y0, NULL);
        LineTo(dc, x0, y2);
        MoveToEx(dc, x2, y0, NULL);
        LineTo(dc, x2, y2);
        for (int i = 0; i <= 11; i++) {
            int y = client.bottom * (72 + i * 24) / 480;
            MoveToEx(dc, x0, y, NULL);
            LineTo(dc, x2 + 1, y);
        }
        MoveToEx(dc, x1, client.bottom * 336 / 480, NULL);
        LineTo(dc, x1, y2);
        SelectObject(dc, op);
        DeleteObject(cyan);
        for (int i = 0; i < 11; i++) {
            int y = client.bottom * (76 + i * 24) / 480;
            draw_fastgraph_text(dc, client.right * 59 / 640, y, g_selectpolys_labels[i], RGB(0, 255, 255));
            draw_fastgraph_text(dc, client.right * 219 / 640, y, g_selectpolys_action == i ? L"YES" : L"NO", RGB(0, 255, 255));
        }
        draw_fastgraph_text(dc, client.right * 159 / 640, client.bottom * 52 / 480, L"Select Polygons", RGB(0, 255, 255));
        draw_fastgraph_text(dc, client.right * 69 / 640, client.bottom * 340 / 480, L"OK", RGB(0, 255, 255));
        draw_fastgraph_text(dc, client.right * 229 / 640, client.bottom * 340 / 480, L"CANCEL", RGB(0, 255, 255));
        return;
    }
    if (g_dos_prompt == PROMPT_FRAMES) {
        int x0 = client.right * 50 / 640, x1 = client.right * 210 / 640, x2 = client.right * 370 / 640, y0 = client.bottom * 50 / 480, y2 = client.bottom * 242 / 480;
        HBRUSH blue = CreateSolidBrush(RGB(0, 0, 255));
        FillRect(dc, &(RECT){x0, y0, x2 + 1, y2}, blue);
        DeleteObject(blue);
        HPEN cyan = CreatePen(PS_SOLID, 1, RGB(0, 255, 255));
        HGDIOBJ op = SelectObject(dc, cyan);
        MoveToEx(dc, x0, y0, NULL);
        LineTo(dc, x2 + 1, y0);
        MoveToEx(dc, x0, y2 - 1, NULL);
        LineTo(dc, x2 + 1, y2 - 1);
        MoveToEx(dc, x0, y0, NULL);
        LineTo(dc, x0, y2);
        MoveToEx(dc, x2, y0, NULL);
        LineTo(dc, x2, y2);
        for (int i = 0; i <= 6; i++) {
            int y = client.bottom * (72 + i * 24) / 480;
            MoveToEx(dc, x0, y, NULL);
            LineTo(dc, x2 + 1, y);
        }
        MoveToEx(dc, x1, client.bottom * 216 / 480, NULL);
        LineTo(dc, x1, y2);
        SelectObject(dc, op);
        DeleteObject(cyan);
        static const wchar_t *labels[] = {L"Next", L"Previous", L"Add (frames)", L"Delete (frames)", L"Copy to frame", L"Show all"};
        for (int i = 0; i < 6; i++) {
            int y = client.bottom * (76 + i * 24) / 480;
            draw_fastgraph_text(dc, client.right * 59 / 640, y, labels[i], RGB(0, 255, 255));
            const wchar_t *value;
            if (i == 0 || i == 1 || i == 5)
                value = g_frames_action == i ? L"YES" : L"NO";
            else
                value = g_frames_text[i - 2];
            draw_fastgraph_text(dc, client.right * 219 / 640, y, value, RGB(0, 255, 255));
            if (i == g_frames_active_edit)
                draw_fastgraph_text(dc, client.right * (219 + (int)wcslen(value) * 8) / 640, y, L"_", RGB(0, 255, 255));
        }
        draw_fastgraph_text(dc, client.right * 159 / 640, client.bottom * 52 / 480, L"Animation Frames", RGB(0, 255, 255));
        draw_fastgraph_text(dc, client.right * 69 / 640, client.bottom * 220 / 480, L"OK", RGB(0, 255, 255));
        draw_fastgraph_text(dc, client.right * 229 / 640, client.bottom * 220 / 480, L"CANCEL", RGB(0, 255, 255));
        return;
    }
    if (g_dos_prompt == PROMPT_SCALE) {
        int x0 = client.right * 50 / 640, x1 = client.right * 210 / 640, x2 = client.right * 370 / 640, y0 = client.bottom * 50 / 480, y2 = client.bottom * 242 / 480;
        HBRUSH blue = CreateSolidBrush(RGB(0, 0, 255));
        FillRect(dc, &(RECT){x0, y0, x2 + 1, y2}, blue);
        DeleteObject(blue);
        HPEN cyan = CreatePen(PS_SOLID, 1, RGB(0, 255, 255));
        HGDIOBJ op = SelectObject(dc, cyan);
        MoveToEx(dc, x0, y0, NULL);
        LineTo(dc, x2 + 1, y0);
        MoveToEx(dc, x0, y2 - 1, NULL);
        LineTo(dc, x2 + 1, y2 - 1);
        MoveToEx(dc, x0, y0, NULL);
        LineTo(dc, x0, y2);
        MoveToEx(dc, x2, y0, NULL);
        LineTo(dc, x2, y2);
        for (int i = 0; i <= 6; i++) {
            int y = client.bottom * (72 + i * 24) / 480;
            MoveToEx(dc, x0, y, NULL);
            LineTo(dc, x2 + 1, y);
        }
        MoveToEx(dc, x1, client.bottom * 216 / 480, NULL);
        LineTo(dc, x1, y2);
        SelectObject(dc, op);
        DeleteObject(cyan);
        static const wchar_t *labels[] = {L"Scale All", L"Scale X", L"Scale Y", L"Scale Z", L"Selected Dots Only", L"All frames"};
        for (int i = 0; i < 6; i++) {
            int y = client.bottom * (76 + i * 24) / 480;
            draw_fastgraph_text(dc, client.right * 59 / 640, y, labels[i], RGB(0, 255, 255));
            const wchar_t *value = i < 4 ? g_scale_text[i] : (g_scale_options[i - 4] ? L"YES" : L"NO");
            draw_fastgraph_text(dc, client.right * 219 / 640, y, value, RGB(0, 255, 255));
            if (i == g_scale_active_edit)
                draw_fastgraph_text(dc, client.right * (219 + (int)wcslen(value) * 8) / 640, y, L"_", RGB(0, 255, 255));
        }
        draw_fastgraph_text(dc, client.right * 159 / 640, client.bottom * 52 / 480, L"Size Shape", RGB(0, 255, 255));
        draw_fastgraph_text(dc, client.right * 69 / 640, client.bottom * 220 / 480, L"OK", RGB(0, 255, 255));
        draw_fastgraph_text(dc, client.right * 229 / 640, client.bottom * 220 / 480, L"CANCEL", RGB(0, 255, 255));
        return;
    }
    if (g_dos_prompt == PROMPT_MIRROR) {
        int x0 = client.right * 50 / 640, x1 = client.right * 210 / 640, x2 = client.right * 370 / 640, y0 = client.bottom * 50 / 480, y2 = client.bottom * 218 / 480;
        HBRUSH blue = CreateSolidBrush(RGB(0, 0, 255));
        FillRect(dc, &(RECT){x0, y0, x2 + 1, y2}, blue);
        DeleteObject(blue);
        HPEN cyan = CreatePen(PS_SOLID, 1, RGB(0, 255, 255));
        HGDIOBJ op = SelectObject(dc, cyan);
        MoveToEx(dc, x0, y0, NULL);
        LineTo(dc, x2 + 1, y0);
        MoveToEx(dc, x0, y2 - 1, NULL);
        LineTo(dc, x2 + 1, y2 - 1);
        MoveToEx(dc, x0, y0, NULL);
        LineTo(dc, x0, y2);
        MoveToEx(dc, x2, y0, NULL);
        LineTo(dc, x2, y2);
        for (int i = 0; i <= 5; i++) {
            int y = client.bottom * (72 + i * 24) / 480;
            MoveToEx(dc, x0, y, NULL);
            LineTo(dc, x2 + 1, y);
        }
        MoveToEx(dc, x1, client.bottom * 192 / 480, NULL);
        LineTo(dc, x1, y2);
        SelectObject(dc, op);
        DeleteObject(cyan);
        static const wchar_t *labels[] = {L"X reflect", L"Y reflect", L"Z reflect", L"Selected only", L"Add image"};
        for (int i = 0; i < 5; i++) {
            int y = client.bottom * (76 + i * 24) / 480;
            draw_fastgraph_text(dc, client.right * 59 / 640, y, labels[i], RGB(0, 255, 255));
            draw_fastgraph_text(dc, client.right * 219 / 640, y, g_mirror_options[i] ? L"YES" : L"NO", RGB(0, 255, 255));
        }
        draw_fastgraph_text(dc, client.right * 159 / 640, client.bottom * 52 / 480, L"Mirror Shape", RGB(0, 255, 255));
        draw_fastgraph_text(dc, client.right * 69 / 640, client.bottom * 196 / 480, L"OK", RGB(0, 255, 255));
        draw_fastgraph_text(dc, client.right * 229 / 640, client.bottom * 196 / 480, L"CANCEL", RGB(0, 255, 255));
        return;
    }
    int x0 = client.right * 50 / 640, x1 = client.right * 210 / 640, x2 = client.right * 370 / 640, y0 = client.bottom * 50 / 480, y1 = client.bottom * 72 / 480, y2 = client.bottom * 98 / 480;
    HBRUSH blue = CreateSolidBrush(RGB(0, 0, 255));
    FillRect(dc, &(RECT){x0, y0, x2 + 1, y2}, blue);
    DeleteObject(blue);
    HPEN cyan = CreatePen(PS_SOLID, 1, RGB(0, 255, 255));
    HGDIOBJ op = SelectObject(dc, cyan);
    MoveToEx(dc, x0, y0, NULL);
    LineTo(dc, x0, y2);
    MoveToEx(dc, x2, y0, NULL);
    LineTo(dc, x2, y2);
    MoveToEx(dc, x0, y1, NULL);
    LineTo(dc, x2 + 1, y1);
    MoveToEx(dc, x0, y2 - 1, NULL);
    LineTo(dc, x2 + 1, y2 - 1);
    MoveToEx(dc, x1, y1, NULL);
    LineTo(dc, x1, y2);
    SelectObject(dc, op);
    DeleteObject(cyan);
    const wchar_t *title = g_dos_prompt == PROMPT_QUIT ? L"QUIT to OS" : g_dos_prompt == PROMPT_NEW ? L"Delete all"
                                                                                                    : L"Add image";
    const wchar_t *left = g_dos_prompt == PROMPT_ROTATE_ADD ? L"YES" : L"OK";
    const wchar_t *right = g_dos_prompt == PROMPT_ROTATE_ADD ? L"NO" : L"CANCEL";
    int title_x = (x0 + x2 - (int)wcslen(title) * 8) / 2 - 7;
    draw_fastgraph_text(dc, title_x, y0 + 2, title, RGB(0, 255, 255));
    draw_fastgraph_text(dc, x0 + 19, y1 + 4, left, RGB(0, 255, 255));
    draw_fastgraph_text(dc, x1 + 19, y1 + 4, right, RGB(0, 255, 255));
}

static void draw_grid(HDC dc, const View *v) {
    HBRUSH black = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(dc, &v->r, black);
    DeleteObject(black);
    double step = g_grid, px = step * g_zoom;
    while (px < 4.0) {
        step *= 10.0;
        px = step * g_zoom;
    }
    int cx = (v->r.left + v->r.right) / 2, cy = (v->r.top + v->r.bottom) / 2;
    double zero_x = cx - v->signs[0] * g_origin[v->axes[0]] * g_zoom;
    double zero_y = cy + v->signs[1] * g_origin[v->axes[1]] * g_zoom;
    HPEN minor = CreatePen(PS_SOLID, 1, RGB(0, 130, 0)), major = CreatePen(PS_SOLID, 1, RGB(0, 130, 130)), axis = CreatePen(PS_SOLID, 1, RGB(255, 0, 0));
    HGDIOBJ oldpen = SelectObject(dc, minor);
    if (px >= 4.0) {
        double x = v->r.left + fmod(fmod(zero_x - v->r.left, px) + px, px), y = v->r.top + fmod(fmod(zero_y - v->r.top, px) + px, px);
        for (; x < v->r.right; x += px) {
            MoveToEx(dc, (int)x, v->r.top, NULL);
            LineTo(dc, (int)x, v->r.bottom);
        }
        for (; y < v->r.bottom; y += px) {
            MoveToEx(dc, v->r.left, (int)y, NULL);
            LineTo(dc, v->r.right, (int)y);
        }
    }
    SelectObject(dc, major);
    SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(dc, v->r.left, v->r.top, v->r.right, v->r.bottom);
    SelectObject(dc, axis);
    MoveToEx(dc, (int)zero_x, v->r.top, NULL);
    LineTo(dc, (int)zero_x, v->r.bottom);
    MoveToEx(dc, v->r.left, (int)zero_y, NULL);
    LineTo(dc, v->r.right, (int)zero_y);
    SelectObject(dc, oldpen);
    DeleteObject(minor);
    DeleteObject(major);
    DeleteObject(axis);
}

static void draw_shape(HDC dc, const View *v) {
    for (int selected = 0; selected < 2; selected++)
        for (size_t i = 0; i < g_shape.poly_count; i++) {
            Poly *p = &g_shape.polys[i];
            if (!p->count || (p->flags & g_shape_display_mask) == 0 || p->selected != selected)
                continue;
            HPEN pen = CreatePen(PS_SOLID, 1, ega(p->selected ? 7 : 6));
            HGDIOBJ oldpen = SelectObject(dc, pen);
            if (p->count == 1) {
                Dot d = display_dot(p->index[0]);
                POINT point = project(v, &d);
                SetPixelV(dc, point.x, point.y, ega(selected ? 7 : 6));
            } else {
                for (unsigned j = 0; j < p->count; j++) {
                    Dot da = display_dot(p->index[j]), db = display_dot(p->index[(j + 1) % p->count]);
                    POINT a = project(v, &da), b = project(v, &db);
                    MoveToEx(dc, a.x, a.y, NULL);
                    LineTo(dc, b.x, b.y);
                }
            }
            SelectObject(dc, oldpen);
            DeleteObject(pen);
        }
    for (int selected = 0; selected < 2; selected++)
        for (size_t i = 0; i < g_shape.dot_count; i++)
            if (dot_active(i) && g_shape.dots[i].selected == selected) {
                Dot d = display_dot(i);
                POINT p = project(v, &d);
                COLORREF c = ega(selected ? 5 : 4);
                HBRUSH b = CreateSolidBrush(c);
                RECT r = {p.x - 1, p.y - 1, p.x + 1, p.y + 1};
                FillRect(dc, &r, b);
                DeleteObject(b);
            }
}

static void draw_frame_dots(HDC dc, const View *v, size_t frame) {
    for (int selected = 0; selected < 2; selected++)
        for (size_t i = 0; i < g_shape.dot_count; i++)
            if (dot_active_at_frame(i, frame) && dot_selected_at_frame(i, frame) == selected) {
                Dot d = display_dot_at_frame(i, frame);
                POINT p = project(v, &d);
                COLORREF c = ega(selected ? 5 : 4);
                HBRUSH b = CreateSolidBrush(c);
                RECT r = {p.x - 1, p.y - 1, p.x + 1, p.y + 1};
                FillRect(dc, &r, b);
                DeleteObject(b);
            }
}
static size_t show_all_frame_at(size_t step) { return g_shape.frame_count ? (g_current_frame + 1 + step) % g_shape.frame_count : 0; }
static int one_vertex_render_test(void) {
    HDC window = GetDC(g_hwnd), dc = window ? CreateCompatibleDC(window) : NULL;
    HBITMAP bitmap = dc ? CreateCompatibleBitmap(window, 20, 20) : NULL;
    if (!window || !dc || !bitmap) {
        if (bitmap)
            DeleteObject(bitmap);
        if (dc)
            DeleteDC(dc);
        if (window)
            ReleaseDC(g_hwnd, window);
        return 0;
    }
    HGDIOBJ old = SelectObject(dc, bitmap);
    HBRUSH black = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(dc, &(RECT){0, 0, 20, 20}, black);
    DeleteObject(black);
    View view = {{0, 0, 20, 20}, {0, 1}, {1, 1}, L""};
    draw_shape(dc, &view);
    COLORREF pixel = GetPixel(dc, 10, 10);
    SelectObject(dc, old);
    DeleteObject(bitmap);
    DeleteDC(dc);
    ReleaseDC(g_hwnd, window);
    return pixel == ega(6);
}

typedef struct {
    size_t index;
    double depth;
    POINT points[MAX_POLY_VERTS];
    int count;
} PreviewPoly;
static int preview_depth_cmp(const void *a, const void *b) {
    const PreviewPoly *x = (const PreviewPoly *)a, *y = (const PreviewPoly *)b;
    return x->depth > y->depth ? -1 : x->depth < y->depth ? 1
                                                          : 0;
}
static void bsp_preview_order(int node, const double cam[3], int *out, int *count) {
    if (node < 0 || node >= g_bsp_count)
        return;
    BSPNode *n = &g_bsp_nodes[node];
    if (n->leaf) {
        out[(*count)++] = n->poly;
        bsp_preview_order(n->front, cam, out, count);
        return;
    }
    double p[4];
    if (!polygon_plane((size_t)n->poly, p))
        return;
    double side = p[0] * cam[0] + p[1] * cam[1] + p[2] * cam[2] + p[3];
    int far_node = side >= 0 ? n->back : n->front, near_node = side >= 0 ? n->front : n->back;
    bsp_preview_order(far_node, cam, out, count);
    out[(*count)++] = n->poly;
    for (int poly = g_bsp_coplanar_head[node]; poly >= 0; poly = g_bsp_coplanar_next[poly])
        out[(*count)++] = poly;
    bsp_preview_order(near_node, cam, out, count);
}
/* The DOS callback assembled this command stream and sent it to separate
   Argonaut renderer hardware. This lower-right view is its embedded native
   replacement; the exact modal Preview remains a separate recovered path. */
static void draw_system_preview(HDC dc, const View *v) {
    PreviewPoly *list = (PreviewPoly *)calloc(g_shape.poly_count, sizeof(PreviewPoly));
    if (!list)
        return;
    size_t used = 0;
    double yaw = .65, pitch = -.38, cy = cos(yaw), sy = sin(yaw), cp = cos(pitch), sp = sin(pitch), distance = 400.0 / g_zoom;
    int *order = (int *)malloc(g_shape.poly_count * sizeof(int)), order_count = 0;
    if (!g_bsp_valid)
        build_bsp();
    if (order) {
        double cam[3] = {g_origin[0] - distance * cp * sy, g_origin[1] - distance * sp, g_origin[2] - distance * cp * cy};
        bsp_preview_order(g_bsp_root, cam, order, &order_count);
        uint8_t *seen = (uint8_t *)calloc(g_shape.poly_count, 1);
        if (seen) {
            for (int q = 0; q < order_count; q++)
                seen[order[q]] = 1;
            for (size_t i = 0; i < g_shape.poly_count; i++)
                if (!seen[i])
                    order[order_count++] = (int)i;
            free(seen);
        }
    }
    for (size_t pass = 0; pass < (order ? (size_t)order_count : g_shape.poly_count); pass++) {
        size_t i = order ? (size_t)order[pass] : pass;
        Poly *p = &g_shape.polys[i];
        if (p->count < 2 || !(p->flags & g_shape_display_mask))
            continue;
        PreviewPoly *q = &list[used];
        q->index = i;
        q->count = p->count;
        double depth = 0;
        int clipped = 0;
        for (unsigned j = 0; j < p->count; j++) {
            Dot d = display_dot(p->index[j]);
            double x = d.x - g_origin[0], y = d.y - g_origin[1], z = d.z - g_origin[2], rx = x * cy - z * sy, rz = x * sy + z * cy, ry = y * cp - rz * sp;
            rz = y * sp + rz * cp + distance;
            if (rz < 1) {
                clipped = 1;
                break;
            }
            double scale = (v->r.right - v->r.left) * .55 / rz;
            q->points[j].x = (v->r.left + v->r.right) / 2 + (int)(rx * scale);
            q->points[j].y = (v->r.top + v->r.bottom) / 2 - (int)(ry * scale);
            depth += rz;
        }
        if (!clipped) {
            q->depth = depth / p->count;
            used++;
        }
    }
    if (!order)
        qsort(list, used, sizeof(*list), preview_depth_cmp);
    for (size_t k = 0; k < used; k++) {
        PreviewPoly *q = &list[k];
        Poly *p = &g_shape.polys[q->index];
        COLORREF colour = snes_renderer_colour(p->colour);
        if (g_preview_mode == 2 && q->count >= 3) {
            HBRUSH b = CreateSolidBrush(colour);
            HPEN edge = CreatePen(PS_SOLID, 1, p->selected ? RGB(255, 255, 255) : colour);
            HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, edge);
            Polygon(dc, q->points, q->count);
            SelectObject(dc, ob);
            SelectObject(dc, op);
            DeleteObject(b);
            DeleteObject(edge);
        } else {
            HPEN pen = CreatePen(PS_SOLID, 1, p->selected ? RGB(255, 255, 255) : colour);
            HGDIOBJ old = SelectObject(dc, pen);
            for (int j = 0; j < q->count; j++) {
                POINT a = q->points[j], b = q->points[(j + 1) % q->count];
                MoveToEx(dc, a.x, a.y, NULL);
                LineTo(dc, b.x, b.y);
            }
            SelectObject(dc, old);
            DeleteObject(pen);
        }
    }
    free(order);
    free(list);
}

/* Exact matrix built by DOS PreviewFunct.  Its rows transform model X/Y/Z
   into preview X/Y/depth; screen Y deliberately grows downward. */
static void preview_matrix(double m[3][3]) {
    double sa = sin(g_preview_angles[0]), ca = cos(g_preview_angles[0]);
    double sb = sin(g_preview_angles[1]), cb = cos(g_preview_angles[1]);
    double sc = sin(g_preview_angles[2]), cc = cos(g_preview_angles[2]);
    m[0][0] = cb * cc + sb * sa * sc;
    m[0][1] = -cb * sc + sb * sa * cc;
    m[0][2] = ca * sb;
    m[1][0] = ca * sc;
    m[1][1] = ca * cc;
    m[1][2] = -sa;
    m[2][0] = -sb * cc + cb * sa * sc;
    m[2][1] = sb * sc + cb * sa * cc;
    m[2][2] = ca * cb;
}
static int preview_word(double value) {
    if (!isfinite(value))
        return 0;
    int64_t whole = (int64_t)trunc(value);
    return (int)(int16_t)(uint16_t)((uint64_t)whole & 0xffffu);
}
static POINT preview_project(Dot d, double m[3][3]) {
    double rx = m[0][0] * d.x + m[0][1] * d.y + m[0][2] * d.z;
    double ry = m[1][0] * d.x + m[1][1] * d.y + m[1][2] * d.z;
    double rz = m[2][0] * d.x + m[2][1] * d.y + m[2][2] * d.z;
    double denominator = g_preview_distance - rz;
    POINT p = {preview_word(g_preview_scale * rx / denominator), preview_word(g_preview_scale * ry / denominator)};
    return p;
}
static int preview_poly_visible(const Poly *p, double m[3][3]) {
    if (!g_preview_hidden || p->count < 3)
        return 1;
    Dot a = display_dot_at_frame(p->index[0], g_preview_frame), b = display_dot_at_frame(p->index[1], g_preview_frame), c = display_dot_at_frame(p->index[2], g_preview_frame);
    double ux = b.x - a.x, uy = b.y - a.y, uz = b.z - a.z, vx = c.x - a.x, vy = c.y - a.y, vz = c.z - a.z;
    double nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
    double camera[3] = {g_preview_distance * m[2][0], g_preview_distance * m[2][1], g_preview_distance * m[2][2]};
    return nx * camera[0] + ny * camera[1] + nz * camera[2] > nx * a.x + ny * a.y + nz * a.z;
}
static void draw_original_preview(HDC dc, RECT client) {
    double m[3][3];
    preview_matrix(m);
    int sx = client.right, sy = client.bottom;
    for (int selected = 0; selected < 2; selected++)
        for (size_t i = 0; i < g_shape.poly_count; i++) {
            Poly *p = &g_shape.polys[i];
            if (!p->flags || !p->count || p->selected != selected || !preview_poly_visible(p, m))
                continue;
            HPEN pen = CreatePen(PS_SOLID, 1, ega(selected ? 7 : 6));
            HGDIOBJ old = SelectObject(dc, pen);
            if (p->count == 1) {
                POINT point = preview_project(display_dot_at_frame(p->index[0], g_preview_frame), m);
                SetPixelV(dc, (280 + point.x) * sx / 640, (240 + point.y) * sy / 480, ega(selected ? 7 : 6));
            } else {
                for (unsigned j = 0; j < p->count; j++) {
                    POINT a = preview_project(display_dot_at_frame(p->index[j], g_preview_frame), m), b = preview_project(display_dot_at_frame(p->index[(j + 1) % p->count], g_preview_frame), m);
                    MoveToEx(dc, (280 + a.x) * sx / 640, (240 + a.y) * sy / 480, NULL);
                    LineTo(dc, (280 + b.x) * sx / 640, (240 + b.y) * sy / 480);
                }
            }
            SelectObject(dc, old);
            DeleteObject(pen);
        }
    for (int selected = 0; selected < 2; selected++)
        for (size_t i = 0; i < g_shape.dot_count; i++)
            if (dot_active_at_frame(i, g_preview_frame) && dot_selected_at_frame(i, g_preview_frame) == selected) {
                POINT p = preview_project(display_dot_at_frame(i, g_preview_frame), m);
                RECT r = {(279 + p.x) * sx / 640, (239 + p.y) * sy / 480, (281 + p.x) * sx / 640, (241 + p.y) * sy / 480};
                HBRUSH brush = CreateSolidBrush(ega(selected ? 5 : 4));
                FillRect(dc, &r, brush);
                DeleteObject(brush);
            }
}
static void enter_original_preview(void) {
    spread_current_frame_flags();
    g_preview_mode = 1;
    g_preview_scale = 500.0;
    g_preview_distance = 400.0 / g_zoom;
    g_preview_frame = g_current_frame;
}
static int preview_key_down(WPARAM key) {
    const double reciprocal = 0.90909090909090906;
    if (key == 'X' || key == 'Q' || key == VK_SPACE || key == VK_RETURN) {
        g_preview_mode = 0;
        statusf(L"Ready");
        return 1;
    }
    if (key == VK_LEFT)
        g_preview_angles[1] += .1;
    else if (key == VK_RIGHT)
        g_preview_angles[1] -= .1;
    else if (key == VK_UP)
        g_preview_angles[0] += .1;
    else if (key == VK_DOWN)
        g_preview_angles[0] -= .1;
    else if (key == VK_PRIOR)
        g_preview_scale *= 1.1;
    else if (key == VK_NEXT)
        g_preview_scale *= reciprocal;
    else if (key == VK_ADD || (key == VK_OEM_PLUS && (GetKeyState(VK_SHIFT) & 0x8000)))
        g_preview_distance *= 1.1;
    else if (key == VK_SUBTRACT || key == VK_OEM_MINUS)
        g_preview_distance *= reciprocal;
    else if (key == VK_OEM_COMMA || key == VK_OEM_4)
        g_preview_angles[2] += .1;
    else if (key == VK_OEM_PERIOD || key == VK_OEM_6)
        g_preview_angles[2] -= .1;
    else if (key == 'H')
        g_preview_hidden ^= 1;
    else if (key == VK_HOME)
        g_preview_angles[0] = g_preview_angles[1] = g_preview_angles[2] = 0;
    else if (key == 'P') {
        size_t frames = g_shape.frame_count ? g_shape.frame_count : 1;
        g_preview_frame = (g_preview_frame + frames - 1) % frames;
    } else if (key == 'N') {
        size_t frames = g_shape.frame_count ? g_shape.frame_count : 1;
        g_preview_frame = (g_preview_frame + 1) % frames;
    } else
        return 0;
    return 1;
}

static int file_entry_compare(const void *a, const void *b) {
    const FileEntry *x = (const FileEntry *)a, *y = (const FileEntry *)b;
    return _wcsicmp(x->name, y->name);
}
static void refresh_file_entries(void) {
    g_file_count = 0;
    wchar_t pattern[MAX_PATH];
    wsprintfW(pattern, L"%ls\\*", g_file_directory);
    WIN32_FIND_DATAW data;
    HANDLE find = FindFirstFileW(pattern, &data);
    if (find != INVALID_HANDLE_VALUE) {
        do {
            if (!wcscmp(data.cFileName, L"."))
                continue;
            if (g_file_count >= MAX_FILE_ENTRIES)
                break;
            FileEntry *entry = &g_file_entries[g_file_count++];
            wcsncpy(entry->name, data.cFileName, MAX_PATH - 1);
            entry->name[MAX_PATH - 1] = 0;
            entry->directory = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        } while (FindNextFileW(find, &data));
        FindClose(find);
    }
    qsort(g_file_entries, g_file_count, sizeof(g_file_entries[0]), file_entry_compare);
    int maximum = g_file_count ? (int)((g_file_count - 1) / 30) : 0;
    if (maximum > 7)
        maximum = 7;
    if (g_file_page > maximum)
        g_file_page = maximum;
    if (g_file_page < 0)
        g_file_page = 0;
}
static void set_file_directory(const wchar_t *path) {
    wchar_t full[MAX_PATH];
    if (!GetFullPathNameW(path, MAX_PATH, full, NULL))
        return;
    size_t n = wcslen(full);
    while (n > 3 && (full[n - 1] == L'\\' || full[n - 1] == L'/'))
        full[--n] = 0;
    DWORD attr = GetFileAttributesW(full);
    if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY))
        return;
    wcscpy(g_file_directory, full);
    g_file_page = 0;
    refresh_file_entries();
}
static void enter_file_directory(const wchar_t *name) {
    wchar_t path[MAX_PATH];
    wsprintfW(path, L"%ls\\%ls", g_file_directory, name);
    set_file_directory(path);
    g_file_name[0] = 0;
    g_file_active_edit = 0;
    InvalidateRect(g_hwnd, NULL, FALSE);
}
static void draw_file_selector(HDC dc, RECT client) {
    int right = client.right * 560 / 640, y380 = client.bottom * 380 / 480, y430 = client.bottom * 430 / 480;
    HPEN cyan = CreatePen(PS_SOLID, 1, RGB(0, 255, 255));
    HGDIOBJ op = SelectObject(dc, cyan);
    SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(dc, 0, 0, right, client.bottom);
    MoveToEx(dc, 0, y380, NULL);
    LineTo(dc, right, y380);
    MoveToEx(dc, 0, y430, NULL);
    LineTo(dc, right, y430);
    for (int x = 250; x <= 490; x += (x == 250 ? 70 : x == 320 ? 100
                                                               : 70)) {
        int sx = client.right * x / 640;
        MoveToEx(dc, sx, y430, NULL);
        LineTo(dc, sx, client.bottom);
    }
    SelectObject(dc, op);
    DeleteObject(cyan);
    draw_fastgraph_text(dc, client.right * 230 / 640, 2, g_file_title, RGB(0, 255, 255));
    for (int column = 0; column < 5; column++)
        for (int row = 0; row < 30; row++) {
            size_t index = (size_t)(g_file_page + column) * 30 + (size_t)row;
            if (index >= g_file_count)
                break;
            FileEntry *entry = &g_file_entries[index];
            wchar_t display[16];
            if (entry->directory)
                _snwprintf(display, 16, L"[%.*ls]", 11, entry->name);
            else
                _snwprintf(display, 16, L"%.*ls", 13, entry->name);
            display[15] = 0;
            draw_fastgraph_text(dc, client.right * (2 + column * 104) / 640, client.bottom * (14 + row * 12) / 480, display, entry->directory ? ega(7) : ega(6));
        }
    draw_fastgraph_text(dc, client.right * 10 / 640, client.bottom * 396 / 480, L"Path :", RGB(0, 255, 255));
    draw_fastgraph_text(dc, client.right * 100 / 640, client.bottom * 396 / 480, g_file_directory, RGB(0, 255, 255));
    draw_fastgraph_text(dc, client.right * 10 / 640, client.bottom * 436 / 480, L"Name :", RGB(0, 255, 255));
    draw_fastgraph_text(dc, client.right * 100 / 640, client.bottom * 436 / 480, g_file_name, RGB(0, 255, 255));
    draw_fastgraph_text(dc, client.right * 260 / 640, client.bottom * 436 / 480, L"OK", RGB(0, 255, 255));
    draw_fastgraph_text(dc, client.right * 330 / 640, client.bottom * 436 / 480, L"CANCEL", RGB(0, 255, 255));
    draw_fastgraph_text(dc, client.right * 430 / 640, client.bottom * 436 / 480, L"<<", RGB(0, 255, 255));
    draw_fastgraph_text(dc, client.right * 500 / 640, client.bottom * 436 / 480, L">>", RGB(0, 255, 255));
    if (g_file_active_edit == 1)
        draw_fastgraph_text(dc, client.right * (100 + (int)wcslen(g_file_directory) * 8) / 640, client.bottom * 396 / 480, L"_", RGB(0, 255, 255));
    else if (g_file_active_edit == 2)
        draw_fastgraph_text(dc, client.right * (100 + (int)wcslen(g_file_name) * 8) / 640, client.bottom * 436 / 480, L"_", RGB(0, 255, 255));
}
static void accept_file_selector(void) {
    if (!g_file_name[0]) {
        MessageBeep(MB_ICONWARNING);
        return;
    }
    if ((wcslen(g_file_name) > 2 && g_file_name[1] == L':') || g_file_name[0] == L'\\' || g_file_name[0] == L'/')
        wcsncpy(g_file_output, g_file_name, MAX_PATH - 1);
    else
        _snwprintf(g_file_output, MAX_PATH, L"%ls\\%ls", g_file_directory, g_file_name);
    g_file_output[MAX_PATH - 1] = 0;
    if (!g_file_save) {
        DWORD attr = GetFileAttributesW(g_file_output);
        if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY)) {
            MessageBeep(MB_ICONWARNING);
            return;
        }
    }
    g_file_result = 1;
}
static void handle_file_selector_click(int x, int y) {
    RECT c;
    GetClientRect(g_hwnd, &c);
    int lx = c.right ? x * 640 / c.right : 0, ly = c.bottom ? y * 480 / c.bottom : 0;
    if (lx < 0 || lx >= 560)
        return;
    if (ly < 380) {
        if (ly < 14)
            return;
        int column = lx / 104, row = (ly - 14) / 12;
        if (column < 5 && row < 30) {
            size_t index = (size_t)(g_file_page + column) * 30 + (size_t)row;
            if (index < g_file_count) {
                FileEntry *entry = &g_file_entries[index];
                if (entry->directory)
                    enter_file_directory(entry->name);
                else {
                    wcscpy(g_file_name, entry->name);
                    g_file_active_edit = 0;
                    InvalidateRect(g_hwnd, NULL, FALSE);
                }
            }
        }
    } else if (ly < 430) {
        g_file_active_edit = 1;
        g_file_replace_text = 1;
        InvalidateRect(g_hwnd, NULL, FALSE);
    } else if (lx < 250) {
        g_file_active_edit = 2;
        g_file_replace_text = 1;
        InvalidateRect(g_hwnd, NULL, FALSE);
    } else if (lx < 320)
        accept_file_selector();
    else if (lx < 420)
        g_file_result = 0;
    else if (lx < 490) {
        if (g_file_page > 0)
            g_file_page--;
        InvalidateRect(g_hwnd, NULL, FALSE);
    } else {
        int maximum = g_file_count ? (int)((g_file_count - 1) / 30) : 0;
        if (maximum > 7)
            maximum = 7;
        if (g_file_page < maximum)
            g_file_page++;
        InvalidateRect(g_hwnd, NULL, FALSE);
    }
}

static void paint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd, &ps);
    RECT c;
    GetClientRect(hwnd, &c);
    HDC mem = CreateCompatibleDC(dc);
    HBITMAP bm = CreateCompatibleBitmap(dc, c.right, c.bottom);
    HGDIOBJ old = SelectObject(mem, bm);
    HBRUSH black = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(mem, &c, black);
    DeleteObject(black);
    if (g_file_selector)
        draw_file_selector(mem, c);
    else if (g_preview_mode == 1) {
        draw_panels(mem, c);
        RECT preview_area = {0, 0, c.right * 560 / 640, c.bottom};
        HBRUSH blank = CreateSolidBrush(RGB(0, 0, 0));
        FillRect(mem, &preview_area, blank);
        DeleteObject(blank);
        draw_original_preview(mem, c);
    } else {
        View v[4];
        make_views(c, v);
        for (int i = 0; i < 3; i++) {
            draw_grid(mem, &v[i]);
            draw_shape(mem, &v[i]);
            if (g_show_all_frames && g_shape.frame_count)
                for (size_t step = 0; step < g_shape.frame_count; step++)
                    draw_frame_dots(mem, &v[i], show_all_frame_at(step));
        }
        if (g_preview_mode == 2)
            draw_system_preview(mem, &v[3]);
        else {
            HBRUSH blank = CreateSolidBrush(RGB(0, 0, 0));
            FillRect(mem, &v[3].r, blank);
            DeleteObject(blank);
        }
        draw_panels(mem, c);
        draw_fastgraph_text(mem, -1, 2, APP_TITLE, RGB(0, 255, 255));
        RECT status_box = {c.right * 282 / 640, c.bottom * 431 / 480, c.right * 557 / 640, c.bottom * 450 / 480};
        HBRUSH status_blue = CreateSolidBrush(RGB(0, 0, 255));
        FillRect(mem, &status_box, status_blue);
        DeleteObject(status_blue);
        draw_fastgraph_text(mem, c.right * 282 / 640, c.bottom * 434 / 480, g_status, RGB(0, 255, 255));
        draw_cursor_coords(mem, c);
        draw_dos_prompt(mem, c);
    }
    BitBlt(dc, 0, 0, c.right, c.bottom, mem, 0, 0, SRCCOPY);
    SelectObject(mem, old);
    DeleteObject(bm);
    DeleteDC(mem);
    EndPaint(hwnd, &ps);
}

static int choose_file(int save, const wchar_t *title, wchar_t out[MAX_PATH]) {
    GetCurrentDirectoryW(MAX_PATH, g_file_directory);
    g_file_name[0] = 0;
    if (out[0]) {
        wchar_t full[MAX_PATH], *leaf = NULL;
        if (GetFullPathNameW(out, MAX_PATH, full, &leaf) && leaf) {
            wcscpy(g_file_name, leaf);
            if (leaf > full) {
                while (leaf > full && (leaf[-1] == L'\\' || leaf[-1] == L'/'))
                    leaf--;
                *leaf = 0;
                if (full[0])
                    wcscpy(g_file_directory, full);
            }
        }
    }
    wcscpy(g_file_title, title);
    g_file_save = save;
    g_file_page = 0;
    g_file_active_edit = 0;
    g_file_replace_text = 0;
    g_file_result = -1;
    refresh_file_entries();
    g_file_selector = 1;
    InvalidateRect(g_hwnd, NULL, FALSE);
    UpdateWindow(g_hwnd);
    MSG msg;
    while (g_file_result < 0 && GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    int accepted = g_file_result > 0;
    if (accepted)
        wcscpy(out, g_file_output);
    g_file_selector = 0;
    g_file_active_edit = 0;
    InvalidateRect(g_hwnd, NULL, FALSE);
    UpdateWindow(g_hwnd);
    return accepted;
}

static int save_shape(const wchar_t *path) {
    FILE *f = _wfopen(path, L"wb");
    if (!f)
        return 0;
    /* Exact grammar emitted by the DOS _SaveM3d routine. */
    size_t active_dots = 0;
    for (size_t i = 0; i < g_shape.dot_count; i++)
        if (dot_active(i))
            active_dots++;
    fprintf(f, "3DG1\n%llu\n", (unsigned long long)active_dots);
    for (size_t i = 0; i < g_shape.dot_count; i++)
        if (dot_active(i)) {
            Dot d = display_dot(i);
            fprintf(f, "%.0f %.0f %.0f\n", d.x, d.y, d.z);
        }
    for (size_t i = 0; i < g_shape.poly_count; i++) {
        Poly *p = &g_shape.polys[i];
        if (!p->flags)
            continue;
        fprintf(f, "%u ", p->count);
        for (unsigned j = 0; j < p->count; j++)
            fprintf(f, "%u ", p->index[j]);
        fprintf(f, "%u\n", p->colour);
    }
    fclose(f);
    return 1;
}

static int save_internal(const wchar_t *path) {
    FILE *f = _wfopen(path, L"wb");
    if (!f)
        return 0;
    size_t frames = g_shape.frame_count ? g_shape.frame_count : 1, slots = 0;
    for (size_t i = 0; i < g_shape.dot_count; i++)
        if (dot_active(i))
            slots = i + 1;
    fprintf(f, "3DCG\n%llu %llu\n", (unsigned long long)slots, (unsigned long long)frames);
    for (size_t frame = 0; frame < frames; frame++)
        for (size_t i = 0; i < slots; i++) {
            double x = g_shape.dots[i].x, y = g_shape.dots[i].y, z = g_shape.dots[i].z;
            unsigned active = 1;
            if (g_shape.frame_count && g_shape.frames[frame]) {
                FrameDot *d = &g_shape.frames[frame][i];
                x = d->x;
                y = d->y;
                z = d->z;
                active = d->active;
            }
            if (active)
                fprintf(f, "%.0f %.0f %.0f,%u\n", x, y, z, active);
            else
                fputs("0 0 0,0\n", f);
        }
    for (size_t i = 0; i < g_shape.poly_count; i++) {
        Poly *p = &g_shape.polys[i];
        if (!p->flags)
            continue;
        fprintf(f, "%u ", p->count);
        for (unsigned j = 0; j < p->count; j++)
            fprintf(f, "%u ", p->index[j]);
        uint16_t flags = (uint16_t)((p->flags & ~0x0100u) | (p->selected ? 0x0100 : 0));
        fprintf(f, ",%u 0x%X 0x%X\n", p->colour, flags, p->type);
    }
    fclose(f);
    return 1;
}

static void asm_name(const wchar_t *path, char out[64]) {
    const wchar_t *start = path, *p = path;
    for (; *p; p++)
        if (*p == L'\\' || *p == L':' || *p == L'/')
            start = p + 1;
    const wchar_t *end = start;
    while (*end && *end != L'.')
        end++;
    int count = (int)(end - start);
    if (count > 62)
        count = 62;
    if (count > 0)
        WideCharToMultiByte(CP_ACP, 0, start, count, out, 63, NULL, NULL);
    else
        out[0] = 'S', count = 1;
    out[count] = 0;
}
static void source_name(char out[MAX_PATH]) {
    if (g_path[0])
        WideCharToMultiByte(CP_ACP, 0, g_path, -1, out, MAX_PATH, NULL, NULL);
    else
        strcpy(out, "SHAPED");
}
static void poly_normal(const Poly *p, double *n) {
    n[0] = n[1] = n[2] = 0;
    if (p->count < 3)
        return;
    Dot a = display_dot(p->index[0]), b = display_dot(p->index[1]), c = display_dot(p->index[2]);
    double ux = b.x - a.x, uy = b.y - a.y, uz = b.z - a.z, vx = c.x - a.x, vy = c.y - a.y, vz = c.z - a.z;
    n[0] = uy * vz - uz * vy;
    n[1] = uz * vx - ux * vz;
    n[2] = ux * vy - uy * vx;
    double l = sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
    if (l) {
        n[0] = n[0] * 127 / l;
        n[1] = n[1] * 127 / l;
        n[2] = n[2] * 127 / l;
    }
    for (int axis = 0; axis < 3; axis++)
        if (fabs(n[axis]) < 0.0000001)
            n[axis] = 0;
}
typedef struct {
    double radius, x, y, z;
} AsmBounds;
static size_t g_asm_override_count;
static size_t g_asm_override_slot[8];
static Dot g_asm_override_dot[8];
static int asm_override_find(size_t index) {
    for (size_t i = 0; i < g_asm_override_count; i++)
        if (g_asm_override_slot[i] == index)
            return (int)i;
    return -1;
}
static Dot asm_existing_dot_at(size_t index, size_t frame) {
    int override = asm_override_find(index);
    return override >= 0 ? g_asm_override_dot[override] : display_dot_at_frame(index, frame);
}
static int asm_existing_dot_active_at(size_t index, size_t frame) { return asm_override_find(index) >= 0 || dot_active_at_frame(index, frame); }
static void asm_bounds_add(AsmBounds *b, Dot d) {
    double radius = sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
    if (radius > b->radius)
        b->radius = radius;
    if (fabs(d.x) > b->x)
        b->x = fabs(d.x);
    if (fabs(d.y) > b->y)
        b->y = fabs(d.y);
    if (fabs(d.z) > b->z)
        b->z = fabs(d.z);
}
static AsmBounds asm_bounds(const Dot *extra, size_t extra_count) {
    AsmBounds b = {0};
    size_t frames = g_shape.frame_count ? g_shape.frame_count : 1;
    for (size_t f = 0; f < frames; f++)
        for (size_t i = 0; i < g_shape.dot_count; i++)
            if (asm_existing_dot_active_at(i, f))
                asm_bounds_add(&b, asm_existing_dot_at(i, f));
    for (size_t i = 0; i < extra_count; i++)
        asm_bounds_add(&b, extra[i]);
    return b;
}
static void write_asm_header(FILE *f, const char *name, const Dot *extra, size_t extra_count) {
    AsmBounds b = asm_bounds(extra, extra_count);
    char source[MAX_PATH];
    source_name(source);
    const char *coltab = g_coltab_count && g_coltab_index >= 0 && g_coltab_index < g_coltab_count ? g_coltab_entries[g_coltab_index].name : "";
    fprintf(f, ";--Shape file ----- %s ----\n\tifne\tDO_HDR\n\n%s\n", source, name);
    fprintf(f, "\tShapeHdr\t%s_P,0,%s_F,0,0,0,0,0,0,%.0f,%.0f,%.0f,%.0f,%s,0,0,0,0,<%s>\n\telseif\n", name, name, b.x, b.y, b.z, b.radius, coltab, name);
}
static Dot asm_dot_at(size_t index, size_t frame, const Dot *extra, size_t extra_count) {
    if (index < g_shape.dot_count)
        return asm_existing_dot_at(index, frame);
    size_t e = index - g_shape.dot_count;
    return e < extra_count ? extra[e] : (Dot){0};
}
static int asm_dot_active_at(size_t index, size_t frame, size_t extra_count) {
    if (index < g_shape.dot_count)
        return asm_existing_dot_active_at(index, frame);
    return index - g_shape.dot_count < extra_count;
}
static void write_asm_point(FILE *f, char width, Dot d, size_t index, int active) { fprintf(f, "\tp%c\t%d,%d,%d\t;%llu%s\n", width, (int)dos_coord(-d.x), (int)dos_coord(-d.y), (int)dos_coord(d.z), (unsigned long long)index, active ? "" : "  **"); }
static void write_asm_point_run(FILE *f, char width, const uint8_t *kind, size_t start, size_t count, size_t frame, const Dot *extra, size_t extra_count) {
    size_t pos = start, end = start + count;
    while (pos < end) {
        uint8_t exact = kind[pos];
        size_t run = 1;
        while (pos + run < end && kind[pos + run] == exact)
            run++;
        if (exact & 2) {
            fprintf(f, "\tPointsX%c\t%llu\n", width, (unsigned long long)(run / 2));
            for (size_t i = pos; i < pos + run; i += 2)
                write_asm_point(f, width, asm_dot_at(i, frame, extra, extra_count), i, asm_dot_active_at(i, frame, extra_count));
        } else {
            fprintf(f, "\tPoints%c\t%llu\n", width, (unsigned long long)run);
            for (size_t i = pos; i < pos + run; i++)
                write_asm_point(f, width, asm_dot_at(i, frame, extra, extra_count), i, asm_dot_active_at(i, frame, extra_count));
        }
        pos += run;
    }
}
static void write_asm_points_extra(FILE *f, const char *name, const Dot *extra, size_t extra_count) {
    AsmBounds b = asm_bounds(extra, extra_count);
    char width = b.radius > 127 ? 'w' : 'b';
    size_t count = g_shape.dot_count + extra_count, frames = g_shape.frame_count ? g_shape.frame_count : 1;
    uint8_t kind[MAX_DOTS + 8] = {0};
    for (size_t i = 0; i < count; i++)
        if (asm_dot_active_at(i, 0, extra_count)) {
            kind[i] = 2;
            Dot base = asm_dot_at(i, 0, extra, extra_count);
            for (size_t frame = 1; frame < frames; frame++) {
                Dot d = asm_dot_at(i, frame, extra, extra_count);
                if (d.x != base.x || d.y != base.y || d.z != base.z) {
                    kind[i] |= 1;
                    break;
                }
            }
        }
    for (size_t i = 0; i < count;) {
        int mirrored = 0;
        if ((kind[i] & 2) && i + 1 < count && (kind[i + 1] & 2)) {
            mirrored = 1;
            for (size_t frame = 0; frame < frames; frame++) {
                Dot a = asm_dot_at(i, frame, extra, extra_count), d = asm_dot_at(i + 1, frame, extra, extra_count);
                if (a.x != -d.x || a.y != d.y || a.z != d.z || !asm_dot_active_at(i, frame, extra_count) || !asm_dot_active_at(i + 1, frame, extra_count)) {
                    mirrored = 0;
                    break;
                }
            }
        }
        if (mirrored)
            i += 2;
        else {
            kind[i] &= (uint8_t)~2u;
            i++;
        }
    }
    fprintf(f, "%s_P\n", name);
    size_t pos = 0;
    int animated_block = 0;
    while (pos < count) {
        int animated = kind[pos] & 1;
        size_t run = 1;
        while (pos + run < count && ((kind[pos + run] & 1) != 0) == animated)
            run++;
        if (!animated)
            write_asm_point_run(f, width, kind, pos, run, 0, extra, extra_count);
        else {
            char letter = (char)('A' + animated_block);
            fprintf(f, "\tFrames\t%llu\n", (unsigned long long)frames);
            for (size_t frame = 0; frame < frames; frame++)
                fprintf(f, "\tjumptab\t.A%llu%c\n", (unsigned long long)frame, letter);
            for (size_t frame = 0; frame < frames; frame++) {
                fprintf(f, ".A%llu%c", (unsigned long long)frame, letter);
                write_asm_point_run(f, width, kind, pos, run, frame, extra, extra_count);
                if (frame + 1 < frames)
                    fprintf(f, "\tjump\t.EB%d\n", animated_block);
            }
            fprintf(f, ".EB%d\n", animated_block);
            animated_block++;
        }
        pos += run;
    }
    fputs("\n\tEndPoints\n", f);
}
static void write_asm_points(FILE *f, const char *name) { write_asm_points_extra(f, name, NULL, 0); }
static int poly_vizi(size_t index) {
    if (index >= g_shape.poly_count || g_shape.polys[index].count <= 2)
        return -1;
    int vizi = 0;
    for (size_t i = 0; i < index; i++)
        if (g_shape.polys[i].flags && g_shape.polys[i].count > 2)
            vizi++;
    return vizi;
}
static void write_asm_vizis(FILE *f) {
    size_t count = 0;
    for (size_t i = 0; i < g_shape.poly_count; i++)
        if (g_shape.polys[i].flags && g_shape.polys[i].count > 2)
            count++;
    fprintf(f, "\tVizis\t%llu\n", (unsigned long long)count);
    for (size_t i = 0; i < g_shape.poly_count; i++) {
        Poly *p = &g_shape.polys[i];
        if (!p->flags || p->count <= 2)
            continue;
        double n[3];
        poly_normal(p, n);
        fprintf(f, "\tViz\t%u,%u,%u,%.0f,%.0f,%.0f\t;%llu\n", p->index[0], p->index[1], p->index[2], n[0], n[1], n[2], (unsigned long long)i);
    }
}
static void write_asm_vertex_normals(FILE *f, const char *name, const Dot *extra, size_t extra_count) {
    (void)extra;
    size_t extent = g_shape.dot_count + extra_count, count = 0;
    for (size_t i = 0; i < extent; i++)
        if (i < g_shape.dot_count ? asm_existing_dot_active_at(i, g_current_frame) : 1)
            count = i + 1;
    fprintf(f, "%s_VN\t\t;Vertex normals\n\tVNORMALS\t%llu\n", name, (unsigned long long)count);
    for (size_t dot = 0; dot < count; dot++) {
        double sum[3] = {0};
        int adjacent = 0;
        for (size_t i = 0; i < g_shape.poly_count; i++) {
            Poly *p = &g_shape.polys[i];
            if (!p->flags)
                continue;
            int contains = 0;
            for (unsigned j = 0; j < p->count; j++)
                if (p->index[j] == dot) {
                    contains = 1;
                    break;
                }
            if (contains) {
                double n[3];
                poly_normal(p, n);
                sum[0] += n[0];
                sum[1] += n[1];
                sum[2] += n[2];
                adjacent++;
            }
        }
        double length = sqrt(sum[0] * sum[0] + sum[1] * sum[1] + sum[2] * sum[2]);
        if (length > 0.0001) {
            sum[0] = sum[0] * 127.0 / length;
            sum[1] = -sum[1] * 127.0 / length;
            sum[2] = -sum[2] * 127.0 / length;
        } else
            sum[0] = sum[1] = sum[2] = 0;
        int active = dot < g_shape.dot_count ? asm_existing_dot_active_at(dot, g_current_frame) : 1;
        fprintf(f, "\tVN\t%.0f,%.0f,%.0f\t;%llu-(%d)%s\n", sum[0], sum[1], sum[2], (unsigned long long)dot, adjacent, active ? "" : "\t**");
    }
}
static int save_gzs(const wchar_t *path) {
    FILE *f = _wfopen(path, L"wb");
    if (!f)
        return 0;
    char name[64];
    asm_name(path, name);
    size_t group_counts[8] = {0}, group_vertices[8] = {0};
    Dot centers[8] = {{0}};
    for (size_t i = 0; i < g_shape.poly_count; i++) {
        Poly *p = &g_shape.polys[i];
        for (int group = 0; group < 8; group++)
            if (p->flags & (1u << group)) {
                group_counts[group]++;
                for (unsigned j = 0; j < p->count; j++) {
                    Dot d = display_dot(p->index[j]);
                    centers[group].x = dos_coord(centers[group].x + dos_coord(d.x));
                    centers[group].y = dos_coord(centers[group].y + dos_coord(d.y));
                    centers[group].z = dos_coord(centers[group].z + dos_coord(d.z));
                    group_vertices[group]++;
                }
            }
    }
    Dot appended[8];
    size_t point_index[8];
    int entry_group[8], entry_count = 0, append_count = 0, groups = 0;
    g_asm_override_count = 0;
    for (int group = 0; group < 8; group++)
        if (group_counts[group]) {
            groups++;
            if (group_vertices[group]) {
                centers[group].x = dos_coord(centers[group].x / (double)group_vertices[group]);
                centers[group].y = dos_coord(centers[group].y / (double)group_vertices[group]);
                centers[group].z = dos_coord(centers[group].z / (double)group_vertices[group]);
            }
            size_t slot = g_shape.dot_count;
            for (size_t i = 0; i < g_shape.dot_count; i++)
                if (!dot_active_at_frame(i, 0) && asm_override_find(i) < 0) {
                    slot = i;
                    break;
                }
            if (slot < g_shape.dot_count) {
                g_asm_override_slot[g_asm_override_count] = slot;
                g_asm_override_dot[g_asm_override_count++] = centers[group];
            } else {
                slot = g_shape.dot_count + (size_t)append_count;
                appended[append_count++] = centers[group];
            }
            point_index[entry_count] = slot;
            entry_group[entry_count++] = group;
        }
    write_asm_header(f, name, appended, (size_t)append_count);
    write_asm_points_extra(f, name, appended, (size_t)append_count);
    fprintf(f, "%s_F\n", name);
    write_asm_vizis(f);
    if (g_smooth_shade)
        write_asm_vertex_normals(f, name, appended, (size_t)append_count);
    if (groups > 1) {
        fprintf(f, "\tGroups\t%d\n", groups);
        for (int i = 0; i < entry_count; i++)
            fprintf(f, "\tGroupP\t%llu\t;%d\n", (unsigned long long)point_index[i], entry_group[i]);
        for (int i = 0; i < entry_count; i++)
            fprintf(f, "\tGroupF\t%s_f%d\n", name, entry_group[i]);
    }
    for (int group = 0; group < 8; group++)
        if (group_counts[group]) {
            if (groups > 1)
                fprintf(f, "%s_f%d\n", name, group);
            fprintf(f, "\tFaces\t%llu\n", (unsigned long long)group_counts[group]);
            for (size_t i = 0; i < g_shape.poly_count; i++) {
                Poly *p = &g_shape.polys[i];
                if (!(p->flags & (1u << group)))
                    continue;
                double n[3];
                poly_normal(p, n);
                fprintf(f, "\tFace%u\t%u,%d,%.0f,%.0f,%.0f", p->count, p->colour, poly_vizi(i), n[0], n[1], n[2]);
                for (unsigned j = 0; j < p->count; j++)
                    fprintf(f, ",%u", p->index[j]);
                fputc('\n', f);
            }
            fputs("\tFendQ\n", f);
        }
    fputs("\n\tendshape\n\n\tendc\n", f);
    g_asm_override_count = 0;
    fclose(f);
    return 1;
}
static void write_bsp_tree(FILE *f, const char *name, int node, int *number) {
    if (node < 0)
        return;
    BSPNode *n = &g_bsp_nodes[node];
    int face = *number;
    if (n->leaf) {
        if (n->front >= 0)
            write_bsp_tree(f, name, n->front, number);
        else {
            (*number)++;
            fprintf(f, "\tBSPE\t%s_f%d\n", name, face);
        }
        return;
    }
    if (n->front < 0 && n->back < 0) {
        (*number)++;
        fprintf(f, "\tBSPE\t%s_f%d\n", name, face);
        return;
    }
    if (n->front >= 0) {
        int branch = face + 1;
        *number += 2;
        fprintf(f, "\tBSP\t%d,%s_f%d,.bsp%d\n", poly_vizi((size_t)n->poly), name, face, branch);
        if (n->back >= 0)
            write_bsp_tree(f, name, n->back, number);
        else
            fputs("\tBSPEND\n", f);
        fprintf(f, ".bsp%d", branch);
        write_bsp_tree(f, name, n->front, number);
    } else {
        (*number)++;
        fprintf(f, "\tBSPNULL\t%d,%s_f%d\n", poly_vizi((size_t)n->poly), name, face);
        if (n->back >= 0)
            write_bsp_tree(f, name, n->back, number);
        else
            fputs("\tBSPEND\n", f);
    }
}
static void write_bsp_poly_face(FILE *f, size_t index) {
    Poly *p = &g_shape.polys[index];
    double v[3];
    poly_normal(p, v);
    fprintf(f, "\tFace%u\t%u,%d,%.0f,%.0f,%.0f", p->count, p->colour, poly_vizi(index), v[0], v[1], v[2]);
    for (unsigned j = 0; j < p->count; j++)
        fprintf(f, ",%u", p->index[j]);
    fputc('\n', f);
}
static int has_bsp_line_faces(void) {
    for (size_t i = 0; i < g_shape.poly_count; i++)
        if (g_shape.polys[i].flags && g_shape.polys[i].count == 2)
            return 1;
    return 0;
}
static void write_bsp_line_faces(FILE *f) {
    for (size_t i = 0; i < g_shape.poly_count; i++)
        if (g_shape.polys[i].flags && g_shape.polys[i].count == 2)
            write_bsp_poly_face(f, i);
}
static void write_bsp_faces(FILE *f, const char *name, int node, int *number) {
    if (node < 0)
        return;
    BSPNode *n = &g_bsp_nodes[node];
    write_bsp_poly_face(f, (size_t)n->poly);
    for (int poly = g_bsp_coplanar_head[node]; poly >= 0; poly = g_bsp_coplanar_next[poly])
        write_bsp_poly_face(f, (size_t)poly);
    if (n->leaf) {
        write_bsp_faces(f, name, n->front, number);
        return;
    }
    if (n->front >= 0)
        (*number)++;
    if (n->back >= 0) {
        int face = (*number)++;
        fprintf(f, "\tFendQ\n%s_f%d\tFaces\n", name, face);
        write_bsp_faces(f, name, n->back, number);
    }
    if (n->front >= 0) {
        int face = (*number)++;
        fprintf(f, "\tFendQ\n%s_f%d\tFaces\n", name, face);
        write_bsp_faces(f, name, n->front, number);
    }
}
static int save_bsp_asm_body(const wchar_t *path) {
    FILE *f = _wfopen(path, L"wb");
    if (!f)
        return 0;
    if (!g_bsp_valid)
        build_bsp();
    char name[64];
    asm_name(path, name);
    int has_lines = has_bsp_line_faces();
    write_asm_header(f, name, NULL, 0);
    write_asm_points(f, name);
    fprintf(f, "%s_F\n", name);
    write_asm_vizis(f);
    if (g_smooth_shade)
        write_asm_vertex_normals(f, name, NULL, 0);
    if (g_bsp_flat && (g_bsp_root >= 0 || has_lines)) {
        fprintf(f, "\n%s_f1\tFaces\n", name);
        write_bsp_line_faces(f);
        int number = 2;
        write_bsp_faces(f, name, g_bsp_root, &number);
        fputs("\tFend\n\tEndShape\n\n\tendc\n", f);
    } else {
        fprintf(f, "\tBSPInit\t%s_EBSP\n", name);
        int number = 1;
        write_bsp_tree(f, name, g_bsp_root, &number);
        if (g_bsp_root >= 0) {
            fprintf(f, "\n%s_f1\tFaces\n", name);
            write_bsp_line_faces(f);
            number = 2;
            write_bsp_faces(f, name, g_bsp_root, &number);
            fprintf(f, "\tFendQ\n%s_EBSP\n\tEndShape\n\n\tendc\n", name);
        } else
            fprintf(f, "\tBSPEND\n%s_EBSP\n\tEndShape\n\n\tendc\n", name);
    }
    fclose(f);
    return 1;
}
static int save_bsp_asm(const wchar_t *path) {
    Shape saved, working = {0};
    if (!begin_export_bsp(&saved, &working))
        return 0;
    int ok = save_bsp_asm_body(path);
    end_export_bsp(&saved, &working);
    return ok;
}
typedef struct {
    int value[MAX_POLYS * 2];
    size_t paired, count;
} PcComponents;
typedef struct {
    int value[MAX_DOTS * 2], offset[MAX_DOTS * 2];
    size_t count;
} PcAxisMap;
static int pc_dot_component_at(size_t index, size_t frame, int axis) {
    Dot d = display_dot_at_frame(index, frame);
    if (axis == 0)
        return (int)d.x;
    if (axis == 1)
        return (int)dos_coord(-d.y);
    return (int)d.z;
}
static int pc_axis_find(const PcAxisMap *m, int value) {
    for (size_t i = 0; i < m->count; i++)
        if (m->value[i] == value)
            return (int)i;
    return -1;
}
static int pc_axis_eligible(size_t index, size_t frame, const uint16_t *flags, int changed_only, uint16_t skip) {
    if (index >= g_shape.dot_count || !dot_active_at_frame(index, frame))
        return 0;
    if (changed_only)
        return (flags[index] & 0x7000) != 0;
    return (flags[index] & skip) == 0;
}
static int pc_extend_axis(FILE *f, PcAxisMap *m, char axis, int offset, size_t frame, const uint16_t *flags, int changed_only, uint16_t skip) {
    uint8_t processed[MAX_DOTS] = {0};
    size_t pair_start = m->count;
    for (size_t i = 0; i < g_shape.dot_count; i++) {
        if (processed[i] || !pc_axis_eligible(i, frame, flags, changed_only, skip))
            continue;
        int value = pc_dot_component_at(i, frame, axis - 'X');
        if (!value) {
            processed[i] = 1;
            continue;
        }
        if (pc_axis_find(m, value) >= 0 || pc_axis_find(m, -value) >= 0)
            continue;
        for (size_t j = i + 1; j < g_shape.dot_count; j++)
            if (!processed[j] && pc_axis_eligible(j, frame, flags, changed_only, skip) && pc_dot_component_at(j, frame, axis - 'X') == -value) {
                m->value[m->count++] = value;
                m->value[m->count++] = -value;
                processed[i] = processed[j] = 1;
                break;
            }
    }
    size_t pairs = (m->count - pair_start) / 2;
    if (pairs) {
        fprintf(f, "\n\tDW CMD_COORDS_R%c,%llu\n", axis, (unsigned long long)pairs);
        for (size_t i = pair_start; i < m->count; i += 2) {
            m->offset[i] = offset;
            m->offset[i + 1] = offset + 6;
            fprintf(f, "\tDW\t%d\t; %d , %d\n", m->value[i], offset, offset + 6);
            offset += 12;
        }
    }
    size_t single_start = m->count;
    for (size_t i = 0; i < g_shape.dot_count; i++)
        if (pc_axis_eligible(i, frame, flags, changed_only, skip)) {
            int value = pc_dot_component_at(i, frame, axis - 'X');
            if (value && pc_axis_find(m, value) < 0)
                m->value[m->count++] = value;
        }
    if (m->count > single_start) {
        fprintf(f, "\n\tDW CMD_COORDS_%c,%llu\n", axis, (unsigned long long)(m->count - single_start));
        for (size_t i = single_start; i < m->count; i++) {
            m->offset[i] = offset;
            fprintf(f, "\tDW\t%d\t; %d \n", m->value[i], offset);
            offset += 6;
        }
    }
    return offset;
}
static int pc_axis_offset(const PcAxisMap *m, int value) {
    if (!value)
        return 0;
    int index = pc_axis_find(m, value);
    return index < 0 ? 0 : m->offset[index];
}
static void pc_dot_flags(uint16_t flags[MAX_DOTS]) {
    memset(flags, 0, MAX_DOTS * sizeof(*flags));
    size_t frames = g_shape.frame_count ? g_shape.frame_count : 1;
    for (size_t i = 0; i < g_shape.dot_count; i++)
        if (dot_active_at_frame(i, 0)) {
            flags[i] = 1;
            Dot base = display_dot_at_frame(i, 0);
            for (size_t frame = 1; frame < frames; frame++) {
                Dot d = display_dot_at_frame(i, frame);
                if (d.x != base.x)
                    flags[i] |= 0x1000;
                if (d.y != base.y)
                    flags[i] |= 0x2000;
                if (d.z != base.z)
                    flags[i] |= 0x4000;
            }
        }
    for (size_t i = 0; i + 1 < g_shape.dot_count; i++) {
        if (!dot_active_at_frame(i, 0) || !dot_active_at_frame(i + 1, 0))
            continue;
        Dot a = display_dot_at_frame(i, 0), b = display_dot_at_frame(i + 1, 0);
        if (a.x == -b.x && a.y == b.y && a.z == b.z) {
            flags[i + 1] |= 0x0200;
            i++;
        }
    }
}
static int pc_component_index(const PcComponents *c, int value) {
    for (size_t i = 0; i < c->count; i++)
        if (c->value[i] == value)
            return (int)i;
    return -1;
}
static void pc_build_value_components(PcComponents *c, const int *values, const uint8_t *eligible, size_t count) {
    memset(c, 0, sizeof(*c));
    uint8_t processed[MAX_POLYS] = {0};
    for (size_t i = 0; i < count; i++) {
        if (!eligible[i] || processed[i])
            continue;
        int value = values[i];
        if (!value) {
            processed[i] = 1;
            continue;
        }
        if (pc_component_index(c, value) >= 0 || pc_component_index(c, -value) >= 0)
            continue;
        for (size_t j = i + 1; j < count; j++)
            if (eligible[j] && !processed[j] && values[j] == -value) {
                c->value[c->count++] = value;
                c->value[c->count++] = -value;
                processed[i] = processed[j] = 1;
                break;
            }
    }
    c->paired = c->count;
    for (size_t i = 0; i < count; i++)
        if (eligible[i] && values[i] && pc_component_index(c, values[i]) < 0)
            c->value[c->count++] = values[i];
}
static int pc_write_i_components(FILE *f, const PcComponents *c, char axis, int offset) {
    if (c->paired) {
        fprintf(f, "\n\tDW CMD_ICOORDS_R%c,%llu\n", axis, (unsigned long long)(c->paired / 2));
        for (size_t i = 0; i < c->paired; i += 2) {
            fprintf(f, "\tDW\t%d\t; %d , %d\n", c->value[i], offset, offset + 2);
            offset += 4;
        }
    }
    if (c->count > c->paired) {
        fprintf(f, "\n\tDW CMD_ICOORDS_%c,%llu\n", axis, (unsigned long long)(c->count - c->paired));
        for (size_t i = c->paired; i < c->count; i++) {
            fprintf(f, "\tDW\t%d\t; %d \n", c->value[i], offset);
            offset += 2;
        }
    }
    return offset;
}
static void pc_poly_normal(const Poly *p, double *n, int scaled[3]) {
    n[0] = n[1] = n[2] = 0;
    if (p->count >= 3) {
        Dot a = display_dot(p->index[0]), b = display_dot(p->index[1]), c = display_dot(p->index[2]);
        a.y = -a.y;
        b.y = -b.y;
        c.y = -c.y;
        double ux = b.x - a.x, uy = b.y - a.y, uz = b.z - a.z, vx = c.x - a.x, vy = c.y - a.y, vz = c.z - a.z;
        n[0] = uy * vz - uz * vy;
        n[1] = uz * vx - ux * vz;
        n[2] = ux * vy - uy * vx;
        double length = sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
        if (length) {
            n[0] /= length;
            n[1] /= length;
            n[2] /= length;
        }
    }
    for (int axis = 0; axis < 3; axis++)
        scaled[axis] = (int)(n[axis] * 32767.0);
}
static int pc_i_component_offset(const PcComponents *c, int base, int value) {
    if (!value)
        return 0;
    int index = pc_component_index(c, value);
    return index < 0 ? 0 : base + index * 2;
}
static size_t pc_frame_slots(size_t frame) {
    size_t slots = 0;
    while (slots < g_shape.dot_count && dot_active_at_frame(slots, frame))
        slots++;
    return slots;
}
static int pc_reflected_pair(const uint16_t *flags, size_t index, size_t slots) { return index + 1 < slots && (flags[index] & 1) && (flags[index + 1] & 1) && (flags[index + 1] & 0x7200) == 0x0200; }
static void pc_write_vertex(FILE *f, const PcAxisMap maps[3], size_t index, size_t frame, int destination) {
    int raw[3] = {pc_dot_component_at(index, frame, 0), pc_dot_component_at(index, frame, 1), pc_dot_component_at(index, frame, 2)};
    fprintf(f, "\tDW\t%d,%d,%d\t; (%d,%d,%d), %d\n", pc_axis_offset(&maps[0], raw[0]), pc_axis_offset(&maps[1], raw[1]), pc_axis_offset(&maps[2], raw[2]), raw[0], raw[1], raw[2], destination);
}
static int pc_write_static_vertices(FILE *f, const PcAxisMap maps[3], size_t slots, int vertex_base, const uint16_t *flags, int vertex_offsets[MAX_DOTS]) {
    uint8_t paired[MAX_DOTS] = {0};
    size_t ordinary = 0, pairs = 0;
    int destination = vertex_base;
    for (size_t i = 0; i < MAX_DOTS; i++)
        vertex_offsets[i] = -1;
    for (size_t i = 0; i < slots; i++)
        if (pc_reflected_pair(flags, i, slots)) {
            paired[i] = paired[i + 1] = 1;
            pairs++;
            i++;
        }
    for (size_t i = 0; i < slots; i++)
        if ((flags[i] & 1) && !(flags[i] & 0x7000) && !paired[i])
            ordinary++;
    if (ordinary) {
        fputs("\n\tDW CMD_VERTICES\n", f);
        for (size_t i = 0; i < slots; i++)
            if ((flags[i] & 1) && !(flags[i] & 0x7000) && !paired[i]) {
                vertex_offsets[i] = destination;
                pc_write_vertex(f, maps, i, 0, destination);
                destination += 6;
            }
        fputs("\tDW\tnil\n", f);
    }
    if (pairs) {
        fputs("\n\tDW CMD_VERTICES_RX\n", f);
        for (size_t i = 0; i < slots; i++)
            if (pc_reflected_pair(flags, i, slots)) {
                int a[3] = {pc_dot_component_at(i, 0, 0), pc_dot_component_at(i, 0, 1), pc_dot_component_at(i, 0, 2)};
                int b[3] = {pc_dot_component_at(i + 1, 0, 0), pc_dot_component_at(i + 1, 0, 1), pc_dot_component_at(i + 1, 0, 2)};
                vertex_offsets[i] = destination;
                vertex_offsets[i + 1] = destination + 6;
                fprintf(f, "\tDW\t%d,%d,%d\t; (%d,%d,%d),(%d,%d,%d), %d\n", pc_axis_offset(&maps[0], a[0]), pc_axis_offset(&maps[1], a[1]), pc_axis_offset(&maps[2], a[2]), a[0], a[1], a[2], b[0], b[1], b[2], destination);
                destination += 12;
                i++;
            }
        fputs("\tDW\tnil\n", f);
    }
    for (size_t i = 0; i < slots; i++)
        if ((flags[i] & 1) && (flags[i] & 0x7000)) {
            vertex_offsets[i] = destination;
            destination += 6;
        }
    return destination;
}
static void pc_write_changed_vertices(FILE *f, const PcAxisMap maps[3], size_t frame, size_t slots, const uint16_t *flags, const int vertex_offsets[MAX_DOTS]) {
    fputs("\n\tDW CMD_VERTICES\n", f);
    for (size_t i = 0; i < slots; i++)
        if ((flags[i] & 0x7000) && dot_active_at_frame(i, frame))
            pc_write_vertex(f, maps, i, frame, vertex_offsets[i]);
    fputs("\tDW\tnil\n", f);
}
static int pc_vertex_offset(const int vertex_offsets[MAX_DOTS], unsigned index) { return index < MAX_DOTS && vertex_offsets[index] >= 0 ? vertex_offsets[index] : 0; }
static int pc_same_visibility_plane(size_t a, size_t b, int *opposite) {
    double pa[4], pb[4];
    if (!polygon_plane(a, pa) || !polygon_plane(b, pb))
        return 0;
    double facing = pa[0] * pb[0] + pa[1] * pb[1] + pa[2] * pb[2];
    if (fabs(fabs(facing) - 1.0) > 1e-8)
        return 0;
    *opposite = facing < 0.0;
    double aligned_d = *opposite ? -pb[3] : pb[3];
    return fabs(pa[3] - aligned_d) <= 0.0001;
}
static void pc_write_primitive(FILE *f, const Poly *p, size_t index, const int vertex_offsets[MAX_DOTS], const int *intensity_offsets, const int *visibility_offsets) {
    if (!p->flags || (p->type & 0x10) || !(p->type & 1))
        return;
    if (p->count == 2) {
        fprintf(f, "\tDW\tCMD_LINE_FV,%u", p->colour);
        for (unsigned j = 0; j < p->count; j++)
            fprintf(f, ",%d", pc_vertex_offset(vertex_offsets, p->index[j]));
        fputc('\n', f);
        return;
    }
    unsigned colour = p->colour < 0x200 ? p->colour + 0x200 : p->colour;
    unsigned mode = p->type & 0x0e;
    int vizi = visibility_offsets[index], intensity = intensity_offsets[index];
    switch (mode) {
    case 0:
        fprintf(f, "\tDW\tCMD_POLYGON_FV,%u,%u", colour, p->count);
        break;
    case 2:
        fprintf(f, "\tDW\tCMD_POLYGON_IV,%u,%d,%u", colour, intensity, p->count);
        break;
    case 4:
        fprintf(f, "\tDW\tCMD_POLYGON_F,%d,%u,%u", vizi, colour, p->count);
        break;
    case 6:
        fprintf(f, "\tDW\tCMD_POLYGON_I,%d,%u,%d,%u", vizi, colour, intensity, p->count);
        break;
    case 8:
        fprintf(f, "\tDW\tCMD_POLYGON_FVZ,%u,%u", colour, p->count);
        break;
    case 10:
        fprintf(f, "\tDW\tCMD_POLYGON_IVZ,%u,%d,%u", colour, intensity, p->count);
        break;
    case 12:
        fprintf(f, "\tDW\tCMD_POLYGON_FZ,%d,%u,%u", vizi, colour, p->count);
        break;
    default:
        fprintf(f, "\tDW\tCMD_POLYGON_IZ,%d,%u,%d,%u", vizi, colour, intensity, p->count);
        break;
    }
    for (unsigned j = 0; j < p->count; j++)
        fprintf(f, ",%d", pc_vertex_offset(vertex_offsets, p->index[j]));
    fputc('\n', f);
}
static void pc_write_bsp_commands(FILE *f, const char *name, int node, int *number, const int visibility_offsets[MAX_POLYS]) {
    if (node < 0 || node >= g_bsp_count)
        return;
    BSPNode *n = &g_bsp_nodes[node];
    int face = *number;
    if (n->front < 0 && n->back < 0) {
        fprintf(f, "\tDW CMD_JUMP,%s_f%d\n", name, face);
        (*number)++;
        return;
    }
    int branch = face + 1;
    fprintf(f, "\tDW CMD_BSP_NODE,%d,%s_end,%s_bsp%d,%s_f%d\n", visibility_offsets[n->poly], name, name, branch, name, face);
    (*number)++;
    if (n->back >= 0) {
        (*number)++;
        pc_write_bsp_commands(f, name, n->back, number, visibility_offsets);
    } else
        fputs("\tDW CMD_QUIT\n", f);
    if (n->front >= 0) {
        fprintf(f, "%s_bsp%d\t label word\n", name, branch);
        pc_write_bsp_commands(f, name, n->front, number, visibility_offsets);
    } else
        fprintf(f, "%s_bsp%d\t label word\n\tDW CMD_QUIT\n", name, branch);
}
static void pc_write_bsp_primitives(FILE *f, const char *name, int node, int *number, const int vertex_offsets[MAX_DOTS], const int *intensity_offsets, const int *visibility_offsets) {
    if (node < 0 || node >= g_bsp_count)
        return;
    BSPNode *n = &g_bsp_nodes[node];
    pc_write_primitive(f, &g_shape.polys[n->poly], (size_t)n->poly, vertex_offsets, intensity_offsets, visibility_offsets);
    for (int poly = g_bsp_coplanar_head[node]; poly >= 0; poly = g_bsp_coplanar_next[poly])
        pc_write_primitive(f, &g_shape.polys[poly], (size_t)poly, vertex_offsets, intensity_offsets, visibility_offsets);
    if (n->front < 0 && n->back < 0)
        return;
    if (n->back >= 0)
        (*number)++;
    if (n->back >= 0) {
        int face = (*number)++;
        fprintf(f, "\tDW CMD_QUIT\n%s_f%d\tlabel word\n", name, face);
        pc_write_bsp_primitives(f, name, n->back, number, vertex_offsets, intensity_offsets, visibility_offsets);
    }
    if (n->front >= 0) {
        int face = (*number)++;
        fprintf(f, "\tDW CMD_QUIT\n%s_f%d\tlabel word\n", name, face);
        pc_write_bsp_primitives(f, name, n->front, number, vertex_offsets, intensity_offsets, visibility_offsets);
    }
}
static void pc_write_flat_primitives(FILE *f, int node, const int vertex_offsets[MAX_DOTS], const int *intensity_offsets, const int *visibility_offsets) {
    for (; node >= 0 && node < g_bsp_count; node = g_bsp_nodes[node].front) {
        BSPNode *n = &g_bsp_nodes[node];
        pc_write_primitive(f, &g_shape.polys[n->poly], (size_t)n->poly, vertex_offsets, intensity_offsets, visibility_offsets);
        for (int poly = g_bsp_coplanar_head[node]; poly >= 0; poly = g_bsp_coplanar_next[poly])
            pc_write_primitive(f, &g_shape.polys[poly], (size_t)poly, vertex_offsets, intensity_offsets, visibility_offsets);
        if (!n->leaf)
            break;
    }
}
static int save_pc_asm_body(const wchar_t *path) {
    FILE *f = _wfopen(path, L"wb");
    if (!f)
        return 0;
    char name[64];
    asm_name(path, name);
    size_t frames = g_shape.frame_count ? g_shape.frame_count : 1, slots = 0;
    uint16_t dot_flags[MAX_DOTS];
    pc_dot_flags(dot_flags);
    PcAxisMap base_maps[3] = {0};
    PcAxisMap(*frame_maps)[3] = (PcAxisMap(*)[3])calloc(frames, sizeof(*frame_maps));
    if (!frame_maps) {
        fclose(f);
        return 0;
    }
    fprintf(f, "%s label word\n", name);
    int offset = 8;
    offset = pc_extend_axis(f, &base_maps[0], 'X', offset, 0, dot_flags, 0, 0x1200);
    offset = pc_extend_axis(f, &base_maps[1], 'Y', offset, 0, dot_flags, 0, 0x2000);
    offset = pc_extend_axis(f, &base_maps[2], 'Z', offset, 0, dot_flags, 0, 0x4000);
    int base_end = offset, max_coeff = offset;
    if (frames > 1) {
        fprintf(f, "\n\n\tDW CMD_SWITCH,obj_anim,%llu\n", (unsigned long long)(frames * 2));
        for (size_t frame = 0; frame < frames; frame++)
            fprintf(f, "\tDW\t%s_c%llu\n", name, (unsigned long long)frame);
        for (size_t frame = 0; frame < frames; frame++) {
            memcpy(frame_maps[frame], base_maps, sizeof(base_maps));
            fprintf(f, "%s_c%llu\tlabel word\n", name, (unsigned long long)frame);
            int branch = base_end;
            branch = pc_extend_axis(f, &frame_maps[frame][0], 'X', branch, frame, dot_flags, 1, 0);
            branch = pc_extend_axis(f, &frame_maps[frame][1], 'Y', branch, frame, dot_flags, 1, 0);
            branch = pc_extend_axis(f, &frame_maps[frame][2], 'Z', branch, frame, dot_flags, 1, 0);
            fprintf(f, "\tDW CMD_BLANK,MAX_COE_OF-%d\n", branch);
            if (frame + 1 < frames)
                fprintf(f, "\tDW CMD_JUMP,%s_c_end\n", name);
            else
                fprintf(f, "%s_c_end\tlabel word\n\n", name);
            if (branch > max_coeff)
                max_coeff = branch;
        }
    } else
        memcpy(frame_maps[0], base_maps, sizeof(base_maps));
    fprintf(f, "\n\nMAX_COE_OF\tEQU\t%d\n", max_coeff);
    int vertex_base = max_coeff;
    for (size_t frame = 0; frame < frames; frame++) {
        size_t frame_slots = pc_frame_slots(frame);
        if (frame_slots > slots)
            slots = frame_slots;
    }
    int vertex_offsets[MAX_DOTS];
    offset = pc_write_static_vertices(f, frame_maps[0], slots, vertex_base, dot_flags, vertex_offsets);
    if (frames > 1) {
        fprintf(f, "\n\n\tDW CMD_SWITCH,obj_anim,%llu\n", (unsigned long long)(frames * 2));
        for (size_t frame = 0; frame < frames; frame++)
            fprintf(f, "\tDW\t%s_v%llu\n", name, (unsigned long long)frame);
        for (size_t frame = 0; frame < frames; frame++) {
            fprintf(f, "%s_v%llu\tlabel word\n", name, (unsigned long long)frame);
            pc_write_changed_vertices(f, frame_maps[frame], frame, pc_frame_slots(frame), dot_flags, vertex_offsets);
            if (frame + 1 < frames)
                fprintf(f, "\tDW CMD_JUMP,%s_v_end\n", name);
        }
        fprintf(f, "%s_v_end\tlabel word\n\n", name);
    }
    int normal_values[3][MAX_POLYS] = {{0}}, intensity_offsets[MAX_POLYS] = {0}, visibility_offsets[MAX_POLYS] = {0};
    double normals[MAX_POLYS][3] = {{0}};
    uint8_t lit[MAX_POLYS] = {0};
    for (size_t i = 0; i < g_shape.poly_count; i++) {
        Poly *p = &g_shape.polys[i];
        if (p->flags && p->count > 2 && (p->type & 2)) {
            lit[i] = 1;
            int values[3];
            pc_poly_normal(p, normals[i], values);
            for (int axis = 0; axis < 3; axis++)
                normal_values[axis][i] = values[axis];
        }
    }
    PcComponents ix, iy, iz;
    pc_build_value_components(&ix, normal_values[0], lit, g_shape.poly_count);
    pc_build_value_components(&iy, normal_values[1], lit, g_shape.poly_count);
    pc_build_value_components(&iz, normal_values[2], lit, g_shape.poly_count);
    int ix_base = offset;
    offset = pc_write_i_components(f, &ix, 'X', offset);
    int iy_base = offset;
    offset = pc_write_i_components(f, &iy, 'Y', offset);
    int iz_base = offset;
    offset = pc_write_i_components(f, &iz, 'Z', offset);
    fputs("\n\tDW CMD_INTENSITIES\n", f);
    for (size_t i = 0; i < g_shape.poly_count; i++)
        if (lit[i]) {
            int duplicate = -1;
            for (size_t j = 0; j < i; j++)
                if (lit[j] && normal_values[0][i] == normal_values[0][j] && normal_values[1][i] == normal_values[1][j] && normal_values[2][i] == normal_values[2][j]) {
                    duplicate = (int)j;
                    break;
                }
            if (duplicate >= 0) {
                intensity_offsets[i] = intensity_offsets[duplicate];
                continue;
            }
            intensity_offsets[i] = offset;
            for (size_t j = i + 1; j < g_shape.poly_count; j++)
                if (lit[j] && normal_values[0][i] == normal_values[0][j] && normal_values[1][i] == normal_values[1][j] && normal_values[2][i] == normal_values[2][j])
                    intensity_offsets[j] = offset;
            fprintf(f, "\tDW\t%d,%d,%d\t; (%.0f,%.0f,%.0f), %d\n", pc_i_component_offset(&ix, ix_base, normal_values[0][i]), pc_i_component_offset(&iy, iy_base, normal_values[1][i]), pc_i_component_offset(&iz, iz_base, normal_values[2][i]), normals[i][0] * 32767.0, normals[i][1] * 32767.0, normals[i][2] * 32767.0, offset);
            offset += 2;
        }
    fputs("\tDW\tnil\n\n\tDW CMD_VISIBILITIES\n", f);
    size_t visibility_representative[MAX_POLYS], visibility_count = 0;
    for (size_t i = 0; i < g_shape.poly_count; i++) {
        Poly *p = &g_shape.polys[i];
        if (!p->flags || p->count <= 2)
            continue;
        int shared = 0;
        for (size_t q = 0; q < visibility_count; q++) {
            int opposite = 0;
            size_t representative = visibility_representative[q];
            if (pc_same_visibility_plane(representative, i, &opposite)) {
                visibility_offsets[i] = visibility_offsets[representative] + opposite;
                shared = 1;
                break;
            }
        }
        if (shared)
            continue;
        visibility_representative[visibility_count++] = i;
        visibility_offsets[i] = offset;
        fprintf(f, "\tDW\t%d,%d,%d\t; %d\n", pc_vertex_offset(vertex_offsets, p->index[0]), pc_vertex_offset(vertex_offsets, p->index[1]), pc_vertex_offset(vertex_offsets, p->index[2]), offset);
        offset += 2;
    }
    fputs("\tDW\tnil\n\n", f);
    if (!g_bsp_valid)
        build_bsp();
    if (g_bsp_root >= 0 && g_bsp_flat) {
        fprintf(f, "\tDW CMD_JUMP,%s_f1\n\n%s_f1\tlabel word\n", name, name);
        for (size_t i = 0; i < g_shape.poly_count; i++)
            if (g_shape.polys[i].flags && g_shape.polys[i].count == 2)
                pc_write_primitive(f, &g_shape.polys[i], i, vertex_offsets, intensity_offsets, visibility_offsets);
        pc_write_flat_primitives(f, g_bsp_root, vertex_offsets, intensity_offsets, visibility_offsets);
    } else if (g_bsp_root >= 0) {
        int number = 1;
        pc_write_bsp_commands(f, name, g_bsp_root, &number, visibility_offsets);
        fprintf(f, "\n%s_f1\tlabel word\n", name);
        number = 2;
        for (size_t i = 0; i < g_shape.poly_count; i++)
            if (g_shape.polys[i].flags && g_shape.polys[i].count == 2)
                pc_write_primitive(f, &g_shape.polys[i], i, vertex_offsets, intensity_offsets, visibility_offsets);
        pc_write_bsp_primitives(f, name, g_bsp_root, &number, vertex_offsets, intensity_offsets, visibility_offsets);
    } else
        for (size_t i = 0; i < g_shape.poly_count; i++)
            pc_write_primitive(f, &g_shape.polys[i], i, vertex_offsets, intensity_offsets, visibility_offsets);
    fprintf(f, "\tDW CMD_QUIT\n%s_end\tlabel\tword\n\tdw CMD_QUIT\n", name);
    free(frame_maps);
    fclose(f);
    return 1;
}
static int save_pc_asm(const wchar_t *path) {
    Shape saved, working = {0};
    if (!begin_export_bsp(&saved, &working))
        return 0;
    int ok = save_pc_asm_body(path);
    end_export_bsp(&saved, &working);
    return ok;
}
static void save_assembler(int kind) {
    wchar_t p[MAX_PATH] = L"";
    const wchar_t *title = kind == 0 ? L"Save Assembler" : kind == 1 ? L"Save Assembler (BSP)"
                                                                     : L"Save PC Assembler";
    if (!choose_file(1, title, p))
        return;
    int ok = kind == 0 ? save_gzs(p) : kind == 1 ? save_bsp_asm(p)
                                                 : save_pc_asm(p);
    if (ok)
        statusf(L"Saved assembler output");
    else
        statusf(L"Could not save assembler output");
}
static void snes_entry(SNESEntry *entry, const char *name, int value) {
    size_t length = strlen(name);
    if (length >= sizeof(entry->name))
        length = sizeof(entry->name) - 1;
    memcpy(entry->name, name, length);
    entry->name[length] = 0;
    MultiByteToWideChar(CP_ACP, 0, entry->name, -1, entry->label, (int)(sizeof(entry->label) / sizeof(entry->label[0])));
    entry->label[49] = 0;
    entry->value = value;
}
static int load_colour_tables_path(const wchar_t *path) {
    FILE *f = _wfopen(path, L"rb");
    if (!f)
        return 0;
    g_coltab_count = g_palette_count = g_texture_count = 0;
    wchar_t full[MAX_PATH], *leaf = NULL;
    if (GetFullPathNameW(path, MAX_PATH, full, &leaf) && leaf) {
        while (leaf > full && (leaf[-1] == L'\\' || leaf[-1] == L'/'))
            leaf--;
        *leaf = 0;
        wcsncpy(g_snes_data_dir, full, MAX_PATH - 1);
        g_snes_data_dir[MAX_PATH - 1] = 0;
    }
    char kind[32], name[50];
    int value;
    while (fscanf(f, "%31s %49s %d", kind, name, &value) == 3) {
        if (!strcmp(kind, "COLTAB") && g_coltab_count < MAX_SNES_ENTRIES) {
            snes_entry(&g_coltab_entries[g_coltab_count], name, value);
            g_coltab_labels[g_coltab_count] = g_coltab_entries[g_coltab_count].label;
            g_coltab_count++;
        } else if (!strcmp(kind, "TEXMAP") && g_texture_count < MAX_SNES_ENTRIES) {
            snes_entry(&g_texture_entries[g_texture_count], name, value);
            g_texture_labels[g_texture_count] = g_texture_entries[g_texture_count].label;
            g_texture_count++;
        } else if (!strcmp(kind, "COLOUR") && g_palette_count < MAX_SNES_ENTRIES) {
            snes_entry(&g_palette_entries[g_palette_count], name, value);
            g_palette_labels[g_palette_count] = g_palette_entries[g_palette_count].label;
            g_palette_count++;
        }
    }
    fclose(f);
    if (g_coltab_index >= g_coltab_count)
        g_coltab_index = 0;
    if (g_palette_index >= g_palette_count)
        g_palette_index = -1;
    if (g_texture_index >= g_texture_count)
        g_texture_index = -1;
    g_smooth_shade = g_coltab_count && g_coltab_entries[g_coltab_index].value < 0;
    return 1;
}
static int ensure_colour_tables(void) {
    if (g_coltab_count || g_palette_count || g_texture_count)
        return 1;
    wchar_t path[MAX_PATH];
    _snwprintf(path, MAX_PATH, L"%ls\\COLTABS.DAT", g_snes_data_dir);
    path[MAX_PATH - 1] = 0;
    if (load_colour_tables_path(path))
        return 1;
    statusf(L"File 'SENDCOL.DAT'?");
    return 0;
}
static void snes_data_path(wchar_t path[MAX_PATH], const char *name, const wchar_t *extension) {
    wchar_t wide[50];
    MultiByteToWideChar(CP_ACP, 0, name, -1, wide, 50);
    _snwprintf(path, MAX_PATH, L"%ls\\%ls%ls", g_snes_data_dir, wide, extension);
    path[MAX_PATH - 1] = 0;
}
static void load_snes_palette(int index) {
    if (index < 0 || index >= g_palette_count)
        return;
    wchar_t path[MAX_PATH];
    snes_data_path(path, g_palette_entries[index].name, L".col");
    FILE *f = _wfopen(path, L"rb");
    if (!f) {
        statusf(L"File Error");
        return;
    }
    size_t got = fread(g_snes_palette, sizeof(uint16_t), 256, f);
    fclose(f);
    if (got != 256) {
        statusf(L"File Error");
        return;
    }
    g_palette_loaded = 1;
    g_palette_index = index;
    statusf(L"Palette %ls -> 0x5E00 (512 bytes)", g_palette_entries[index].label);
}
static void choose_texture(int index) {
    if (index < 0 || index >= g_texture_count)
        return;
    wchar_t path[MAX_PATH];
    snes_data_path(path, g_texture_entries[index].name, L".dat");
    FILE *f = _wfopen(path, L"rb");
    if (!f) {
        statusf(L"File Error");
        return;
    }
    fseek(f, 0, SEEK_END);
    long bytes = ftell(f);
    fclose(f);
    wcscpy(g_texture_path, path);
    g_texture_index = index;
    statusf(L"Texture %ls -> 0x8000 (%ld bytes)", g_texture_entries[index].label, bytes);
}
static void build_colour_table_source(int index) {
    if (index < 0 || index >= g_coltab_count)
        return;
    g_coltab_index = index;
    g_smooth_shade = g_coltab_entries[index].value < 0;
    wchar_t grid[MAX_PATH];
    snes_data_path(grid, "GRID", L".DAT");
    FILE *in = _wfopen(grid, L"rb");
    if (in) {
        FILE *out = _wfopen(L"tmpshape.asm", L"wb");
        if (out) {
            char line[200];
            if (fgets(line, sizeof(line), in))
                fputs(line, out);
            if (fgets(line, sizeof(line), in))
                fprintf(out, line, g_coltab_entries[index].name);
            while (fgets(line, sizeof(line), in))
                fputs(line, out);
            fclose(out);
        }
        fclose(in);
    }
    statusf(L"Colour tab => %ls", g_coltab_entries[index].label);
}
static void open_colour_table_menu(void) {
    if (ensure_colour_tables() && g_coltab_count) {
        g_active_menu = MENU_COLTAB;
        InvalidateRect(g_hwnd, NULL, FALSE);
    }
}
static void send_snes_stream(void) {
    wchar_t path[MAX_PATH] = L"tmpShape.asm";
    if (!choose_file(1, L"Save SNES Assembler", path))
        return;
    if (save_bsp_asm(path))
        statusf(L"SNES assembler saved (ARG/DL payload source)");
    else
        statusf(L"File Error");
}

static int load_m3d(FILE *f, Shape *n) {
    unsigned count;
    if (fscanf(f, "%u", &count) != 1 || count > MAX_DOTS)
        return 0;
    n->frame_count = 1;
    n->frames[0] = (FrameDot *)calloc(count, sizeof(FrameDot));
    if (count && !n->frames[0])
        return 0;
    for (unsigned i = 0; i < count; i++) {
        Dot *d = &n->dots[n->dot_count];
        if (fscanf(f, "%lf %lf %lf", &d->x, &d->y, &d->z) != 3)
            return 0;
        d->x = dos_coord(d->x);
        d->y = dos_coord(d->y);
        d->z = dos_coord(d->z);
        n->frames[0][i] = (FrameDot){d->x, d->y, d->z, g_current_group};
        n->dot_count++;
    }
    while (n->poly_count < MAX_POLYS && fscanf(f, "%u", &count) == 1) {
        if (count > MAX_POLY_VERTS)
            return 0;
        Poly *p = &n->polys[n->poly_count];
        p->count = (uint16_t)count;
        for (unsigned j = 0; j < count; j++) {
            unsigned x;
            if (fscanf(f, "%u", &x) != 1 || x >= n->dot_count)
                return 0;
            p->index[j] = (uint16_t)x;
        }
        unsigned colour;
        if (fscanf(f, "%u", &colour) != 1)
            return 0;
        p->colour = (uint16_t)colour;
        p->flags = g_current_group;
        p->type = g_poly_type;
        n->poly_count++;
    }
    return 1;
}

static int load_internal(FILE *f, Shape *n) {
    unsigned dots, frames;
    if (fscanf(f, "%u %u", &dots, &frames) != 2 || dots > MAX_DOTS || !frames || frames > MAX_FRAMES)
        return 0;
    n->dot_count = dots;
    n->frame_count = frames;
    for (unsigned frame = 0; frame < frames; frame++) {
        n->frames[frame] = (FrameDot *)calloc(dots, sizeof(FrameDot));
        if (!n->frames[frame])
            return 0;
        for (unsigned i = 0; i < dots; i++) {
            FrameDot *d = &n->frames[frame][i];
            if (fscanf(f, "%lf %lf %lf,%hu", &d->x, &d->y, &d->z, &d->active) != 4)
                return 0;
            d->x = dos_coord(d->x);
            d->y = dos_coord(d->y);
            d->z = dos_coord(d->z);
            if (frame == 0) {
                n->dots[i].x = d->x;
                n->dots[i].y = d->y;
                n->dots[i].z = d->z;
                n->dots[i].selected = (uint8_t)((d->active & 0x100) != 0);
            }
        }
    }
    unsigned count;
    while (n->poly_count < MAX_POLYS && fscanf(f, "%u", &count) == 1) {
        if (count > MAX_POLY_VERTS)
            return 0;
        Poly *p = &n->polys[n->poly_count];
        p->count = (uint16_t)count;
        for (unsigned j = 0; j < count; j++) {
            unsigned x;
            if (fscanf(f, "%u", &x) != 1 || x >= dots)
                return 0;
            p->index[j] = (uint16_t)x;
        }
        short flags, type;
        if (fscanf(f, " ,%hu %hi %hi", &p->colour, &flags, &type) != 3)
            return 0;
        p->flags = (uint16_t)flags & ~0x0100u;
        p->selected = (uint8_t)(((uint16_t)flags & 0x0100) != 0);
        p->type = (uint16_t)type;
        n->poly_count++;
    }
    /* ReadInt repeatedly chooses the last free fixed polygon slot. Compacting
       those slots later therefore exposes the file records in reverse order. */
    for (size_t i = 0; i < n->poly_count / 2; i++) {
        Poly p = n->polys[i];
        n->polys[i] = n->polys[n->poly_count - 1 - i];
        n->polys[n->poly_count - 1 - i] = p;
    }
    return 1;
}

static int read_packed_poly(FILE *f, Shape *n, unsigned index_base, int reverse, int sams) {
    static const uint16_t sams_types[16] = {7, 5, 7, 5, 3, 1, 3, 1, 15, 13, 15, 13, 11, 9, 11, 9};
    unsigned count;
    if (fscanf(f, "%u", &count) != 1)
        return 0;
    if (count > MAX_POLY_VERTS || n->poly_count >= MAX_POLYS)
        return -1;
    Poly *p = &n->polys[n->poly_count];
    p->count = (uint16_t)count;
    for (unsigned j = 0; j < count; j++) {
        unsigned x;
        if (fscanf(f, "%u", &x) != 1 || x + index_base >= n->dot_count)
            return -1;
        unsigned at = reverse ? count - 1 - j : j;
        p->index[at] = (uint16_t)(x + index_base);
    }
    long packed;
    if (fscanf(f, "%li", &packed) != 1)
        return -1;
    p->flags = g_current_group;
    if (sams) {
        long type_index = packed >> 1;
        p->colour = (uint16_t)packed;
        p->type = type_index >= 0 && type_index < 16 ? sams_types[type_index] : g_poly_type;
    } else {
        p->colour = (uint16_t)(packed & 0xff);
        p->type = (uint16_t)(packed >> 1);
        if (!p->type)
            p->type = g_poly_type;
    }
    n->poly_count++;
    return 1;
}

static int load_anim(FILE *f, Shape *n) {
    unsigned dots, frames;
    if (fscanf(f, "%u %u", &dots, &frames) != 2 || dots > MAX_DOTS || !frames || frames > MAX_FRAMES)
        return 0;
    n->dot_count = dots;
    n->frame_count = frames;
    for (unsigned frame = 0; frame < frames; frame++) {
        n->frames[frame] = (FrameDot *)calloc(dots, sizeof(FrameDot));
        if (!n->frames[frame])
            return 0;
        for (unsigned i = 0; i < dots; i++) {
            FrameDot *d = &n->frames[frame][i];
            if (fscanf(f, "%lf %lf %lf", &d->x, &d->y, &d->z) != 3)
                return 0;
            d->x = dos_coord(d->x);
            d->y = dos_coord(d->y);
            d->z = dos_coord(d->z);
            d->active = g_current_group;
            if (frame == 0) {
                n->dots[i].x = d->x;
                n->dots[i].y = d->y;
                n->dots[i].z = d->z;
            }
        }
    }
    int result;
    while ((result = read_packed_poly(f, n, 0, 0, 0)) > 0) {
    }
    return result == 0;
}

static int load_sams(FILE *f, Shape *n) {
    unsigned fixed, animated, frames;
    if (fscanf(f, " POINTS:%u", &fixed) != 1 || fixed > MAX_DOTS)
        return 0;
    for (unsigned i = 0; i < fixed; i++) {
        Dot *d = &n->dots[i];
        if (fscanf(f, "%lf %lf %lf", &d->x, &d->y, &d->z) != 3)
            return 0;
        d->x = dos_coord(d->x);
        d->y = dos_coord(-d->y);
        d->z = dos_coord(d->z);
    }
    n->dot_count = fixed;
    if (fscanf(f, " ANIM:%u %u", &animated, &frames) != 2 || fixed + animated > MAX_DOTS || !frames || frames > MAX_FRAMES)
        return 0;
    n->dot_count = fixed + animated;
    n->frame_count = frames;
    for (unsigned frame = 0; frame < frames; frame++) {
        n->frames[frame] = (FrameDot *)calloc(n->dot_count, sizeof(FrameDot));
        if (!n->frames[frame])
            return 0;
        for (unsigned i = 0; i < fixed; i++)
            n->frames[frame][i] = (FrameDot){n->dots[i].x, n->dots[i].y, n->dots[i].z, g_current_group};
        unsigned frame_number;
        if (fscanf(f, " FRAME:%u", &frame_number) != 1)
            return 0;
        (void)frame_number;
        for (unsigned i = 0; i < animated; i++) {
            FrameDot *d = &n->frames[frame][fixed + i];
            if (fscanf(f, "%lf %lf %lf", &d->x, &d->y, &d->z) != 3)
                return 0;
            d->x = dos_coord(d->x);
            d->y = dos_coord(-d->y);
            d->z = dos_coord(d->z);
            d->active = g_current_group;
            if (frame == 0) {
                n->dots[fixed + i].x = d->x;
                n->dots[fixed + i].y = d->y;
                n->dots[fixed + i].z = d->z;
            }
        }
    }
    unsigned faces;
    if (fscanf(f, " FACES:%u", &faces) != 1 || faces > MAX_POLYS)
        return 0;
    for (unsigned i = 0; i < faces; i++)
        if (read_packed_poly(f, n, 0, 1, 1) <= 0)
            return 0;
    return 1;
}

static int load_shape(const wchar_t *path) {
    g_last_load_opened = 0;
    FILE *f = _wfopen(path, L"rb");
    if (!f)
        return 0;
    g_last_load_opened = 1;
    wcsncpy(g_path, path, MAX_PATH - 1);
    g_path[MAX_PATH - 1] = 0;
    Shape n = {0};
    char magic[16];
    int ok = 0;
    if (fscanf(f, "%15s", magic) == 1) {
        if (!strcmp(magic, "3DG1"))
            ok = load_m3d(f, &n);
        else if (!strcmp(magic, "3DCG"))
            ok = load_internal(f, &n);
        else if (!strcmp(magic, "3DAN"))
            ok = load_anim(f, &n);
        else if (!strcmp(magic, "3DA1"))
            ok = load_sams(f, &n);
    }
    fclose(f);
    if (!ok) {
        free_frames(&n);
        return 0;
    }
    snapshot();
    free_frames(&g_shape);
    g_shape = n;
    g_current_frame = 0;
    rebuild_dot_selection_order();
    return 1;
}

static void load_key_animation(void) {
    ensure_first_frame();
    if (g_shape.frame_count > 1) {
        statusf(L"Shape already animated");
        return;
    }
    snapshot();
    wchar_t path[MAX_PATH] = L"";
    if (!choose_file(0, L"Load Animation key", path))
        return;
    FILE *f = _wfopen(path, L"rb");
    if (!f) {
        statusf(L"File Error");
        return;
    }
    char magic[16];
    unsigned key_dots = 0, key_frames = 0;
    double key[MAX_FRAMES][16][3] = {0};
    int ok = fscanf(f, "%15s", magic) == 1 && !strcmp(magic, "3DCG") && fscanf(f, "%u %u", &key_dots, &key_frames) == 2;
    if (ok && (key_dots > 16 || !key_frames || key_frames > MAX_FRAMES))
        ok = 0;
    for (unsigned frame = 0; ok && frame < key_frames; frame++)
        for (unsigned i = 0; i < key_dots; i++) {
            double x, y, z;
            unsigned ignored;
            if (fscanf(f, "%lf %lf %lf,%u", &x, &y, &z, &ignored) != 4) {
                ok = 0;
                break;
            }
            key[frame][i][0] = dos_coord(x);
            key[frame][i][1] = dos_coord(y);
            key[frame][i][2] = dos_coord(z);
        }
    fclose(f);
    if (!ok) {
        statusf(key_dots > 16 ? L"Too many dots in key" : L"Not Shaped Anim file!");
        return;
    }
    FrameDot *added[MAX_FRAMES] = {0};
    for (unsigned frame = 1; frame < key_frames; frame++) {
        added[frame] = (FrameDot *)malloc(g_shape.dot_count * sizeof(FrameDot));
        if (!added[frame]) {
            for (unsigned q = 1; q < frame; q++)
                free(added[q]);
            statusf(L"No mem for key");
            return;
        }
        memcpy(added[frame], g_shape.frames[0], g_shape.dot_count * sizeof(FrameDot));
    }
    for (unsigned frame = 1; frame < key_frames; frame++)
        g_shape.frames[frame] = added[frame];
    g_shape.frame_count = key_frames;
    g_current_frame = 0;
    for (size_t i = 0; i < g_shape.dot_count; i++)
        if (g_shape.frames[0][i].active & 0x0100) {
            double weight[16] = {0}, total = 0;
            for (unsigned k = 0; k < key_dots; k++) {
                double dx = g_shape.frames[0][i].x - key[0][k][0], dy = g_shape.frames[0][i].y - key[0][k][1], dz = g_shape.frames[0][i].z - key[0][k][2], distance2 = dx * dx + dy * dy + dz * dz;
                weight[k] = 1.0 / distance2;
                total += weight[k];
            }
            for (unsigned k = 0; k < key_dots; k++)
                weight[k] /= total;
            for (unsigned frame = 1; frame < key_frames; frame++) {
                double movement[3] = {0};
                for (unsigned k = 0; k < key_dots; k++)
                    for (int axis = 0; axis < 3; axis++)
                        movement[axis] += (key[frame][k][axis] - key[frame - 1][k][axis]) * weight[k];
                g_shape.frames[frame][i].x = dos_coord(g_shape.frames[frame - 1][i].x + movement[0]);
                g_shape.frames[frame][i].y = dos_coord(g_shape.frames[frame - 1][i].y + movement[1]);
                g_shape.frames[frame][i].z = dos_coord(g_shape.frames[frame - 1][i].z + movement[2]);
            }
        }
    statusf(L"Key Loaded! %zu Frames", g_shape.frame_count);
}

static void delete_selected(void) {
    if (!g_select_count)
        return;
    snapshot();
    ensure_first_frame();
    if (!g_shape.frame_count) {
        statusf(L"Out of memory");
        return;
    }
    for (size_t i = 0; i < g_shape.dot_count; i++)
        if (g_shape.dots[i].selected) {
            for (size_t f = 0; f < g_shape.frame_count; f++)
                g_shape.frames[f][i].active = 0;
            for (size_t p = 0; p < g_shape.poly_count; p++)
                if (g_shape.polys[p].flags)
                    for (unsigned j = 0; j < g_shape.polys[p].count; j++)
                        if (g_shape.polys[p].index[j] == i) {
                            g_shape.polys[p].flags = 0;
                            g_shape.polys[p].selected = 0;
                            break;
                        }
        }
    clear_dot_selection(0);
    g_bsp_valid = 0;
    statusf(L" Selected %zu", g_select_count);
}

static void add_polygon_from_selected(void) {
    size_t slot = g_shape.poly_count;
    for (size_t i = 0; i < g_shape.poly_count; i++)
        if (!g_shape.polys[i].flags) {
            slot = i;
            break;
        }
    if (slot == g_shape.poly_count && slot >= MAX_POLYS) {
        statusf(L"Polygon limit reached");
        return;
    }
    Poly p = {0};
    for (size_t i = 0; i < g_select_count && p.count < MAX_POLY_VERTS; i++) {
        size_t index = g_select_order[i];
        if (index < g_shape.dot_count && g_shape.dots[index].selected)
            p.index[p.count++] = (uint16_t)index;
    }
    if (p.count < 1) {
        statusf(L"Select one or more dots first");
        return;
    }
    snapshot();
    p.colour = g_poly_colour;
    p.type = g_poly_type;
    p.flags = g_current_group;
    p.selected = 1;
    g_shape.polys[slot] = p;
    if (slot == g_shape.poly_count)
        g_shape.poly_count++;
    statusf(L"Created %u vertex polygon", p.count);
}
static void delete_selected_polys(void) {
    snapshot();
    size_t n = 0;
    for (size_t i = 0; i < g_shape.poly_count; i++)
        if (g_shape.polys[i].flags && g_shape.polys[i].selected) {
            g_shape.polys[i].flags = 0;
            g_shape.polys[i].selected = 0;
            n++;
        }
    g_bsp_valid = 0;
    statusf(L"Deleted %zu polygons", n);
}
static void select_poly_step(int delta) {
    if (!g_shape.poly_count)
        return;
    size_t selected = (size_t)-1;
    if (delta > 0) {
        for (size_t i = 0; i < g_shape.poly_count; i++)
            if (g_shape.polys[i].selected) {
                selected = i;
                break;
            }
        if (selected != (size_t)-1) {
            g_shape.polys[selected].selected = 0;
            for (size_t i = selected + 1; i < g_shape.poly_count; i++)
                if (g_shape.polys[i].flags & g_shape_display_mask) {
                    g_shape.polys[i].selected = 1;
                    statusf(L"Polygon %zu", i);
                    return;
                }
        }
        for (size_t i = 0; i < g_shape.poly_count; i++)
            if ((g_shape.polys[i].flags & g_shape_display_mask) && !g_shape.polys[i].selected) {
                g_shape.polys[i].selected = 1;
                statusf(L"Polygon %zu", i);
                return;
            }
    } else {
        for (size_t i = g_shape.poly_count; i > 0; i--)
            if (g_shape.polys[i - 1].selected) {
                selected = i - 1;
                break;
            }
        if (selected != (size_t)-1) {
            g_shape.polys[selected].selected = 0;
            for (size_t i = selected; i > 0; i--)
                if (g_shape.polys[i - 1].flags & g_shape_display_mask) {
                    g_shape.polys[i - 1].selected = 1;
                    statusf(L"Polygon %zu", i - 1);
                    return;
                }
        }
        for (size_t i = g_shape.poly_count; i > 0; i--)
            if ((g_shape.polys[i - 1].flags & g_shape_display_mask) && !g_shape.polys[i - 1].selected) {
                g_shape.polys[i - 1].selected = 1;
                statusf(L"Polygon %zu", i - 1);
                return;
            }
    }
    statusf(L"No displayed polygon");
}
static void select_best_polygon_from_dots(int select_on) {
    if (!g_select_count) {
        statusf(L"Select dots first");
        return;
    }
    snapshot();
    size_t best = (size_t)-1, best_score = 0;
    for (size_t i = 0; i < g_shape.poly_count; i++) {
        Poly *p = &g_shape.polys[i];
        if (!p->flags)
            continue;
        size_t score = 0;
        for (unsigned j = 0; j < p->count; j++)
            if (p->index[j] < g_shape.dot_count && g_shape.dots[p->index[j]].selected)
                score++;
        if (score > best_score) {
            best_score = score;
            best = i;
        }
    }
    if (best == (size_t)-1) {
        statusf(L"No polygon uses selected dots");
        return;
    }
    g_shape.polys[best].selected = (uint8_t)select_on;
    statusf(select_on ? L"Selected polygon %zu (%zu matching dots)" : L"Deselected polygon %zu (%zu matching dots)", best, best_score);
}
static void auto_zoom(void) {
    int have = 0;
    double low[3] = {0, 0, 0}, high[3] = {0, 0, 0};
    for (size_t i = 0; i < g_shape.dot_count; i++)
        if (dot_active(i)) {
            Dot d = display_dot(i);
            double value[3] = {d.x, d.y, d.z};
            for (int a = 0; a < 3; a++) {
                if (value[a] > high[a])
                    high[a] = value[a];
                if (value[a] < low[a])
                    low[a] = value[a];
            }
            have = 1;
        }
    if (!have) {
        g_zoom = 1.0;
        statusf(L"Auto zoom %.3g", g_zoom);
        return;
    }
    double range = high[0] - low[0];
    for (int a = 1; a < 3; a++)
        if (high[a] - low[a] > range)
            range = high[a] - low[a];
    g_zoom = range > 0.0 ? 238.0 / range : 1.0;
    for (int a = 0; a < 3; a++)
        g_origin[a] = trunc((high[a] + low[a]) / 2.0);
    statusf(L"Auto zoom %.3g", g_zoom);
}
static FrameDot *make_display_frame(void) {
    FrameDot *n = (FrameDot *)calloc(g_shape.dot_count, sizeof(FrameDot));
    if (!n)
        return NULL;
    for (size_t i = 0; i < g_shape.dot_count; i++) {
        Dot d = display_dot(i);
        n[i] = (FrameDot){d.x, d.y, d.z, (uint16_t)(dot_active(i) ? 1 | (g_shape.dots[i].selected ? 0x0100 : 0) : 0)};
    }
    return n;
}
static void ensure_first_frame(void) {
    if (g_shape.frame_count)
        return;
    FrameDot *n = make_display_frame();
    if (n) {
        g_shape.frames[0] = n;
        g_shape.frame_count = 1;
        g_current_frame = 0;
    }
}
static void add_frames_count(size_t count) {
    if (!count)
        return;
    if (g_shape.frame_count >= MAX_FRAMES) {
        statusf(L"Maximum %d frames", MAX_FRAMES);
        return;
    }
    snapshot();
    ensure_first_frame();
    if (!g_shape.frame_count) {
        statusf(L"Out of memory");
        return;
    }
    if (count > MAX_FRAMES - g_shape.frame_count)
        count = MAX_FRAMES - g_shape.frame_count;
    FrameDot *copies[MAX_FRAMES] = {0};
    for (size_t n = 0; n < count; n++) {
        copies[n] = (FrameDot *)malloc(g_shape.dot_count * sizeof(FrameDot));
        if (!copies[n]) {
            for (size_t q = 0; q < n; q++)
                free(copies[q]);
            restore_undo_copy();
            statusf(L"Out of memory");
            return;
        }
        memcpy(copies[n], g_shape.frames[g_current_frame], g_shape.dot_count * sizeof(FrameDot));
    }
    size_t insert = g_current_frame + 1, old = g_shape.frame_count;
    for (size_t f = old; f > insert; f--)
        g_shape.frames[f + count - 1] = g_shape.frames[f - 1];
    for (size_t n = 0; n < count; n++)
        g_shape.frames[insert + n] = copies[n];
    g_shape.frame_count += count;
    g_current_frame = insert;
    statusf(L"Frame %zu of %zu", g_current_frame + 1, g_shape.frame_count);
}
static void add_frame(void) { add_frames_count(1); }
static void delete_frames_count(size_t count) {
    if (g_shape.frame_count <= 1) {
        statusf(L"Cannot delete only frame");
        return;
    }
    if (count > g_shape.frame_count - g_current_frame)
        count = g_shape.frame_count - g_current_frame;
    if (count >= g_shape.frame_count)
        count = g_shape.frame_count - 1;
    if (!count)
        return;
    snapshot();
    for (size_t f = g_current_frame; f < g_current_frame + count; f++)
        free(g_shape.frames[f]);
    for (size_t f = g_current_frame; f + count < g_shape.frame_count; f++)
        g_shape.frames[f] = g_shape.frames[f + count];
    for (size_t f = g_shape.frame_count - count; f < g_shape.frame_count; f++)
        g_shape.frames[f] = NULL;
    g_shape.frame_count -= count;
    if (g_current_frame)
        g_current_frame--;
    if (g_current_frame >= g_shape.frame_count)
        g_current_frame = g_shape.frame_count - 1;
    remap_selection_to_current_frame();
    statusf(L"Frame %zu of %zu", g_current_frame + 1, g_shape.frame_count);
}
static void copy_to_frame(size_t destination) {
    if (g_shape.frame_count < 2 || destination == 0 || destination >= g_shape.frame_count) {
        statusf(L"Choose another destination frame");
        return;
    }
    snapshot();
    memcpy(g_shape.frames[destination], g_shape.frames[g_current_frame], g_shape.dot_count * sizeof(FrameDot));
    statusf(L"Copied frame %zu to frame %zu", g_current_frame + 1, destination + 1);
}
static void shift_animation(void) {
    ensure_first_frame();
    if (!g_shape.frame_count)
        return;
    snapshot();
    tmp_mark_selected();
    spread_current_frame_flags();
    FrameDot *current = g_shape.frames[g_current_frame];
    for (size_t i = 0; i < g_shape.dot_count; i++)
        if (current[i].active & 0x0200) {
            FrameDot last = g_shape.frames[g_shape.frame_count - 1][i];
            for (size_t f = g_shape.frame_count - 1; f > 0; f--)
                g_shape.frames[f][i] = g_shape.frames[f - 1][i];
            g_shape.frames[0][i] = last;
        }
    for (size_t i = 0; i < g_shape.dot_count; i++) {
        g_shape.dots[i].x = g_shape.frames[0][i].x;
        g_shape.dots[i].y = g_shape.frames[0][i].y;
        g_shape.dots[i].z = g_shape.frames[0][i].z;
    }
    g_bsp_valid = 0;
    statusf(L"Animation shifted");
}
static size_t selected_dot_count(void) {
    size_t n = 0;
    for (size_t i = 0; i < g_shape.dot_count; i++)
        n += g_shape.dots[i].selected != 0;
    return n;
}
static void build_transform_selection(int include_selected_polygons) {
    memset(g_transform_selected, 0, g_shape.dot_count);
    g_transform_count = 0;
    for (size_t i = 0; i < g_shape.dot_count; i++)
        if (g_shape.dots[i].selected) {
            g_transform_selected[i] = 1;
            g_transform_count++;
        }
    if (include_selected_polygons)
        for (size_t p = 0; p < g_shape.poly_count; p++)
            if (g_shape.polys[p].selected)
                for (unsigned j = 0; j < g_shape.polys[p].count; j++) {
                    size_t i = g_shape.polys[p].index[j];
                    if (i < g_shape.dot_count && !g_transform_selected[i]) {
                        g_transform_selected[i] = 1;
                        g_transform_count++;
                    }
                }
}
static double snap_dot_coordinate(double value) {
    if (g_grid <= 0.0)
        return value;
    return trunc((value + (value > 0.0 ? g_grid * .5 : -g_grid * .5)) / g_grid) * g_grid;
}
static void add_dot_at(double x, double y, double z) {
    size_t at = g_shape.dot_count;
    for (size_t i = 0; i < g_shape.dot_count; i++)
        if (!dot_active(i)) {
            at = i;
            break;
        }
    if (at == g_shape.dot_count && at >= MAX_DOTS) {
        statusf(L"Dot limit reached");
        return;
    }
    x = dos_coord(snap_dot_coordinate(x));
    y = dos_coord(snap_dot_coordinate(y));
    z = dos_coord(snap_dot_coordinate(z));
    snapshot();
    if (at == g_shape.dot_count)
        for (size_t f = 0; f < g_shape.frame_count; f++) {
            FrameDot *q = (FrameDot *)realloc(g_shape.frames[f], (at + 1) * sizeof(FrameDot));
            if (!q) {
                restore_undo_copy();
                statusf(L"Out of memory");
                return;
            }
            g_shape.frames[f] = q;
        }
    g_shape.dots[at] = (Dot){x, y, z, 0};
    for (size_t f = 0; f < g_shape.frame_count; f++)
        g_shape.frames[f][at] = (FrameDot){x, y, z, g_current_group};
    if (at == g_shape.dot_count)
        g_shape.dot_count++;
    set_dot_selected(at, 1);
    for (size_t f = 0; f < g_shape.frame_count; f++)
        g_shape.frames[f][at].active |= 0x0100;
    statusf(L" %.0f %.0f %.0f", x, y, z);
}
static void add_dot_at_origin(void) { add_dot_at(g_origin[0], g_origin[1], g_origin[2]); }
static void reverse_poly(Poly *p) {
    if (p->count < 2)
        return;
    if (p->count == 2) {
        uint16_t q = p->index[0];
        p->index[0] = p->index[1];
        p->index[1] = q;
        return;
    }
    uint16_t old[MAX_POLY_VERTS];
    memcpy(old, p->index, p->count * sizeof(old[0]));
    p->index[0] = old[2];
    p->index[1] = old[1];
    p->index[2] = old[0];
    for (unsigned i = 3; i < p->count; i++)
        p->index[i] = old[p->count + 2 - i];
}
static double *frame_axis_ptr(FrameDot *d, int a) { return a == 0 ? &d->x : a == 1 ? &d->y
                                                                                   : &d->z; }
static void negate_frame_axis(FrameDot *d, int axis) {
    double *q = frame_axis_ptr(d, axis);
    *q = dos_coord(-*q);
}
static int mirror_shape(int axes, int selected_only, int add_image) {
    ensure_first_frame();
    snapshot();
    if (selected_only)
        tmp_mark_selected();
    else
        tmp_mark_all();
    spread_current_frame_flags();
    size_t extent = 0, highest = 0, chosen = 0, oldpolys = g_shape.poly_count, old_select_count = g_select_count;
    for (size_t i = 0; i < g_shape.dot_count; i++)
        if (dot_active(i))
            extent = i + 1;
    for (size_t i = 0; i < extent; i++)
        if (g_shape.frames[g_current_frame][i].active & 0x0200) {
            highest = i + 1;
            chosen++;
        }
    if (add_image) {
        if (extent + highest > MAX_DOTS) {
            restore_undo_copy();
            statusf(L"Dot limit reached");
            return 0;
        }
        size_t olddots = g_shape.dot_count, newcount = extent + highest;
        for (size_t f = 0; f < g_shape.frame_count; f++) {
            FrameDot *q = (FrameDot *)realloc(g_shape.frames[f], newcount * sizeof(FrameDot));
            if (!q) {
                restore_undo_copy();
                statusf(L"Out of memory (Undo restores the shape)");
                return 0;
            }
            g_shape.frames[f] = q;
            if (newcount > olddots)
                memset(&q[olddots], 0, (newcount - olddots) * sizeof(FrameDot));
            for (size_t i = 0; i < highest; i++)
                if (q[i].active & 0x0200) {
                    size_t dest = extent + i;
                    q[dest] = q[i];
                    for (int a = 0; a < 3; a++)
                        if (axes & (1 << a))
                            negate_frame_axis(&q[dest], a);
                    q[dest].active &= (uint16_t)~0x0200;
                }
        }
        for (size_t i = 0; i < highest; i++)
            if (g_shape.frames[0][extent + i].active) {
                FrameDot *d = &g_shape.frames[0][extent + i];
                int selected = (g_shape.frames[g_current_frame][extent + i].active & 0x0100) != 0;
                g_shape.dots[extent + i] = (Dot){d->x, d->y, d->z, (uint8_t)selected};
            }
        g_shape.dot_count = newcount;
        for (size_t q = 0; q < old_select_count && g_select_count < MAX_DOTS; q++) {
            size_t source = g_select_order[q];
            if (source < highest && g_shape.dots[extent + source].selected)
                g_select_order[g_select_count++] = (uint16_t)(extent + source);
        }
        for (size_t i = 0; i < oldpolys; i++)
            if (g_shape.polys[i].flags & 0x0200) {
                size_t slot = g_shape.poly_count;
                for (size_t q = 0; q < g_shape.poly_count; q++)
                    if (!g_shape.polys[q].flags) {
                        slot = q;
                        break;
                    }
                if (slot >= MAX_POLYS)
                    break;
                Poly p = g_shape.polys[i];
                p.flags &= (uint16_t)~0x0200;
                for (unsigned j = 0; j < p.count; j++)
                    p.index[j] = (uint16_t)(p.index[j] + extent);
                reverse_poly(&p);
                g_shape.polys[slot] = p;
                if (slot == g_shape.poly_count)
                    g_shape.poly_count++;
            }
    } else {
        for (size_t f = 0; f < g_shape.frame_count; f++)
            for (size_t i = 0; i < g_shape.dot_count; i++)
                if (g_shape.frames[f][i].active & 0x0200)
                    for (int a = 0; a < 3; a++)
                        if (axes & (1 << a))
                            negate_frame_axis(&g_shape.frames[f][i], a);
        for (size_t i = 0; i < g_shape.dot_count; i++) {
            g_shape.dots[i].x = g_shape.frames[0][i].x;
            g_shape.dots[i].y = g_shape.frames[0][i].y;
            g_shape.dots[i].z = g_shape.frames[0][i].z;
        }
        for (size_t i = 0; i < oldpolys; i++)
            if (g_shape.polys[i].flags & 0x0200)
                reverse_poly(&g_shape.polys[i]);
    }
    g_bsp_valid = 0;
    statusf(add_image ? L"Mirrored image added: %zu dots, %zu polygons" : L"Mirrored %zu dots", chosen, g_shape.poly_count - oldpolys);
    return 1;
}

static void mirror_dialog(void) {
    memset(g_mirror_options, 0, sizeof(g_mirror_options));
    g_mirror_options[0] = 1;
    g_mirror_options[4] = 1;
    for (size_t i = 0; i < g_shape.poly_count; i++)
        if (g_shape.polys[i].selected) {
            g_mirror_options[3] = 1;
            break;
        }
    g_dos_prompt = PROMPT_MIRROR;
    statusf(L"Mirror Shape");
    InvalidateRect(g_hwnd, NULL, FALSE);
}
static void toggle_mirror_option(int option) {
    if (option < 0 || option >= 5)
        return;
    g_mirror_options[option] ^= 1;
    if (!g_mirror_options[4])
        g_mirror_options[3] = 0;
}
static void open_number_prompt(int mode, const wchar_t *title, const wchar_t *label, int initial) {
    g_number_mode = mode;
    wcscpy(g_number_title, title);
    wcscpy(g_number_label, label);
    wsprintfW(g_number_text, L"%d", initial);
    g_number_active_edit = 0;
    g_number_replace_text = 0;
    g_dos_prompt = PROMPT_NUMBER;
    statusf(L"%ls", title);
    InvalidateRect(g_hwnd, NULL, FALSE);
}
static void finish_number_prompt(int accept) {
    if (!accept) {
        g_dos_prompt = PROMPT_NONE;
        g_number_active_edit = 0;
        g_number_mode = NUMBER_NONE;
        statusf(L"Ready");
        InvalidateRect(g_hwnd, NULL, FALSE);
        return;
    }
    wchar_t *end;
    long value = wcstol(g_number_text, &end, 10);
    while (*end == L' ' || *end == L'\t')
        end++;
    if (end == g_number_text || *end || value < 0 || value > 255) {
        g_number_active_edit = 1;
        g_number_replace_text = 1;
        statusf(L"Enter a colour from 0 to 255");
        MessageBeep(MB_ICONWARNING);
        InvalidateRect(g_hwnd, NULL, FALSE);
        return;
    }
    int mode = g_number_mode;
    g_dos_prompt = PROMPT_NONE;
    g_number_active_edit = 0;
    g_number_mode = NUMBER_NONE;
    if (mode == NUMBER_DEFAULT_COLOUR) {
        g_poly_colour = (uint16_t)value;
        statusf(L"Default colour %ld", value);
    } else {
        snapshot();
        for (size_t i = 0; i < g_shape.poly_count; i++)
            if (g_shape.polys[i].selected)
                g_shape.polys[i].colour = (uint16_t)value;
        statusf(L"Colour  %ld", value);
    }
    InvalidateRect(g_hwnd, NULL, FALSE);
}
static void colour_selected_polygons(void) {
    size_t selected = 0, first = 0;
    for (size_t i = 0; i < g_shape.poly_count; i++)
        if (g_shape.polys[i].selected) {
            if (!selected)
                first = i;
            selected++;
        }
    if (!selected) {
        statusf(L"No polygons selected");
        return;
    }
    int value = selected == 1 ? g_shape.polys[first].colour : g_poly_colour;
    open_number_prompt(NUMBER_POLYGON_COLOUR, L"Colour Polygon(s)", L"Polygon colour", value);
}
static void choose_default_colour(void) { open_number_prompt(NUMBER_DEFAULT_COLOUR, L"Colour", L"Default colour", g_poly_colour); }
static void apply_select_polygon_action(int action) {
    size_t changed = 0;
    for (size_t i = 0; i < g_shape.poly_count; i++) {
        Poly *p = &g_shape.polys[i];
        if (!(p->flags & g_shape_display_mask))
            continue;
        int match = 0, on = 1;
        if (action == 0)
            match = 1;
        else if (action == 1) {
            match = 1;
            on = 0;
        } else if (action == 2 || action == 3) {
            on = action == 2;
            for (unsigned j = 0; j < p->count; j++)
                if (p->index[j] < g_shape.dot_count && g_shape.dots[p->index[j]].selected) {
                    match = 1;
                    break;
                }
        } else if (action >= 4 && action <= 9)
            match = p->count == (unsigned)(action - 3);
        else if (action == 10)
            match = p->count >= 7;
        if (match && p->selected != (uint8_t)on) {
            p->selected = (uint8_t)on;
            changed++;
        }
    }
    statusf(L"Select Polygons: %zu changed", changed);
}
static void select_polygons_dialog(void) {
    snapshot();
    size_t counts[8] = {0}, active = 0;
    for (size_t i = 0; i < g_shape.poly_count; i++)
        if (g_shape.polys[i].flags) {
            unsigned n = g_shape.polys[i].count;
            active++;
            if (n >= 7)
                counts[7]++;
            else
                counts[n]++;
        }
    wsprintfW(g_selectpolys_labels[0], L"Select all [%u]", (unsigned)active);
    wcscpy(g_selectpolys_labels[1], L"Deselect all");
    wcscpy(g_selectpolys_labels[2], L"Select");
    wcscpy(g_selectpolys_labels[3], L"DeSelect");
    for (int n = 1; n <= 6; n++) {
        if (n == 1)
            wsprintfW(g_selectpolys_labels[n + 3], L"1 sided(dots)[%u]", (unsigned)counts[n]);
        else if (n == 2)
            wsprintfW(g_selectpolys_labels[n + 3], L"2 sided(lines)[%u]", (unsigned)counts[n]);
        else
            wsprintfW(g_selectpolys_labels[n + 3], L"%d sided[%u]", n, (unsigned)counts[n]);
    }
    wsprintfW(g_selectpolys_labels[10], L"7+ sides[%u]", (unsigned)counts[7]);
    g_selectpolys_action = g_select_count ? 2 : 0;
    g_dos_prompt = PROMPT_SELECT_POLYS;
    statusf(L"Select Polygons");
    InvalidateRect(g_hwnd, NULL, FALSE);
}
static void finish_selectpolys_prompt(int accept) {
    g_dos_prompt = PROMPT_NONE;
    if (accept)
        apply_select_polygon_action(g_selectpolys_action);
    else
        statusf(L"Ready");
    InvalidateRect(g_hwnd, NULL, FALSE);
}
static void frames_dialog(void) {
    ensure_first_frame();
    snapshot();
    wcscpy(g_frames_text[0], L"0");
    wcscpy(g_frames_text[1], L"0");
    g_frames_text[2][0] = 0;
    g_frames_action = 0;
    g_frames_active_edit = -1;
    g_frames_replace_text = 0;
    g_dos_prompt = PROMPT_FRAMES;
    statusf(L"Animation Frames");
    InvalidateRect(g_hwnd, NULL, FALSE);
}
static int parse_scale_text(int index, double *out) {
    wchar_t *end;
    double value = wcstod(g_scale_text[index], &end);
    while (*end == L' ' || *end == L'\t')
        end++;
    if (end == g_scale_text[index] || *end || !isfinite(value))
        return 0;
    *out = value;
    return 1;
}
static void commit_scale_edit(void) {
    if (g_scale_active_edit == 0) {
        double value;
        if (parse_scale_text(0, &value) && value != 0.0)
            for (int i = 1; i < 4; i++)
                wcscpy(g_scale_text[i], g_scale_text[0]);
    }
    g_scale_active_edit = -1;
    g_scale_replace_text = 0;
}
static void finish_scale_prompt(int accept) {
    commit_scale_edit();
    if (!accept) {
        g_dos_prompt = PROMPT_NONE;
        statusf(L"Ready");
        InvalidateRect(g_hwnd, NULL, FALSE);
        return;
    }
    double factor[3];
    for (int a = 0; a < 3; a++)
        if (!parse_scale_text(a + 1, &factor[a])) {
            g_scale_active_edit = a + 1;
            g_scale_replace_text = 1;
            statusf(L"Enter a valid scale");
            MessageBeep(MB_ICONWARNING);
            InvalidateRect(g_hwnd, NULL, FALSE);
            return;
        }
    int selected_only = g_scale_options[0], all_frames = g_scale_options[1];
    if (g_shape.frame_count) {
        size_t first = all_frames ? 0 : g_current_frame, last = all_frames ? g_shape.frame_count : g_current_frame + 1;
        uint16_t mask = selected_only ? 0x0100 : 0xffff;
        for (size_t f = first; f < last; f++)
            for (size_t i = 0; i < g_shape.dot_count; i++)
                if (g_shape.frames[f][i].active & mask) {
                    g_shape.frames[f][i].x = dos_coord(g_shape.frames[f][i].x * factor[0]);
                    g_shape.frames[f][i].y = dos_coord(g_shape.frames[f][i].y * factor[1]);
                    g_shape.frames[f][i].z = dos_coord(g_shape.frames[f][i].z * factor[2]);
                }
        if (first == 0)
            for (size_t i = 0; i < g_shape.dot_count; i++) {
                g_shape.dots[i].x = g_shape.frames[0][i].x;
                g_shape.dots[i].y = g_shape.frames[0][i].y;
                g_shape.dots[i].z = g_shape.frames[0][i].z;
            }
    } else
        for (size_t i = 0; i < g_shape.dot_count; i++)
            if (!selected_only || g_shape.dots[i].selected) {
                g_shape.dots[i].x = dos_coord(g_shape.dots[i].x * factor[0]);
                g_shape.dots[i].y = dos_coord(g_shape.dots[i].y * factor[1]);
                g_shape.dots[i].z = dos_coord(g_shape.dots[i].z * factor[2]);
            }
    g_dos_prompt = PROMPT_NONE;
    g_bsp_valid = 0;
    statusf(L"Size X %.4g Y %.4g Z %.4g", factor[0], factor[1], factor[2]);
    InvalidateRect(g_hwnd, NULL, FALSE);
}
static void scale_shape_dialog(void) {
    if (!g_shape.dot_count) {
        statusf(L"No dots to size");
        return;
    }
    snapshot();
    spread_current_frame_flags();
    for (int i = 0; i < 4; i++)
        wcscpy(g_scale_text[i], L"1.0");
    g_scale_options[0] = 0;
    g_scale_options[1] = 1;
    g_scale_active_edit = -1;
    g_scale_replace_text = 0;
    g_dos_prompt = PROMPT_SCALE;
    statusf(L"Size Shape");
    InvalidateRect(g_hwnd, NULL, FALSE);
}
static int copy_selection(void) {
    if (!g_transform_count)
        return 0;
    ensure_first_frame();
    size_t extent = 0, highest = 0;
    for (size_t i = 0; i < g_shape.dot_count; i++)
        if (dot_active(i))
            extent = i + 1;
    for (size_t i = 0; i < extent; i++)
        if (g_transform_selected[i])
            highest = i + 1;
    if (!highest || extent + highest > MAX_DOTS)
        return 0;
    snapshot();
    tmp_mark_selected();
    spread_current_frame_flags();
    size_t olddots = g_shape.dot_count, newcount = extent + highest, oldpolys = g_shape.poly_count;
    for (size_t f = 0; f < g_shape.frame_count; f++) {
        FrameDot *q = (FrameDot *)realloc(g_shape.frames[f], newcount * sizeof(FrameDot));
        if (!q) {
            restore_undo_copy();
            return 0;
        }
        g_shape.frames[f] = q;
        if (newcount > olddots)
            memset(&q[olddots], 0, (newcount - olddots) * sizeof(FrameDot));
    }
    memset(g_copy_source, 0xff, sizeof(g_copy_source));
    g_copy_start = extent;
    g_copy_count = highest;
    for (size_t i = 0; i < highest; i++)
        if (g_transform_selected[i]) {
            size_t dest = extent + i;
            g_copy_source[dest] = (uint16_t)i;
            g_shape.dots[dest] = g_shape.dots[i];
            g_shape.dots[dest].selected = 0;
            for (size_t f = 0; f < g_shape.frame_count; f++) {
                g_shape.frames[f][dest] = g_shape.frames[f][i];
                g_shape.frames[f][dest].active = g_current_group;
            }
        }
    g_shape.dot_count = newcount;
    for (size_t i = 0; i < oldpolys; i++)
        if ((g_shape.polys[i].flags & 0x0200) && g_shape.polys[i].selected) {
            size_t slot = g_shape.poly_count;
            for (size_t q = 0; q < g_shape.poly_count; q++)
                if (!g_shape.polys[q].flags) {
                    slot = q;
                    break;
                }
            if (slot >= MAX_POLYS)
                break;
            Poly p = g_shape.polys[i];
            p.flags &= (uint16_t)~0x0200;
            for (unsigned j = 0; j < p.count; j++)
                p.index[j] = (uint16_t)(p.index[j] + extent);
            g_shape.polys[slot] = p;
            if (slot == g_shape.poly_count)
                g_shape.poly_count++;
        }
    return 1;
}
static int add_rotated_image(const Dot *rotated, size_t rotated_count) {
    ensure_first_frame();
    size_t extent = 0, highest = 0;
    for (size_t i = 0; i < g_shape.dot_count; i++)
        if (dot_active(i))
            extent = i + 1;
    for (size_t i = 0; i < extent; i++)
        if (g_transform_selected[i])
            highest = i + 1;
    if (!highest || extent + highest > MAX_DOTS)
        return 0;
    snapshot();
    tmp_mark_selected();
    spread_current_frame_flags();
    size_t olddots = g_shape.dot_count, newcount = extent + highest, oldpolys = g_shape.poly_count;
    for (size_t f = 0; f < g_shape.frame_count; f++) {
        FrameDot *q = (FrameDot *)realloc(g_shape.frames[f], newcount * sizeof(FrameDot));
        if (!q) {
            restore_undo_copy();
            return 0;
        }
        g_shape.frames[f] = q;
        if (newcount > olddots)
            memset(&q[olddots], 0, (newcount - olddots) * sizeof(FrameDot));
    }
    size_t w = 0;
    for (size_t i = 0; i < highest; i++)
        if (g_transform_selected[i] && w < rotated_count) {
            size_t dest = extent + i;
            Dot d = rotated[w++];
            g_shape.frames[g_current_frame][dest] = (FrameDot){d.x, d.y, d.z, g_current_group};
            g_shape.dots[dest] = (Dot){0, 0, 0, 0};
            if (g_current_frame == 0) {
                g_shape.dots[dest].x = d.x;
                g_shape.dots[dest].y = d.y;
                g_shape.dots[dest].z = d.z;
            }
        }
    g_shape.dot_count = newcount;
    for (size_t i = 0; i < oldpolys; i++)
        if ((g_shape.polys[i].flags & 0x0200) && g_shape.polys[i].selected) {
            size_t slot = g_shape.poly_count;
            for (size_t q = 0; q < g_shape.poly_count; q++)
                if (!g_shape.polys[q].flags) {
                    slot = q;
                    break;
                }
            if (slot >= MAX_POLYS)
                break;
            Poly p = g_shape.polys[i];
            p.flags &= (uint16_t)~0x0200;
            for (unsigned j = 0; j < p.count; j++)
                p.index[j] = (uint16_t)(p.index[j] + extent);
            g_shape.polys[slot] = p;
            if (slot == g_shape.poly_count)
                g_shape.poly_count++;
        }
    return 1;
}
static void begin_tool_drag(int x, int y, int view) {
    if (!(g_tool == ID_EDIT_ROTATE && g_rotate_stage == 2))
        build_transform_selection(g_tool == ID_EDIT_COPY || g_tool == ID_EDIT_ROTATE);
    if (!g_transform_count) {
        statusf(L"Select dots first");
        return;
    }
    RECT c;
    GetClientRect(g_hwnd, &c);
    View views[4];
    make_views(c, views);
    View *v = &views[view];
    if (g_tool == ID_EDIT_ROTATE) {
        if (g_rotate_stage == 0) {
            snapshot();
            tmp_mark_selected();
            spread_current_frame_flags();
            g_drag_view = view;
            g_rotate_center[0] = g_origin[0];
            g_rotate_center[1] = g_origin[1];
            g_rotate_center[2] = g_origin[2];
            g_rotate_center[v->axes[0]] = view_world_coordinate(v, 0, x);
            g_rotate_center[v->axes[1]] = view_world_coordinate(v, 1, y);
            g_rotate_stage = 1;
            statusf(L"Release & move off centre");
            InvalidateRect(g_hwnd, NULL, FALSE);
            return;
        }
        if (g_rotate_stage != 2 || view != g_drag_view)
            return;
        double ux = view_world_coordinate(v, 0, x) - g_rotate_center[v->axes[0]];
        double uy = view_world_coordinate(v, 1, y) - g_rotate_center[v->axes[1]];
        g_rotate_start_angle = atan2(uy, ux);
        g_dragging = 1;
        g_drag_start = (POINT){x, y};
        SetCapture(g_hwnd);
        return;
    }
    if (g_tool == ID_EDIT_COPY && !copy_selection()) {
        statusf(L"Cannot copy selection");
        return;
    } else if (g_tool != ID_EDIT_COPY)
        snapshot();
    g_dragging = 1;
    g_drag_start = (POINT){x, y};
    g_drag_view = view;
    SetCapture(g_hwnd);
}
static double *axis_ptr(Dot *d, int a) { return a == 0 ? &d->x : a == 1 ? &d->y
                                                                        : &d->z; }
static size_t copied_source(size_t index) { return index < MAX_DOTS ? g_copy_source[index] : 0; }
static Dot undo_display_dot(size_t index) {
    Dot d = g_undo.dots[index];
    if (g_undo.frame_count && g_current_frame < g_undo.frame_count && g_undo.frames[g_current_frame]) {
        FrameDot *f = &g_undo.frames[g_current_frame][index];
        d.x = f->x;
        d.y = f->y;
        d.z = f->z;
    }
    return d;
}
static void put_display_dot(size_t index, Dot d) {
    d.x = dos_coord(d.x);
    d.y = dos_coord(d.y);
    d.z = dos_coord(d.z);
    if (g_shape.frame_count && g_shape.frames[g_current_frame]) {
        FrameDot *f = &g_shape.frames[g_current_frame][index];
        f->x = d.x;
        f->y = d.y;
        f->z = d.z;
    } else {
        uint8_t selected = g_shape.dots[index].selected;
        g_shape.dots[index] = d;
        g_shape.dots[index].selected = selected;
    }
}
static void update_tool_drag(int x, int y) {
    if (!g_dragging)
        return;
    RECT c;
    GetClientRect(g_hwnd, &c);
    View views[4];
    make_views(c, views);
    View *v = &views[g_drag_view];
    double start_u = view_world_coordinate(v, 0, g_drag_start.x), start_v = view_world_coordinate(v, 1, g_drag_start.y), current_u = view_world_coordinate(v, 0, x), current_v = view_world_coordinate(v, 1, y);
    double dx = snap_dot_coordinate(current_u) - snap_dot_coordinate(start_u), dy = snap_dot_coordinate(current_v) - snap_dot_coordinate(start_v);
    if (g_tool == ID_EDIT_MOVE) {
        for (size_t i = 0; i < g_shape.dot_count; i++)
            if (g_shape.dots[i].selected) {
                Dot d = undo_display_dot(i);
                *axis_ptr(&d, v->axes[0]) += dx;
                *axis_ptr(&d, v->axes[1]) += dy;
                put_display_dot(i, d);
            }
    } else if (g_tool == ID_EDIT_COPY) {
        for (size_t i = g_copy_start; i < g_copy_start + g_copy_count && i < g_shape.dot_count; i++) {
            size_t source = copied_source(i);
            if (source == UINT16_MAX || source >= g_undo.dot_count)
                continue;
            for (size_t f = 0; f < g_shape.frame_count; f++) {
                FrameDot *d = &g_shape.frames[f][i], *b = &g_undo.frames[f][source];
                d->x = b->x;
                d->y = b->y;
                d->z = b->z;
                d->active = g_current_group;
                double *q = v->axes[0] == 0 ? &d->x : v->axes[0] == 1 ? &d->y
                                                                      : &d->z;
                *q = dos_coord(*q + dx);
                q = v->axes[1] == 0 ? &d->x : v->axes[1] == 1 ? &d->y
                                                              : &d->z;
                *q = dos_coord(*q + dy);
            }
            g_shape.dots[i].x = g_shape.frames[0][i].x;
            g_shape.dots[i].y = g_shape.frames[0][i].y;
            g_shape.dots[i].z = g_shape.frames[0][i].z;
            g_shape.dots[i].selected = 0;
        }
    } else if (g_tool == ID_EDIT_SIZE) {
        double scale = exp((x - g_drag_start.x) / 120.0);
        for (size_t i = 0; i < g_shape.dot_count; i++)
            if (g_shape.dots[i].selected) {
                Dot b = undo_display_dot(i);
                b.x = g_origin[0] + (b.x - g_origin[0]) * scale;
                b.y = g_origin[1] + (b.y - g_origin[1]) * scale;
                b.z = g_origin[2] + (b.z - g_origin[2]) * scale;
                put_display_dot(i, b);
            }
    } else if (g_tool == ID_EDIT_ROTATE) {
        double ux = view_world_coordinate(v, 0, x) - g_rotate_center[v->axes[0]], uy = view_world_coordinate(v, 1, y) - g_rotate_center[v->axes[1]], a = atan2(uy, ux) - g_rotate_start_angle, cs = cos(a), sn = sin(a);
        for (size_t i = 0; i < g_shape.dot_count; i++)
            if (g_transform_selected[i]) {
                Dot b = undo_display_dot(i);
                double u = *axis_ptr(&b, v->axes[0]) - g_rotate_center[v->axes[0]], q = *axis_ptr(&b, v->axes[1]) - g_rotate_center[v->axes[1]];
                *axis_ptr(&b, v->axes[0]) = trunc(g_rotate_center[v->axes[0]] + u * cs - q * sn);
                *axis_ptr(&b, v->axes[1]) = trunc(g_rotate_center[v->axes[1]] + u * sn + q * cs);
                put_display_dot(i, b);
            }
        statusf(L"Angle %.2f", a * 180.0 / 3.141592653589793);
    }
    InvalidateRect(g_hwnd, NULL, FALSE);
}

static void finish_rotate_choice(int add_image) {
    size_t nsel = g_transform_count;
    g_dos_prompt = PROMPT_NONE;
    if (add_image && nsel) {
        Dot *rotated = (Dot *)malloc(nsel * sizeof(Dot));
        if (rotated) {
            size_t w = 0;
            for (size_t i = 0; i < g_shape.dot_count; i++)
                if (g_transform_selected[i])
                    rotated[w++] = display_dot(i);
            restore_undo_copy();
            if (add_rotated_image(rotated, w))
                statusf(L"Rotated image added");
            else
                statusf(L"Cannot add rotated image");
            free(rotated);
        }
    } else
        statusf(L"Rotate complete");
    g_dragging = 0;
    g_rotate_stage = 0;
    g_tool = 0;
    ReleaseCapture();
    g_bsp_valid = 0;
    InvalidateRect(g_hwnd, NULL, FALSE);
}
static void finish_rotate(void) {
    g_dragging = 0;
    g_rotate_stage = 0;
    g_tool = 0;
    ReleaseCapture();
    g_dos_prompt = PROMPT_ROTATE_ADD;
    statusf(L"Add image");
    InvalidateRect(g_hwnd, NULL, FALSE);
}
static int same_cyclic_poly(const Poly *a, const Poly *b) {
    if (a->count != b->count)
        return 0;
    if (!a->count)
        return 1;
    for (unsigned start = 0; start < b->count; start++) {
        unsigned j = 0;
        for (; j < a->count; j++)
            if (a->index[j] != b->index[(start + j) % b->count])
                break;
        if (j == a->count)
            return 1;
    }
    return 0;
}
static int compact_dot_key_compare(size_t a, size_t b, const uint16_t *classes) {
    if (classes[a] != classes[b])
        return classes[a] > classes[b] ? 1 : -1;
    Dot da = display_dot(a), db = display_dot(b);
    if (da.z != db.z)
        return da.z > db.z ? 1 : -1;
    if (da.y != db.y)
        return da.y > db.y ? 1 : -1;
    double ax = fabs(da.x), bx = fabs(db.x);
    if (ax != bx)
        return ax > bx ? 1 : -1;
    return 0;
}
static int sort_compacted_dots(void) {
    size_t count = g_shape.dot_count;
    if (count < 2)
        return 1;
    uint16_t classes[MAX_DOTS] = {0}, order[MAX_DOTS], old_to_new[MAX_DOTS];
    for (size_t i = 0; i < count; i++) {
        order[i] = (uint16_t)i;
        if (g_shape.frame_count > 1) {
            FrameDot *base = &g_shape.frames[0][i];
            for (size_t f = 1; f < g_shape.frame_count; f++) {
                FrameDot *q = &g_shape.frames[f][i];
                if (base->x != q->x || base->y != q->y || base->z != q->z) {
                    classes[i] = 2;
                    break;
                }
            }
        }
    }
    for (size_t i = 0; i + 1 < count; i++)
        for (size_t j = i + 1; j < count; j++) {
            if (classes[j] & 1)
                continue;
            int mirror = 1;
            size_t frames = g_shape.frame_count ? g_shape.frame_count : 1;
            for (size_t f = 0; f < frames && mirror; f++) {
                Dot a = display_dot_at_frame(i, f), b = display_dot_at_frame(j, f);
                if (a.x != -b.x || a.y != b.y || a.z != b.z)
                    mirror = 0;
            }
            if (mirror) {
                classes[i] |= 1;
                classes[j] |= 1;
                break;
            }
        }
    for (size_t pass = 0; pass + 1 < count; pass++) {
        int changed = 0;
        for (size_t i = 0; i + 1 < count - pass; i++)
            if (compact_dot_key_compare(order[i], order[i + 1], classes) > 0) {
                uint16_t q = order[i];
                order[i] = order[i + 1];
                order[i + 1] = q;
                changed = 1;
            }
        if (!changed)
            break;
    }
    int changed = 0;
    for (size_t i = 0; i < count; i++) {
        old_to_new[order[i]] = (uint16_t)i;
        changed |= order[i] != i;
    }
    if (!changed)
        return 1;
    Dot *sorted_dots = (Dot *)malloc(count * sizeof(Dot));
    if (!sorted_dots)
        return 0;
    for (size_t i = 0; i < count; i++)
        sorted_dots[i] = g_shape.dots[order[i]];
    memcpy(g_shape.dots, sorted_dots, count * sizeof(Dot));
    free(sorted_dots);
    for (size_t f = 0; f < g_shape.frame_count; f++) {
        FrameDot *sorted = (FrameDot *)malloc(count * sizeof(FrameDot));
        if (!sorted)
            return 0;
        for (size_t i = 0; i < count; i++)
            sorted[i] = g_shape.frames[f][order[i]];
        memcpy(g_shape.frames[f], sorted, count * sizeof(FrameDot));
        free(sorted);
    }
    for (size_t i = 0; i < g_shape.poly_count; i++)
        for (unsigned j = 0; j < g_shape.polys[i].count; j++)
            g_shape.polys[i].index[j] = old_to_new[g_shape.polys[i].index[j]];
    for (size_t i = 0; i < g_select_count; i++)
        g_select_order[i] = old_to_new[g_select_order[i]];
    return 1;
}
static void compact_shape(void) {
    size_t olddots = g_shape.dot_count, oldpolys = g_shape.poly_count, extra_vertices = 0, merged_dots = 0, duplicate_polys = 0, invalid_polys = 0;
    snapshot();
    size_t dot_bytes = (olddots ? olddots : 1) * sizeof(uint16_t), poly_bytes = oldpolys ? oldpolys : 1;
    uint16_t *representative = (uint16_t *)malloc(dot_bytes), *newindex = (uint16_t *)malloc(dot_bytes);
    unsigned char *drop = (unsigned char *)calloc(poly_bytes, 1);
    if (!representative || !newindex || !drop) {
        free(representative);
        free(newindex);
        free(drop);
        statusf(L"Poly compact failed");
        return;
    }
    spread_current_frame_flags();
    double merge_distance = fmax(g_grid / 8.0, 1.0), merge_distance_squared = merge_distance * merge_distance;
    for (size_t i = 0; i < olddots; i++) {
        representative[i] = UINT16_MAX;
        if (!dot_active(i))
            continue;
        representative[i] = (uint16_t)i;
        Dot a = display_dot(i);
        for (size_t j = 0; j < i; j++)
            if (representative[j] == j) {
                Dot b = display_dot(j);
                double dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
                if (dx * dx + dy * dy + dz * dz <= merge_distance_squared) {
                    representative[i] = (uint16_t)j;
                    merged_dots++;
                    break;
                }
            }
    }
    /* Remove inactive polygons/references and repeated vertices, including repeats introduced by dot merging. */
    for (size_t i = 0; i < oldpolys; i++) {
        Poly *p = &g_shape.polys[i];
        if (!p->flags) {
            drop[i] = 1;
            continue;
        }
        unsigned w = 0;
        for (unsigned j = 0; j < p->count && j < MAX_POLY_VERTS; j++) {
            uint16_t old = p->index[j];
            if (old >= olddots || representative[old] == UINT16_MAX) {
                drop[i] = 1;
                invalid_polys++;
                break;
            }
            uint16_t index = representative[old];
            int duplicate = 0;
            for (unsigned k = 0; k < w; k++)
                if (p->index[k] == index) {
                    duplicate = 1;
                    break;
                }
            if (duplicate)
                extra_vertices++;
            else
                p->index[w++] = index;
        }
        p->count = (uint16_t)w;
        if (!drop[i] && !w) {
            drop[i] = 1;
            invalid_polys++;
        }
    }
    size_t dw = 0;
    for (size_t i = 0; i < olddots; i++)
        if (representative[i] == i) {
            newindex[i] = (uint16_t)dw;
            g_shape.dots[dw] = g_shape.dots[i];
            for (size_t f = 0; f < g_shape.frame_count; f++)
                g_shape.frames[f][dw] = g_shape.frames[f][i];
            dw++;
        }
    for (size_t i = 0; i < olddots; i++)
        if (representative[i] != UINT16_MAX && representative[i] != i)
            newindex[i] = newindex[representative[i]];
    g_shape.dot_count = dw;
    for (size_t i = 0; i < oldpolys; i++)
        if (!drop[i])
            for (unsigned j = 0; j < g_shape.polys[i].count; j++)
                g_shape.polys[i].index[j] = newindex[g_shape.polys[i].index[j]];
    /* RemDupPolys compares cyclic rotations, deliberately not reversed winding. */
    for (size_t i = 0; i < oldpolys; i++)
        if (!drop[i])
            for (size_t j = 0; j < i; j++)
                if (!drop[j] && same_cyclic_poly(&g_shape.polys[j], &g_shape.polys[i])) {
                    drop[i] = 1;
                    duplicate_polys++;
                    break;
                }
    size_t pw = 0;
    for (size_t i = 0; i < oldpolys; i++)
        if (!drop[i])
            g_shape.polys[pw++] = g_shape.polys[i];
    g_shape.poly_count = pw;
    for (size_t f = 0; f < g_shape.frame_count; f++) {
        FrameDot *q = (FrameDot *)realloc(g_shape.frames[f], g_shape.dot_count * sizeof(FrameDot));
        if (q || !g_shape.dot_count)
            g_shape.frames[f] = q;
    }
    for (size_t i = 0; i < g_shape.dot_count; i++)
        g_shape.dots[i].selected = 0;
    g_select_count = 0;
    for (size_t q = 0; q < g_undo_select_count; q++) {
        size_t old = g_undo_select_order[q];
        if (old < olddots && representative[old] == old)
            set_dot_selected(newindex[old], 1);
    }
    if (!sort_compacted_dots()) {
        restore_undo_copy();
        free(drop);
        free(representative);
        free(newindex);
        statusf(L"Poly compact failed");
        return;
    }
    free(drop);
    free(representative);
    free(newindex);
    g_bsp_valid = 0;
    statusf(L"Compact: %zu duplicate dots, %zu duplicate + %zu invalid polys, %zu extra vertices", merged_dots, duplicate_polys, invalid_polys, extra_vertices);
}
static double calculate_twist_test(void) {
    double total = 0;
    size_t polygon_count = 0;
    for (size_t i = 0; i < g_shape.poly_count; i++) {
        Poly *p = &g_shape.polys[i];
        if (!p->flags || p->count <= 2)
            continue;
        polygon_count++;
        p->selected = 0;
        double twist = 0;
        if (p->count > 3) {
            Dot a = display_dot(p->index[0]), b = display_dot(p->index[1]), c = display_dot(p->index[2]);
            double ux = b.x - a.x, uy = b.y - a.y, uz = b.z - a.z, vx = c.x - a.x, vy = c.y - a.y, vz = c.z - a.z;
            double nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
            double magnitude = sqrt(nx * nx + ny * ny + nz * nz);
            nx /= magnitude;
            ny /= magnitude;
            nz /= magnitude;
            double plane_d = nx * a.x + ny * a.y + nz * a.z, sum = 0;
            for (unsigned j = 0; j < p->count; j++) {
                Dot d = display_dot(p->index[j]);
                double distance = nx * d.x + ny * d.y + nz * d.z - plane_d;
                sum += distance * distance;
            }
            twist = sum / magnitude;
            if (twist > 0.01)
                p->selected = 1;
        }
        total += twist;
    }
    return polygon_count ? total * 100.0 / (double)polygon_count : 0;
}
static void twist_test(void) { statusf(L"Avg twist %.6f%%", calculate_twist_test()); }
static int save_twist_report(const wchar_t *path) {
    double average = calculate_twist_test();
    FILE *f = _wfopen(path, L"wb");
    if (!f)
        return 0;
    fprintf(f, "Avg twist %.6f%%\n", average);
    fputs("Selected", f);
    for (size_t i = 0; i < g_shape.poly_count; i++)
        if (g_shape.polys[i].selected)
            fprintf(f, " %llu", (unsigned long long)i);
    fputc('\n', f);
    return fclose(f) == 0;
}
static int polygon_plane_at_frame(size_t pi, size_t frame, double plane[4]) {
    Poly *p = &g_shape.polys[pi];
    if (p->count < 3)
        return 0;
    Dot a = display_dot_at_frame(p->index[0], frame), b = display_dot_at_frame(p->index[1], frame), c = display_dot_at_frame(p->index[2], frame);
    plane[0] = (b.y - a.y) * (c.z - a.z) - (b.z - a.z) * (c.y - a.y);
    plane[1] = (b.z - a.z) * (c.x - a.x) - (b.x - a.x) * (c.z - a.z);
    plane[2] = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    double len = sqrt(plane[0] * plane[0] + plane[1] * plane[1] + plane[2] * plane[2]);
    if (len < 1e-12)
        return 0;
    plane[0] /= len;
    plane[1] /= len;
    plane[2] /= len;
    plane[3] = -(plane[0] * a.x + plane[1] * a.y + plane[2] * a.z);
    return 1;
}
static int polygon_plane(size_t pi, double plane[4]) {
    size_t frame = g_shape.frame_count && g_current_frame < g_shape.frame_count ? g_current_frame : 0;
    return polygon_plane_at_frame(pi, frame, plane);
}
static int classify_poly(size_t pi, const double plane[4], double *centroid_side) {
    Poly *p = &g_shape.polys[pi];
    int front = 0, back = 0;
    double sum = 0;
    for (unsigned j = 0; j < p->count; j++) {
        Dot d = display_dot(p->index[j]);
        double side = plane[0] * d.x + plane[1] * d.y + plane[2] * d.z + plane[3];
        sum += side;
        if (side > g_plane_weight)
            front = 1;
        else if (side < -g_plane_weight)
            back = 1;
    }
    *centroid_side = p->count ? sum / p->count : 0;
    if (front && back)
        return 2;
    if (front)
        return 1;
    if (back)
        return -1;
    return 0;
}
static unsigned bsp_relation_flags(const int *items, int count, unsigned short *out) {
    unsigned spanning = 0;
    memset(out, 0, (size_t)count * sizeof(*out));
    for (int a = 0; a < count; a++)
        for (int b = a + 1; b < count; b++) {
            double pa[4], pb[4], ca, cb;
            if (!polygon_plane((size_t)items[a], pa) || !polygon_plane((size_t)items[b], pb))
                continue;
            int ab = classify_poly((size_t)items[a], pb, &ca), ba = classify_poly((size_t)items[b], pa, &cb);
            int af = ab == 1 || ab == 2, ak = ab == -1 || ab == 2, bf = ba == 1 || ba == 2, bk = ba == -1 || ba == 2;
            if (ab == 2 || ba == 2)
                spanning++;
            if (ab == 2 && ba == 2) {
                out[a] |= 0x7000;
                out[b] |= 0x7000;
                continue;
            }
            if (ab == 2) {
                out[b] |= (unsigned short)(0x1000 | (bf ? 0x2000 : 0) | (bk ? 0x4000 : 0));
                out[a] |= (unsigned short)(bf ? 0x4000 : 0x2000);
            } else if (ba == 2) {
                out[a] |= (unsigned short)(0x1000 | (af ? 0x2000 : 0) | (ak ? 0x4000 : 0));
                out[b] |= (unsigned short)(af ? 0x4000 : 0x2000);
            } else if (af && !bf) {
                out[a] |= 0x2000;
                out[b] |= 0x4000;
            } else if (bf && !af) {
                out[b] |= 0x2000;
                out[a] |= 0x4000;
            }
        }
    return spanning;
}
static double bsp_splitter_score(int poly) {
    Poly *p = &g_shape.polys[poly];
    if (p->count < 3)
        return -1;
    Dot a = display_dot(p->index[0]), b = display_dot(p->index[1]), c = display_dot(p->index[2]);
    double ux = b.x - a.x, uy = b.y - a.y, uz = b.z - a.z, vx = c.x - a.x, vy = c.y - a.y, vz = c.z - a.z;
    double nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
    double score = sqrt(nx * nx + ny * ny + nz * nz);
    return score * ((p->type & 0x20) ? 25.0 : 1.0);
}
static int build_bsp_leaf_list(const int *items, int count) {
    int root = -1, tail = -1;
    for (int i = 0; i < count && g_bsp_count < MAX_POLYS; i++) {
        int node = g_bsp_count++;
        g_bsp_nodes[node] = (BSPNode){items[i], -1, -1, 1};
        g_bsp_coplanar_head[node] = -1;
        if (root < 0)
            root = node;
        else
            g_bsp_nodes[tail].front = node;
        tail = node;
    }
    return root;
}
static int build_bsp_leaf_list_ordered(const int *items, int count, const unsigned short *flags) {
    int ordered[MAX_POLYS], used = 0;
    unsigned char emitted[MAX_POLYS] = {0};
    for (int phase = 0; phase < 5; phase++)
        for (int i = 0; i < count; i++) {
            unsigned high = flags[i] & 0xf000;
            int match = phase == 0 ? high == 0x4000 : phase == 1 ? (flags[i] & 0x9000) == 0x1000
                                                  : phase == 2   ? high == 0x6000
                                                  : phase == 3   ? high == 0
                                                                 : high == 0x2000;
            if (match && !emitted[i]) {
                ordered[used++] = items[i];
                emitted[i] = 1;
            }
        }
    for (int i = 0; i < count; i++)
        if (!emitted[i])
            ordered[used++] = items[i];
    return build_bsp_leaf_list(ordered, used);
}
static int build_bsp_level(int *items, int count, int depth) {
    if (count <= 0 || g_bsp_count >= MAX_POLYS)
        return -1;
    unsigned short *flags = (unsigned short *)calloc((size_t)count, sizeof(*flags));
    if (!flags)
        return build_bsp_leaf_list(items, count);
    g_bsp_spanning += (int)bsp_relation_flags(items, count, flags);
    if (g_bsp_diag_mode == 2)
        for (int i = 0; i < count; i++)
            if ((flags[i] & 0x7000) == 0x7000)
                g_shape.polys[items[i]].selected = 1;
    if (g_bsp_diag_mode == 3)
        for (int i = 0; i < count; i++)
            if (flags[i] & 0x1000)
                g_shape.polys[items[i]].selected = 1;
    int best = -1;
    double bestscore = -1;
    for (int i = 0; i < count; i++)
        if ((flags[i] & 0xf000) == 0x6000) {
            double score = bsp_splitter_score(items[i]);
            if (score > bestscore) {
                bestscore = score;
                best = i;
            }
        }
    if (best < 0 || depth >= MAX_POLYS) {
        int leaf = build_bsp_leaf_list_ordered(items, count, flags);
        free(flags);
        return leaf;
    }
    free(flags);
    g_bsp_flat = 0;
    int splitter = items[best], node = g_bsp_count++;
    g_bsp_nodes[node] = (BSPNode){splitter, -1, -1, 0};
    g_bsp_coplanar_head[node] = -1;
    double plane[4];
    if (!polygon_plane((size_t)splitter, plane))
        return node;
    int *fronts = (int *)malloc((size_t)count * 2 * sizeof(int));
    if (!fronts)
        return node;
    int *backs = fronts + count, nf = 0, nb = 0;
    for (int i = 0; i < count; i++)
        if (i != best) {
            double center;
            int side = classify_poly((size_t)items[i], plane, &center);
            if (side == 2 || side == 0)
                side = center >= 0 ? 1 : -1;
            if (side > 0)
                fronts[nf++] = items[i];
            else
                backs[nb++] = items[i];
        }
    g_bsp_nodes[node].front = build_bsp_level(fronts, nf, depth + 1);
    g_bsp_nodes[node].back = build_bsp_level(backs, nb, depth + 1);
    free(fronts);
    return node;
}
static void build_bsp(void) {
    int *items = (int *)malloc(g_shape.poly_count * sizeof(int));
    if (!items)
        return;
    int n = 0;
    for (size_t i = 0; i < g_shape.poly_count; i++)
        if (g_shape.polys[i].count >= 3 && g_shape.polys[i].flags)
            items[n++] = (int)i;
    for (int i = 0; i < MAX_POLYS; i++) {
        g_bsp_coplanar_head[i] = -1;
        g_bsp_coplanar_next[i] = -1;
    }
    double saved_weight = g_plane_weight, max_radius = 0;
    for (size_t i = 0; i < g_shape.dot_count; i++) {
        Dot d = display_dot(i);
        double r = sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
        if (r > max_radius)
            max_radius = r;
    }
    g_plane_weight = max_radius / 30.0;
    g_bsp_count = 0;
    g_bsp_spanning = 0;
    g_bsp_flat = 1;
    g_bsp_root = build_bsp_level(items, n, 0);
    g_plane_weight = saved_weight;
    g_bsp_valid = 1;
    free(items);
}
static int begin_export_bsp(Shape *saved, Shape *working) {
    *saved = g_shape;
    if (!copy_shape(working, &g_shape))
        return 0;
    g_shape = *working;
    g_bsp_valid = 0;
    build_bsp();
    *working = g_shape;
    return 1;
}
static void end_export_bsp(Shape *saved, Shape *working) {
    *working = g_shape;
    g_shape = *saved;
    free_frames(working);
    g_bsp_valid = 0;
    g_bsp_root = -1;
    g_bsp_count = 0;
}
static void show_bsp_class(int mode) {
    for (size_t i = 0; i < g_shape.poly_count; i++)
        if (g_shape.polys[i].flags && g_shape.polys[i].count > 2)
            g_shape.polys[i].selected = 0;
    g_bsp_diag_mode = mode + 1;
    build_bsp();
    g_bsp_diag_mode = 0;
    if (mode == 0)
        for (int n = 0; n < g_bsp_count; n++)
            g_shape.polys[g_bsp_nodes[n].poly].selected = 1;
}
static int polygon_size_exponent(const Poly *p) {
    if (p->count < 3 || p->index[0] >= g_shape.dot_count || p->index[1] >= g_shape.dot_count || p->index[2] >= g_shape.dot_count)
        return 0;
    Dot a = display_dot(p->index[0]), b = display_dot(p->index[1]), c = display_dot(p->index[2]);
    double ux = b.x - a.x, uy = b.y - a.y, uz = b.z - a.z, vx = c.x - a.x, vy = c.y - a.y, vz = c.z - a.z;
    double nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
    double length = sqrt(nx * nx + ny * ny + nz * nz);
    int exponent = 0;
    frexp(length * length * p->count, &exponent);
    return exponent;
}
static int poly_sort_cmp(const void *a, const void *b) {
    const Poly *x = (const Poly *)a, *y = (const Poly *)b;
    if (!x->flags || !y->flags)
        return (int)(y->flags & 0xff) - (int)(x->flags & 0xff);
    int xc = (x->count == 2 ? -1 : 0) + ((x->type & 0x10) != 0), yc = (y->count == 2 ? -1 : 0) + ((y->type & 0x10) != 0);
    if (xc != yc)
        return yc - xc;
    return polygon_size_exponent(y) - polygon_size_exponent(x);
}
static void sort_polygons(void) {
    qsort(g_shape.polys, g_shape.poly_count, sizeof(Poly), poly_sort_cmp);
    statusf(L"Sorted %zu polygons", g_shape.poly_count);
}
static void selected_polys_to_end(void) {
    Poly *tmp = (Poly *)malloc(g_shape.poly_count * sizeof(Poly));
    if (!tmp)
        return;
    size_t w = 0;
    for (size_t i = 0; i < g_shape.poly_count; i++)
        if (!g_shape.polys[i].selected)
            tmp[w++] = g_shape.polys[i];
    for (size_t i = g_shape.poly_count; i > 0; i--)
        if (g_shape.polys[i - 1].selected)
            tmp[w++] = g_shape.polys[i - 1];
    memcpy(g_shape.polys, tmp, g_shape.poly_count * sizeof(Poly));
    free(tmp);
    statusf(L"Selected polygons draw last");
}
static void select_polys_by_vertices(int choice) {
    size_t n = 0;
    for (size_t i = 0; i < g_shape.poly_count; i++) {
        if (!g_shape.polys[i].flags) {
            g_shape.polys[i].selected = 0;
            continue;
        }
        unsigned count = g_shape.polys[i].count;
        int on = choice == 0 || (choice < 7 && count == (unsigned)choice) || (choice == 7 && count >= 7);
        g_shape.polys[i].selected = (uint8_t)on;
        n += on;
    }
    statusf(L"Selected %zu polygons", n);
}
static void sync_type_menu_from_selection(void) {
    /* TypeMenuUpdate stops at the first selected real polygon in slot order. */
    for (size_t i = 0; i < g_shape.poly_count; i++)
        if (g_shape.polys[i].selected && g_shape.polys[i].count > 1) {
            g_poly_type = (uint16_t)((g_poly_type & ~0x3fu) | (g_shape.polys[i].type & 0x3fu));
            return;
        }
}
static size_t select_selected_polygon_vertices(void) {
    size_t before = g_select_count;
    for (size_t i = 0; i < g_shape.poly_count; i++)
        if (g_shape.polys[i].selected && g_shape.polys[i].count > 1)
            for (unsigned j = 0; j < g_shape.polys[i].count; j++)
                set_dot_selected(g_shape.polys[i].index[j], 1);
    return g_select_count - before;
}
static int run_editor_callback_regression(const wchar_t *path) {
    g_snes_palette[6] = 0x7c00;
    g_palette_loaded = 1;
    int palette_ui_isolation = ega(6) == RGB(170, 85, 0) && snes_renderer_colour(6) == RGB(0, 0, 255);
    g_palette_loaded = 0;
    int palette_number_command = snes_palette_number_command(0) == 0xd0 && snes_palette_number_command(15) == 0xdf;
    free_frames(&g_shape);
    memset(&g_shape, 0, sizeof(g_shape));
    g_shape.poly_count = 2;
    g_shape.polys[0] = (Poly){.count = 3, .type = 2, .flags = 1, .selected = 1};
    g_shape.polys[1] = (Poly){.count = 3, .type = 32, .flags = 1, .selected = 1};
    g_poly_type = 0;
    sync_type_menu_from_selection();
    int type_first = g_poly_type == 2;
    memset(&g_shape, 0, sizeof(g_shape));
    g_shape.dot_count = 2;
    g_shape.poly_count = 1;
    g_shape.polys[0] = (Poly){.index = {0, 1}, .count = 2, .type = 4, .flags = 1, .selected = 1};
    g_poly_type = 0;
    sync_type_menu_from_selection();
    reverse_poly(&g_shape.polys[0]);
    int line_edit = g_poly_type == 4 && g_shape.polys[0].index[0] == 1 && g_shape.polys[0].index[1] == 0 && select_selected_polygon_vertices() == 2;
    clear_dot_selection(1);
    g_shape.dot_count = 3;
    g_shape.dots[0] = (Dot){1, 2, 3, 0};
    g_shape.dots[1] = (Dot){4, 5, 6, 0};
    g_shape.dots[2] = (Dot){7, 8, 9, 0};
    g_shape.poly_count = 1;
    g_shape.polys[0] = (Poly){.index = {0, 1, 2}, .count = 3, .flags = 1, .selected = 0};
    int mirror_ok = mirror_shape(0, 0, 0) && g_shape.polys[0].index[0] == 2 && g_shape.polys[0].index[1] == 1 && g_shape.polys[0].index[2] == 0 && g_shape.frames[0][0].x == 1 && g_shape.frames[0][0].y == 2 && g_shape.frames[0][0].z == 3;
    memset(g_mirror_options, 0, sizeof(g_mirror_options));
    g_mirror_options[3] = g_mirror_options[4] = 1;
    toggle_mirror_option(4);
    int mirror_coupling = !g_mirror_options[3] && !g_mirror_options[4];
    set_dot_selected(0, 1);
    int mirror_clone_selection = mirror_shape(1, 1, 1) && g_shape.dot_count == 4 && g_shape.dots[3].selected && (g_shape.frames[0][3].active & 0x0100) && g_select_count == 2 && g_select_order[1] == 3;
    g_shape.frames[1] = (FrameDot *)calloc(g_shape.dot_count, sizeof(FrameDot));
    int preview_spread = g_shape.frames[1] != NULL;
    if (preview_spread) {
        memcpy(g_shape.frames[1], g_shape.frames[0], g_shape.dot_count * sizeof(FrameDot));
        for (size_t i = 0; i < g_shape.dot_count; i++)
            g_shape.frames[1][i].active = 1;
        g_shape.frame_count = 2;
    }
    g_zoom = 2;
    g_preview_angles[0] = g_preview_angles[1] = g_preview_angles[2] = 0;
    g_preview_hidden = 1;
    g_current_frame = 0;
    enter_original_preview();
    if (preview_spread)
        preview_spread = g_shape.frames[1][0].active == g_shape.frames[0][0].active;
    double matrix[3][3];
    preview_matrix(matrix);
    POINT projected = preview_project((Dot){10, 20, 0, 0}, matrix);
    int preview_initial = g_preview_mode == 1 && g_preview_scale == 500 && g_preview_distance == 200 && projected.x == 25 && projected.y == 50;
    preview_key_down(VK_LEFT);
    preview_key_down(VK_UP);
    preview_key_down(VK_OEM_COMMA);
    int preview_rotate = fabs(g_preview_angles[0] - .1) < 1e-12 && fabs(g_preview_angles[1] - .1) < 1e-12 && fabs(g_preview_angles[2] - .1) < 1e-12;
    preview_key_down(VK_HOME);
    preview_key_down('H');
    int preview_reset = g_preview_angles[0] == 0 && g_preview_angles[1] == 0 && g_preview_angles[2] == 0 && !g_preview_hidden;
    g_shape.frame_count = 3;
    int show_all_order = show_all_frame_at(0) == 1 && show_all_frame_at(1) == 2 && show_all_frame_at(2) == 0;
    g_preview_frame = 0;
    preview_key_down('P');
    int preview_frames = g_preview_frame == 2;
    preview_key_down('N');
    preview_frames = preview_frames && g_preview_frame == 0;
    preview_key_down(VK_SPACE);
    int preview_exit = g_preview_mode == 0;
    free_frames(&g_shape);
    free_frames(&g_undo);
    memset(&g_shape, 0, sizeof(g_shape));
    memset(&g_undo, 0, sizeof(g_undo));
    g_has_undo = 0;
    g_shape.dot_count = 1;
    g_shape.frame_count = 1;
    g_shape.frames[0] = (FrameDot *)calloc(1, sizeof(FrameDot));
    g_shape.poly_count = 1;
    g_shape.polys[0] = (Poly){.index = {0}, .count = 1, .flags = 1};
    g_current_frame = 0;
    g_zoom = 1;
    g_origin[0] = g_origin[1] = g_origin[2] = 0;
    int one_vertex_render = g_shape.frames[0] && one_vertex_render_test();
    free_frames(&g_shape);
    free_frames(&g_undo);
    memset(&g_shape, 0, sizeof(g_shape));
    memset(&g_undo, 0, sizeof(g_undo));
    g_has_undo = 0;
    g_shape.dot_count = 1;
    g_shape.dots[0] = (Dot){1, 0, 0, 1};
    g_shape.frame_count = 2;
    g_shape.frames[0] = (FrameDot *)calloc(1, sizeof(FrameDot));
    g_shape.frames[1] = (FrameDot *)calloc(1, sizeof(FrameDot));
    int frame_actions = g_shape.frames[0] && g_shape.frames[1], frame_selection_remap = frame_actions;
    if (frame_actions) {
        g_shape.frames[0][0] = (FrameDot){1, 0, 0, 0x0101};
        g_shape.frames[1][0] = (FrameDot){2, 0, 0, 1};
        g_select_count = 1;
        g_select_order[0] = 0;
        g_current_frame = 1;
        remap_selection_to_current_frame();
        frame_selection_remap = g_shape.dots[0].selected && g_shape.frames[1][0].active == 0x0101 && g_select_count == 1;
        g_current_frame = 0;
        remap_selection_to_current_frame();
        frame_selection_remap = frame_selection_remap && g_shape.dots[0].selected;
        add_frames_count(2);
        frame_actions = g_shape.frame_count == 4 && g_current_frame == 1 && g_shape.frames[0][0].x == 1 && g_shape.frames[1][0].x == 1 && g_shape.frames[2][0].x == 1 && g_shape.frames[3][0].x == 2;
        delete_frames_count(2);
        frame_actions = frame_actions && g_shape.frame_count == 2 && g_current_frame == 0 && g_shape.frames[1][0].x == 2;
        copy_to_frame(1);
        frame_actions = frame_actions && g_shape.frames[1][0].x == 1;
        g_shape.frames[0][0].x = 10;
        g_shape.frames[1][0].x = 20;
        shift_animation();
        frame_actions = frame_actions && g_shape.frames[0][0].x == 20 && g_shape.frames[1][0].x == 10 && g_shape.dots[0].x == 20;
    }
    add_frames_count(MAX_FRAMES);
    int frame_capacity = g_shape.frame_count == MAX_FRAMES;
    add_frames_count(1);
    frame_capacity = frame_capacity && g_shape.frame_count == MAX_FRAMES;
    FILE *f = _wfopen(path, L"wb");
    if (!f)
        return 0;
    fprintf(f, "palette-ui-isolation=%d\npalette-number-command=%d\ntype-first=%d\nline-edit=%d\nmirror-zero-axis=%d\nmirror-option-coupling=%d\nmirror-clone-selection=%d\npreview-initial=%d\npreview-spread=%d\npreview-rotate=%d\npreview-reset=%d\npreview-frames=%d\npreview-exit=%d\nshow-all-order=%d\nrender-one-vertex=%d\nframe-selection-remap=%d\nframe-actions=%d\nframe-capacity=%d\n", palette_ui_isolation, palette_number_command, type_first, line_edit, mirror_ok, mirror_coupling, mirror_clone_selection, preview_initial, preview_spread, preview_rotate, preview_reset, preview_frames, preview_exit, show_all_order, one_vertex_render, frame_selection_remap, frame_actions, frame_capacity);
    int ok = fclose(f) == 0 && palette_ui_isolation && palette_number_command && type_first && line_edit && mirror_ok && mirror_coupling && mirror_clone_selection && preview_initial && preview_spread && preview_rotate && preview_reset && preview_frames && preview_exit && show_all_order && one_vertex_render && frame_selection_remap && frame_actions && frame_capacity;
    free_frames(&g_shape);
    free_frames(&g_undo);
    memset(&g_shape, 0, sizeof(g_shape));
    memset(&g_undo, 0, sizeof(g_undo));
    g_has_undo = 0;
    return ok;
}
static void run_frames_action(int index) {
    if (index == 0)
        command(ID_ANIM_NEXT);
    else if (index == 1)
        command(ID_ANIM_PREV);
    else if (index == 2)
        add_frames_count(1);
    else if (index == 3)
        delete_frames_count(1);
    else if (index == 4) {
        if (g_shape.frame_count < 2)
            statusf(L"Add a destination frame first");
        else
            copy_to_frame((g_current_frame + 1) % g_shape.frame_count);
    } else if (index == 5) {
        spread_current_frame_flags();
        g_show_all_frames = 1;
        statusf(L"Showing all %zu frames", g_shape.frame_count ? g_shape.frame_count : 1);
    }
}
static void commit_frames_edit(void) {
    g_frames_active_edit = -1;
    g_frames_replace_text = 0;
}
static int parse_frames_text(int row, int *out) {
    wchar_t *end;
    long value = wcstol(g_frames_text[row - 2], &end, 10);
    while (*end == L' ' || *end == L'\t')
        end++;
    if (end == g_frames_text[row - 2] || *end || value < 0 || value > 32767)
        return 0;
    *out = (int)value;
    return 1;
}
static void finish_frames_prompt(int accept) {
    commit_frames_edit();
    if (!accept) {
        g_dos_prompt = PROMPT_NONE;
        statusf(L"Ready");
        InvalidateRect(g_hwnd, NULL, FALSE);
        return;
    }
    int action = g_frames_action, value = 0;
    if (action >= 2 && action <= 4 && !parse_frames_text(action, &value)) {
        g_frames_active_edit = action;
        g_frames_replace_text = 1;
        statusf(L"Enter a frame number");
        MessageBeep(MB_ICONWARNING);
        InvalidateRect(g_hwnd, NULL, FALSE);
        return;
    }
    g_dos_prompt = PROMPT_NONE;
    if (action == 0 || action == 1)
        run_frames_action(action);
    else if (action == 2) {
        if (value > 0)
            add_frames_count((size_t)value);
        else
            statusf(L"Frame %zu of %zu", g_current_frame + 1, g_shape.frame_count ? g_shape.frame_count : 1);
    } else if (action == 3) {
        if (value > 0)
            delete_frames_count((size_t)value);
        else
            statusf(L"Frame %zu of %zu", g_current_frame + 1, g_shape.frame_count ? g_shape.frame_count : 1);
    } else if (action == 4) {
        if (value > 1 && (size_t)value <= g_shape.frame_count)
            copy_to_frame((size_t)value - 1);
        else
            statusf(L"Choose another destination frame");
    } else
        run_frames_action(5);
    InvalidateRect(g_hwnd, NULL, FALSE);
}
static void run_menu_item(int menu, int index) {
    static const double planes[] = {.001, .01, .1, .5, 1, 2, 5, 10, 100};
    static const double grids[] = {1, 2, 5, 10, 15, 20, 30, 50, 100, 150, 200, 300, 500, 1000, 2000};
    if (menu == MENU_PLANE) {
        g_plane_weight = planes[index];
        statusf(L"Plane weight %.3g", g_plane_weight);
    } else if (menu == MENU_GRID) {
        g_grid = grids[index];
        statusf(L"Grid %.0f", g_grid);
    } else if (menu == MENU_GROUP) {
        if (index == 1) {
            g_active_menu = MENU_SHOWGROUP;
            return;
        }
        if (index == 2) {
            g_active_menu = MENU_SETGROUP;
            return;
        }
        size_t changed = 0;
        for (size_t i = 0; i < g_shape.poly_count; i++)
            if (g_shape.polys[i].selected) {
                g_shape.polys[i].flags = g_current_group;
                changed++;
            }
        int group = 1;
        for (uint16_t bit = g_current_group; bit > 1; bit >>= 1)
            group++;
        statusf(L"Regrouped %zu polygons into group %d", changed, group);
    } else if (menu == MENU_POLY) {
        if (index == 0)
            add_polygon_from_selected();
        else if (index == 1)
            select_best_polygon_from_dots(1);
        else if (index == 2)
            select_best_polygon_from_dots(0);
        else if (index == 3) {
            sync_type_menu_from_selection();
            g_active_menu = MENU_TYPE;
            return;
        } else if (index == 4)
            select_poly_step(-1);
        else if (index == 5)
            select_poly_step(1);
        else if (index == 6)
            select_polygons_dialog();
        else if (index == 7 || index == 8) {
            if (index == 7)
                snapshot();
            for (size_t i = 0; i < g_shape.poly_count; i++)
                if (g_shape.polys[i].selected && g_shape.polys[i].count > 1) {
                    Poly *p = &g_shape.polys[i];
                    if (index == 7)
                        reverse_poly(p);
                    else {
                        uint16_t t = p->index[0];
                        memmove(p->index, p->index + 1, (p->count - 1) * sizeof(uint16_t));
                        p->index[p->count - 1] = t;
                    }
                }
            statusf(index == 7 ? L"Flipped polygons" : L"Rotated polygon vertices");
        } else if (index == 9)
            delete_selected_polys();
        else if (index == 10)
            choose_default_colour();
        else if (index == 11)
            colour_selected_polygons();
        else if (index == 12)
            sort_polygons();
        else if (index == 13)
            selected_polys_to_end();
        else if (index == 14) {
            select_selected_polygon_vertices();
            statusf(L"Selected polygon vertices");
        }
    } else if (menu == MENU_ZOOM) {
        if (index == 0)
            g_zoom /= 2.0;
        else if (index == 1)
            g_zoom *= 2.0;
        else
            auto_zoom();
    } else if (menu == MENU_SAVE) {
        if (index == 0)
            command(ID_FILE_SAVE_INTERNAL);
        else if (index == 1)
            command(ID_FILE_SAVE_INTERNAL_AS);
        else if (index == 2)
            command(ID_FILE_SAVE);
        else
            save_assembler(index - 3);
    } else if (menu == MENU_ANIM) {
        if (index == 0)
            frames_dialog();
        else if (index == 1)
            load_key_animation();
        else if (index == 2)
            shift_animation();
        else if (index == 3)
            command(ID_ANIM_NEXT);
        else if (index == 4)
            command(ID_ANIM_PREV);
        else if (index == 5)
            add_frame();
    } else if (menu == MENU_TYPE) {
        if (index > 0) {
            uint16_t bit = (uint16_t)(1u << index);
            g_poly_type ^= bit;
            for (size_t i = 0; i < g_shape.poly_count; i++)
                if (g_shape.polys[i].selected && g_shape.polys[i].count > 1) {
                    if (g_poly_type & bit)
                        g_shape.polys[i].type |= bit;
                    else
                        g_shape.polys[i].type &= (uint16_t)~bit;
                }
            statusf(L"Polygon type 0x%X", g_poly_type);
        }
    } else if (menu == MENU_SETGROUP) {
        uint16_t bit = (uint16_t)(1u << index);
        g_current_group = bit;
        g_shape_display_mask = bit;
        statusf(L"Current group %d", index + 1);
    } else if (menu == MENU_SHOWGROUP) {
        g_shape_display_mask = index ? 1u << (index - 1) : 255;
        statusf(index ? L"Displaying group %d" : L"Displaying all groups", index);
    } else if (menu == MENU_SHOW) {
        if (index == 0) {
            enter_original_preview();
            statusf(L"Preview");
        } else {
            g_preview_mode = 2;
            statusf(L"3D system preview");
        }
    } else if (menu == MENU_TEST) {
        if (index == 0) {
            g_active_menu = MENU_BSPTEST;
            return;
        } else
            twist_test();
    } else if (menu == MENU_FRAMES)
        run_frames_action(index);
    else if (menu == MENU_BSPTEST)
        show_bsp_class(index);
    else if (menu == MENU_SNES) {
        if (index == 0)
            send_snes_stream();
        else if (index == 1) {
            open_colour_table_menu();
            return;
        } else if (index == 2) {
            if (ensure_colour_tables() && g_palette_count) {
                g_active_menu = MENU_PALETTE;
                return;
            }
        } else if (index == 3) {
            g_active_menu = MENU_PALNUM;
            return;
        } else if (index == 4) {
            if (ensure_colour_tables() && g_texture_count) {
                g_active_menu = MENU_TEXTURE;
                return;
            }
        } else {
            g_bsp_debug ^= 1;
            statusf(L"BSP DEBUG %ls", g_bsp_debug ? L"ON" : L"OFF");
        }
    } else if (menu == MENU_PALNUM) {
        g_palette_number = index;
        statusf(L"Palette number %X (command 0x%X)", index, snes_palette_number_command(index));
    } else if (menu == MENU_COLTAB)
        build_colour_table_source(index);
    else if (menu == MENU_PALETTE)
        load_snes_palette(index);
    else if (menu == MENU_TEXTURE)
        choose_texture(index);
    else if (menu == MENU_SELECTBY)
        select_polys_by_vertices(index);
    g_active_menu = MENU_NONE;
    InvalidateRect(g_hwnd, NULL, FALSE);
}

static void select_near_dot(const View *v, int x, int y, int deselect) {
    double point[3] = {g_cursor_coords[0], g_cursor_coords[1], g_cursor_coords[2]};
    (void)v;
    (void)x;
    (void)y;
    double best = DBL_MAX, total = 0;
    size_t candidate = (size_t)-1, count = 0;
    for (size_t i = 0; i < g_shape.dot_count; i++)
        if (dot_active(i) && g_shape.dots[i].selected == (uint8_t)deselect) {
            Dot d = display_dot(i);
            double dx = d.x - point[0], dy = d.y - point[1], dz = d.z - point[2], distance = dx * dx + dy * dy + dz * dz;
            total += distance;
            count++;
            if (distance < best) {
                best = distance;
                candidate = i;
            }
        }
    if (candidate != (size_t)-1 && (count == 1 || best * 3.0 < total / count)) {
        set_dot_selected(candidate, !deselect);
        statusf(L" Selected %zu", g_select_count);
    }
}

static void deselect_near(int x, int y) {
    RECT c;
    GetClientRect(g_hwnd, &c);
    POINT click = {x, y};
    View v[4];
    make_views(c, v);
    for (int k = 0; k < 3; k++)
        if (PtInRect(&v[k].r, click)) {
            update_cursor_coords(x, y);
            select_near_dot(&v[k], x, y, 1);
            InvalidateRect(g_hwnd, NULL, FALSE);
            break;
        }
}

static void select_near(int x, int y) {
    if (g_file_selector) {
        handle_file_selector_click(x, y);
        return;
    }
    if (g_preview_mode == 1)
        return;
    if (g_show_all_frames)
        g_show_all_frames = 0;
    if (g_dos_prompt) {
        RECT c;
        GetClientRect(g_hwnd, &c);
        int lx = c.right ? x * 640 / c.right : 0, ly = c.bottom ? y * 480 / c.bottom : 0;
        if (g_dos_prompt == PROMPT_NUMBER) {
            if (lx >= 50 && lx < 370 && ly >= 96 && ly < 122)
                finish_number_prompt(lx < 210);
            else if (lx >= 50 && lx < 370 && ly >= 72 && ly < 96) {
                g_number_active_edit = 1;
                g_number_replace_text = 1;
                InvalidateRect(g_hwnd, NULL, FALSE);
            }
            return;
        }
        if (g_dos_prompt == PROMPT_SELECT_POLYS) {
            if (lx >= 50 && lx < 370 && ly >= 336 && ly < 362)
                finish_selectpolys_prompt(lx < 210);
            else if (lx >= 50 && lx < 370 && ly >= 72 && ly < 336) {
                int option = (ly - 72) / 24;
                if (option >= 0 && option < 11) {
                    g_selectpolys_action = option;
                    InvalidateRect(g_hwnd, NULL, FALSE);
                }
            }
            return;
        }
        if (g_dos_prompt == PROMPT_FRAMES) {
            if (lx >= 50 && lx < 370 && ly >= 216 && ly < 242)
                finish_frames_prompt(lx < 210);
            else if (lx >= 50 && lx < 370 && ly >= 72 && ly < 216) {
                int option = (ly - 72) / 24;
                if (option >= 0 && option < 6) {
                    commit_frames_edit();
                    g_frames_action = option;
                    if (option >= 2 && option <= 4) {
                        g_frames_active_edit = option;
                        g_frames_replace_text = 1;
                    }
                    InvalidateRect(g_hwnd, NULL, FALSE);
                }
            }
            return;
        }
        if (g_dos_prompt == PROMPT_SCALE) {
            if (lx >= 50 && lx < 370 && ly >= 216 && ly < 242)
                finish_scale_prompt(lx < 210);
            else if (lx >= 50 && lx < 370 && ly >= 72 && ly < 216) {
                int option = (ly - 72) / 24;
                if (option >= 0 && option < 6) {
                    commit_scale_edit();
                    if (option < 4) {
                        g_scale_active_edit = option;
                        g_scale_replace_text = 1;
                    } else
                        g_scale_options[option - 4] ^= 1;
                    InvalidateRect(g_hwnd, NULL, FALSE);
                }
            }
            return;
        }
        if (g_dos_prompt == PROMPT_MIRROR) {
            if (lx >= 50 && lx < 370 && ly >= 192 && ly < 218) {
                int accept = lx < 210;
                g_dos_prompt = PROMPT_NONE;
                if (accept)
                    mirror_shape((g_mirror_options[0] ? 1 : 0) | (g_mirror_options[1] ? 2 : 0) | (g_mirror_options[2] ? 4 : 0), g_mirror_options[3], g_mirror_options[4]);
                else
                    statusf(L"Ready");
                InvalidateRect(g_hwnd, NULL, FALSE);
            } else if (lx >= 50 && lx < 370 && ly >= 72 && ly < 192) {
                int option = (ly - 72) / 24;
                if (option >= 0 && option < 5) {
                    toggle_mirror_option(option);
                    InvalidateRect(g_hwnd, NULL, FALSE);
                }
            }
            return;
        }
        int x0 = c.right * 50 / 640, x1 = c.right * 210 / 640, x2 = c.right * 370 / 640, y1 = c.bottom * 72 / 480, y2 = c.bottom * 98 / 480;
        if (y >= y1 && y < y2 && x >= x0 && x < x2) {
            int left = x < x1, prompt = g_dos_prompt;
            if (prompt == PROMPT_QUIT) {
                g_dos_prompt = PROMPT_NONE;
                if (left)
                    DestroyWindow(g_hwnd);
                else {
                    statusf(L"Ready");
                    InvalidateRect(g_hwnd, NULL, FALSE);
                }
            } else if (prompt == PROMPT_NEW) {
                g_dos_prompt = PROMPT_NONE;
                if (left) {
                    snapshot();
                    free_frames(&g_shape);
                    memset(&g_shape, 0, sizeof(g_shape));
                    g_select_count = 0;
                    g_current_frame = 0;
                    g_path[0] = 0;
                    statusf(L"New shape");
                } else
                    statusf(L"Ready");
                InvalidateRect(g_hwnd, NULL, FALSE);
            } else
                finish_rotate_choice(left);
        }
        return;
    }
    RECT c;
    GetClientRect(g_hwnd, &c);
    POINT click = {x, y};
    if (g_active_menu) {
        int count;
        const wchar_t *title;
        menu_labels(g_active_menu, &count, &title);
        for (int i = 0; i < count; i++) {
            RECT r = submenu_rect(c, i, count);
            if (PtInRect(&r, click)) {
                if (flags_enabled(submenu_flags(g_active_menu, i)))
                    run_menu_item(g_active_menu, i);
                return;
            }
        }
    }
    for (int i = 0; i < 24; i++) {
        RECT r = button_rect(c, i);
        if (PtInRect(&r, click)) {
            if (!main_button_enabled(i))
                return;
            int command_id = g_main_buttons[i].command;
            if (command_id == -1) {
                g_dos_prompt = PROMPT_NEW;
                statusf(L"Delete all");
                InvalidateRect(g_hwnd, NULL, FALSE);
            } else if (command_id <= -100) {
                g_active_menu = -command_id - 100;
                statusf(L"%ls", g_main_buttons[i].label);
            } else if (command_id)
                command(command_id);
            else
                statusf(L"%ls", g_main_buttons[i].label);
            return;
        }
    }
    View v[4];
    make_views(c, v);
    for (int k = 0; k < 3; k++)
        if (PtInRect(&v[k].r, click)) {
            update_cursor_coords(x, y);
            if (g_tool) {
                begin_tool_drag(x, y, k);
                return;
            }
            if (g_add_dot_mode)
                add_dot_at(g_cursor_coords[0], g_cursor_coords[1], g_cursor_coords[2]);
            else
                select_near_dot(&v[k], x, y, 0);
            break;
        }
}

static void command(int id) {
    g_show_all_frames = 0;
    if (id == ID_FILE_OPEN) {
        wchar_t p[MAX_PATH] = L"";
        if (choose_file(0, L"Load Shape", p)) {
            if (load_shape(p))
                statusf(L"Loaded %zu dots, %zu polygons", g_shape.dot_count, g_shape.poly_count);
            else if (g_last_load_opened)
                statusf(L"Alien file format");
            else
                statusf(L"File <%ls>?", p);
        }
    } else if (id == ID_FILE_SAVE) {
        if (g_shape.frame_count > 1) {
            statusf(L"Can't save Anim as M3d");
            return;
        }
        if (!shape_is_compacted()) {
            statusf(L"Shape Not Compacted");
            return;
        }
        wchar_t p[MAX_PATH] = L"";
        if (choose_file(1, L"Save M3d", p)) {
            if (save_shape(p)) {
                wcscpy(g_path, p);
                statusf(L"Saved %zu dots, %zu polygons", g_shape.dot_count, g_shape.poly_count);
            } else
                statusf(L"File <%ls>?", p);
        }
    } else if (id == ID_FILE_SAVE_INTERNAL) {
        if (!g_path[0])
            return;
        if (!shape_is_compacted()) {
            statusf(L"Shape Not Compacted");
            return;
        }
        if (save_internal(g_path))
            statusf(L"Saved internal: %zu frames", g_shape.frame_count ? g_shape.frame_count : 1);
        else
            statusf(L"File <%ls>?", g_path);
    } else if (id == ID_FILE_SAVE_INTERNAL_AS) {
        if (!shape_is_compacted()) {
            statusf(L"Shape Not Compacted");
            return;
        }
        wchar_t p[MAX_PATH] = L"";
        if (choose_file(1, L"Save Internal", p)) {
            if (save_internal(p))
                statusf(L"Saved internal: %zu frames", g_shape.frame_count ? g_shape.frame_count : 1);
            else
                statusf(L"File <%ls>?", p);
        }
    } else if (id == ID_FILE_EXIT) {
        g_dos_prompt = PROMPT_QUIT;
        statusf(L"QUIT to OS");
    } else if (id == ID_EDIT_UNDO && g_has_undo) {
        Shape t = g_shape;
        g_shape = g_undo;
        g_undo = t;
        size_t count = g_select_count;
        g_select_count = g_undo_select_count;
        g_undo_select_count = count;
        uint16_t order[MAX_DOTS];
        memcpy(order, g_select_order, count * sizeof(order[0]));
        memcpy(g_select_order, g_undo_select_order, g_select_count * sizeof(g_select_order[0]));
        memcpy(g_undo_select_order, order, count * sizeof(order[0]));
        statusf(L"Undo");
    } else if (id == ID_EDIT_SELECT_ALL || id == ID_EDIT_SELECT_NONE) {
        int on = id == ID_EDIT_SELECT_ALL;
        if (on)
            g_select_count = 0;
        snapshot();
        clear_dot_selection(1);
        if (on)
            for (size_t i = 0; i < g_shape.dot_count; i++)
                if (dot_active(i))
                    set_dot_selected(i, 1);
        statusf(L"Selected %zu dots", g_select_count);
    } else if (id == ID_EDIT_DELETE)
        delete_selected();
    else if (id == ID_DOT_MODE) {
        g_add_dot_mode ^= 1;
        statusf(g_add_dot_mode ? L"Add Dot" : L"Select");
    } else if (id == ID_EDIT_SIZE)
        scale_shape_dialog();
    else if (id == ID_EDIT_MOVE || id == ID_EDIT_COPY || id == ID_EDIT_ROTATE) {
        g_tool = id;
        g_rotate_stage = 0;
        statusf(id == ID_EDIT_MOVE ? L"Drag mouse to move" : id == ID_EDIT_COPY ? L"Drag mouse to copy"
                                                                                : L"Click on centre");
    } else if (id == ID_EDIT_COMPACT)
        compact_shape();
    else if (id == ID_SHAPE_ADD_DOT)
        add_dot_at_origin();
    else if (id == ID_SHAPE_MIRROR_DIALOG)
        mirror_dialog();
    else if (id >= ID_SHAPE_MIRROR_X && id <= ID_SHAPE_MIRROR_Z)
        mirror_shape(1 << (id - ID_SHAPE_MIRROR_X), selected_dot_count() != 0, 0);
    else if (id == ID_VIEW_GRID_1)
        g_grid = 1;
    else if (id == ID_VIEW_GRID_10)
        g_grid = 10;
    else if (id == ID_VIEW_GRID_100)
        g_grid = 100;
    else if (id == ID_VIEW_ZOOM_IN)
        g_zoom /= 2.0;
    else if (id == ID_VIEW_ZOOM_OUT)
        g_zoom *= 2.0;
    else if ((id == ID_ANIM_PREV || id == ID_ANIM_NEXT) && g_shape.frame_count) {
        if (id == ID_ANIM_NEXT)
            g_current_frame = (g_current_frame + 1) % g_shape.frame_count;
        else
            g_current_frame = (g_current_frame + g_shape.frame_count - 1) % g_shape.frame_count;
        remap_selection_to_current_frame();
        statusf(L"Frame %llu of %llu", (unsigned long long)(g_current_frame + 1), (unsigned long long)g_shape.frame_count);
    }
    InvalidateRect(g_hwnd, NULL, FALSE);
}

static int run_keyboard_shortcut(WPARAM key) {
    if (key == VK_LEFT || key == VK_RIGHT || key == VK_UP || key == VK_DOWN || key == VK_HOME)
        return 0;
    if (key == VK_SPACE) {
        if (flags_enabled(g_main_button_flags[7]))
            command(ID_EDIT_SELECT_NONE);
        return 1;
    }
    if (key == VK_DELETE) {
        if (flags_enabled(g_main_button_flags[9]))
            command(ID_EDIT_DELETE);
        return 1;
    }
    if (key == 'M') {
        if (flags_enabled(g_main_button_flags[5]))
            command(ID_EDIT_MOVE);
        return 1;
    }
    if (key == 'A') {
        if (flags_enabled(g_main_button_flags[8]))
            command(ID_EDIT_SELECT_ALL);
        return 1;
    }
    if (key == 'Z') {
        if (flags_enabled(g_main_button_flags[10]))
            command(ID_EDIT_SIZE);
        return 1;
    }
    if (key == 'L') {
        command(ID_FILE_OPEN);
        return 1;
    }
    if (key == 'U') {
        command(ID_EDIT_UNDO);
        return 1;
    }
    if (key == 'X') {
        g_dos_prompt = PROMPT_NEW;
        statusf(L"Delete all");
        return 1;
    }
    if (key == 'Q') {
        command(ID_FILE_EXIT);
        return 1;
    }
    if (key == 'G') {
        if (flags_enabled(g_group_flags[0]))
            run_menu_item(MENU_GROUP, 0);
        return 1;
    }
    if (key == 'P') {
        if (flags_enabled(g_poly_flags[0]))
            run_menu_item(MENU_POLY, 0);
        return 1;
    }
    if (key == 'S') {
        if (flags_enabled(g_poly_flags[1]))
            run_menu_item(MENU_POLY, 1);
        return 1;
    }
    if (key == 'V') {
        if (flags_enabled(g_poly_flags[6]))
            run_menu_item(MENU_POLY, 6);
        return 1;
    }
    if (key == 'F') {
        if (flags_enabled(g_poly_flags[7]))
            run_menu_item(MENU_POLY, 7);
        return 1;
    }
    if (key == 'O') {
        if (flags_enabled(g_poly_flags[8]))
            run_menu_item(MENU_POLY, 8);
        return 1;
    }
    if (key == 'D') {
        if (flags_enabled(g_poly_flags[9]))
            run_menu_item(MENU_POLY, 9);
        return 1;
    }
    if (key == 'C') {
        if (flags_enabled(g_poly_flags[11]))
            run_menu_item(MENU_POLY, 11);
        return 1;
    }
    if (key == 'E') {
        if (flags_enabled(g_poly_flags[13]))
            run_menu_item(MENU_POLY, 13);
        return 1;
    }
    if (key == '9') {
        if (flags_enabled(g_poly_flags[14]))
            run_menu_item(MENU_POLY, 14);
        return 1;
    }
    if (key == 'W') {
        if (flags_enabled(g_save_flags[0]))
            run_menu_item(MENU_SAVE, 0);
        return 1;
    }
    if (key == 'N') {
        run_menu_item(MENU_ANIM, 5);
        return 1;
    }
    if (key == VK_F9) {
        if (flags_enabled(g_poly_flags[4]))
            run_menu_item(MENU_POLY, 4);
        return 1;
    }
    if (key == VK_F10) {
        if (flags_enabled(g_poly_flags[5]))
            run_menu_item(MENU_POLY, 5);
        return 1;
    }
    if (key >= VK_F1 && key <= VK_F8) {
        run_menu_item(MENU_SETGROUP, (int)(key - VK_F1));
        return 1;
    }
    if (key >= '1' && key <= '8' && !(GetKeyState(VK_SHIFT) & 0x8000)) {
        run_menu_item(MENU_SHOWGROUP, (int)(key - '0'));
        return 1;
    }
    if (key == VK_NEXT) {
        run_menu_item(MENU_ZOOM, 0);
        return 1;
    }
    if (key == VK_PRIOR) {
        run_menu_item(MENU_ZOOM, 1);
        return 1;
    }
    return 0;
}
static int run_character_shortcut(wchar_t ch) {
    if (ch == L'.') {
        command(ID_DOT_MODE);
        return 1;
    }
    if (ch == L'=') {
        if (flags_enabled(g_save_flags[5]))
            run_menu_item(MENU_SAVE, 5);
        return 1;
    }
    if (ch == L']') {
        if (flags_enabled(g_anim_flags[3]))
            run_menu_item(MENU_ANIM, 3);
        return 1;
    }
    if (ch == L'[') {
        if (flags_enabled(g_anim_flags[4]))
            run_menu_item(MENU_ANIM, 4);
        return 1;
    }
    if (ch == L'*') {
        run_menu_item(MENU_ZOOM, 2);
        return 1;
    }
    if (ch == L'#') {
        if (flags_enabled(g_show_flags[0]))
            run_menu_item(MENU_SHOW, 0);
        return 1;
    }
    return 0;
}

static LRESULT CALLBACK wndproc(HWND w, UINT m, WPARAM wp, LPARAM lp) {
    switch (m) {
    case WM_PAINT:
        paint(w);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_SIZE:
        InvalidateRect(w, NULL, FALSE);
        return 0;
    case WM_LBUTTONDOWN:
        select_near(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        return 0;
    case WM_RBUTTONDOWN:
        if (g_preview_mode != 1 && !g_file_selector && !g_dos_prompt && !g_active_menu && !g_tool && !g_add_dot_mode)
            deselect_near(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        return 0;
    case WM_MOUSEMOVE:
        if (g_preview_mode == 1)
            return 0;
        if (g_dragging)
            update_tool_drag(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        else if (!g_file_selector && !g_dos_prompt) {
            update_cursor_coords(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            KillTimer(w, 1);
            SetTimer(w, 1, 75, NULL);
        }
        return 0;
    case WM_TIMER:
        if (wp == 1) {
            KillTimer(w, 1);
            g_cursor_overlay = 1;
            RECT c;
            GetClientRect(w, &c);
            RECT r = {c.right * 284 / 640, c.bottom * 399 / 480, c.right * 541 / 640, c.bottom * 416 / 480};
            InvalidateRect(w, &r, FALSE);
            return 0;
        }
        break;
    case WM_LBUTTONUP:
        if (g_tool == ID_EDIT_ROTATE && g_rotate_stage == 1) {
            g_rotate_stage = 2;
            statusf(L"Drag angle");
            InvalidateRect(w, NULL, FALSE);
        } else if (g_dragging) {
            update_tool_drag(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            if (g_tool == ID_EDIT_ROTATE)
                finish_rotate();
            else {
                g_dragging = 0;
                g_tool = 0;
                ReleaseCapture();
                g_bsp_valid = 0;
                statusf(L"Transform complete");
            }
        }
        return 0;
    case WM_CLOSE:
        if (g_file_selector) {
            g_file_result = 0;
            return 0;
        }
        command(ID_FILE_EXIT);
        return 0;
    case WM_COMMAND:
        command(LOWORD(wp));
        return 0;
    case WM_KEYDOWN:
        if (g_file_selector) {
            if (wp == VK_ESCAPE)
                g_file_result = 0;
            else if (wp == VK_RETURN) {
                if (g_file_active_edit == 1) {
                    wchar_t path[MAX_PATH];
                    wcscpy(path, g_file_directory);
                    g_file_active_edit = 0;
                    g_file_replace_text = 0;
                    set_file_directory(path);
                    InvalidateRect(w, NULL, FALSE);
                } else if (g_file_active_edit == 2) {
                    g_file_active_edit = 0;
                    g_file_replace_text = 0;
                    InvalidateRect(w, NULL, FALSE);
                } else
                    accept_file_selector();
            }
            return 0;
        }
        if (g_preview_mode == 1) {
            if (preview_key_down(wp))
                InvalidateRect(w, NULL, FALSE);
            return 0;
        }
        g_show_all_frames = 0;
        if (wp == VK_ESCAPE) {
            if (g_dos_prompt == PROMPT_ROTATE_ADD) {
                finish_rotate_choice(0);
                return 0;
            }
            if (g_dos_prompt == PROMPT_QUIT || g_dos_prompt == PROMPT_NEW || g_dos_prompt == PROMPT_MIRROR || g_dos_prompt == PROMPT_SCALE || g_dos_prompt == PROMPT_FRAMES || g_dos_prompt == PROMPT_SELECT_POLYS || g_dos_prompt == PROMPT_NUMBER) {
                g_dos_prompt = PROMPT_NONE;
                g_scale_active_edit = -1;
                g_frames_active_edit = -1;
                g_number_active_edit = 0;
                statusf(L"Ready");
                InvalidateRect(w, NULL, FALSE);
                return 0;
            }
            if ((g_dragging || g_rotate_stage) && g_has_undo)
                restore_undo_copy();
            g_dragging = 0;
            g_rotate_stage = 0;
            g_tool = 0;
            g_active_menu = 0;
            ReleaseCapture();
            statusf(L"Cancelled");
            InvalidateRect(w, NULL, FALSE);
        } else if (wp == VK_RETURN && g_dos_prompt == PROMPT_SCALE) {
            if (g_scale_active_edit >= 0) {
                commit_scale_edit();
                InvalidateRect(w, NULL, FALSE);
            } else
                finish_scale_prompt(1);
        } else if (wp == VK_RETURN && g_dos_prompt == PROMPT_FRAMES) {
            if (g_frames_active_edit >= 0) {
                commit_frames_edit();
                InvalidateRect(w, NULL, FALSE);
            } else
                finish_frames_prompt(1);
        } else if (wp == VK_RETURN && g_dos_prompt == PROMPT_SELECT_POLYS)
            finish_selectpolys_prompt(1);
        else if (wp == VK_RETURN && g_dos_prompt == PROMPT_NUMBER) {
            if (g_number_active_edit) {
                g_number_active_edit = 0;
                g_number_replace_text = 0;
                InvalidateRect(w, NULL, FALSE);
            } else
                finish_number_prompt(1);
        } else if (g_dos_prompt)
            return 0;
        else if (run_keyboard_shortcut(wp))
            return 0;
        else if (wp == VK_LEFT || wp == VK_RIGHT || wp == VK_UP || wp == VK_DOWN || wp == VK_HOME)
            move_origin_key(wp);
        return 0;
    case WM_CHAR:
        if (g_preview_mode == 1)
            return 0;
        if (g_file_selector && g_file_active_edit) {
            wchar_t *text = g_file_active_edit == 1 ? g_file_directory : g_file_name;
            if (wp == 8) {
                if (g_file_replace_text) {
                    text[0] = 0;
                    g_file_replace_text = 0;
                } else {
                    size_t n = wcslen(text);
                    if (n)
                        text[n - 1] = 0;
                }
            } else if (wp >= 32 && wp < 127) {
                if (g_file_replace_text) {
                    text[0] = 0;
                    g_file_replace_text = 0;
                }
                size_t n = wcslen(text);
                if (n < MAX_PATH - 1) {
                    text[n] = (wchar_t)wp;
                    text[n + 1] = 0;
                }
            }
            InvalidateRect(w, NULL, FALSE);
            return 0;
        }
        if (g_dos_prompt == PROMPT_SCALE && g_scale_active_edit >= 0) {
            int i = g_scale_active_edit;
            if (wp == 8) {
                if (g_scale_replace_text) {
                    g_scale_text[i][0] = 0;
                    g_scale_replace_text = 0;
                } else {
                    size_t n = wcslen(g_scale_text[i]);
                    if (n)
                        g_scale_text[i][n - 1] = 0;
                }
            } else if (wp >= 32 && wp < 127 && wcschr(L"0123456789+-.eE ", (wchar_t)wp)) {
                if (g_scale_replace_text) {
                    g_scale_text[i][0] = 0;
                    g_scale_replace_text = 0;
                }
                size_t n = wcslen(g_scale_text[i]);
                if (n < 18) {
                    g_scale_text[i][n] = (wchar_t)wp;
                    g_scale_text[i][n + 1] = 0;
                }
            }
            InvalidateRect(w, NULL, FALSE);
            return 0;
        }
        if (g_dos_prompt == PROMPT_FRAMES && g_frames_active_edit >= 2 && g_frames_active_edit <= 4) {
            int i = g_frames_active_edit - 2;
            if (wp == 8) {
                if (g_frames_replace_text) {
                    g_frames_text[i][0] = 0;
                    g_frames_replace_text = 0;
                } else {
                    size_t n = wcslen(g_frames_text[i]);
                    if (n)
                        g_frames_text[i][n - 1] = 0;
                }
            } else if (wp >= 32 && wp < 127 && wcschr(L"0123456789 ", (wchar_t)wp)) {
                if (g_frames_replace_text) {
                    g_frames_text[i][0] = 0;
                    g_frames_replace_text = 0;
                }
                size_t n = wcslen(g_frames_text[i]);
                if (n < 18) {
                    g_frames_text[i][n] = (wchar_t)wp;
                    g_frames_text[i][n + 1] = 0;
                }
            }
            InvalidateRect(w, NULL, FALSE);
            return 0;
        }
        if (g_dos_prompt == PROMPT_NUMBER && g_number_active_edit) {
            if (wp == 8) {
                if (g_number_replace_text) {
                    g_number_text[0] = 0;
                    g_number_replace_text = 0;
                } else {
                    size_t n = wcslen(g_number_text);
                    if (n)
                        g_number_text[n - 1] = 0;
                }
            } else if (wp >= 32 && wp < 127 && wcschr(L"0123456789 ", (wchar_t)wp)) {
                if (g_number_replace_text) {
                    g_number_text[0] = 0;
                    g_number_replace_text = 0;
                }
                size_t n = wcslen(g_number_text);
                if (n < 18) {
                    g_number_text[n] = (wchar_t)wp;
                    g_number_text[n + 1] = 0;
                }
            }
            InvalidateRect(w, NULL, FALSE);
            return 0;
        }
        if (!g_file_selector && !g_dos_prompt && run_character_shortcut((wchar_t)wp))
            return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(w, m, wp, lp);
}

static int is_classic_bsp_switch(const wchar_t *arg) { return arg[0] == L'-' && (arg[1] == L'b' || arg[1] == L'B') && !arg[2]; }
static int is_auto_bsp_switch(const wchar_t *arg) { return !_wcsicmp(arg, L"--bsp") || !_wcsicmp(arg, L"--export-bsp") || is_classic_bsp_switch(arg); }
static int derive_asm_path(const wchar_t *input, wchar_t output[MAX_PATH]) {
    size_t length = wcslen(input);
    if (!length || length >= MAX_PATH)
        return 0;
    const wchar_t *leaf = input, *dot = NULL;
    for (const wchar_t *p = input; *p; p++) {
        if (*p == L'\\' || *p == L'/') {
            leaf = p + 1;
            dot = NULL;
        } else if (*p == L'.')
            dot = p;
    }
    const wchar_t *end = dot && dot > leaf ? dot : input + length;
    size_t stem = (size_t)(end - input);
    if (stem + 4 >= MAX_PATH)
        return 0;
    memcpy(output, input, stem * sizeof(wchar_t));
    wcscpy(output + stem, L".asm");
    return 1;
}
static int export_bsp_cli(const wchar_t *input, const wchar_t *output, int compact_first) {
    if (wcslen(input) >= MAX_PATH || !load_shape(input))
        return 0;
    wcsncpy(g_path, input, MAX_PATH - 1);
    g_path[MAX_PATH - 1] = 0;
    if (compact_first)
        compact_shape();
    return save_bsp_asm(output);
}

int WINAPI wWinMain(HINSTANCE hi, HINSTANCE prev, PWSTR cmd, int show) {
    (void)prev;
    (void)cmd;
    SetProcessDPIAware();
    GetCurrentDirectoryW(MAX_PATH, g_snes_data_dir);
    wchar_t snes_rom[MAX_PATH];
    _snwprintf(snes_rom, MAX_PATH, L"%ls\\sdemo.rom", g_snes_data_dir);
    snes_rom[MAX_PATH - 1] = 0;
    FILE *rom = _wfopen(snes_rom, L"rb");
    if (rom) {
        g_snes_mode = 1;
        fclose(rom);
    }
    WNDCLASSW wc = {0};
    wc.hInstance = hi;
    wc.lpfnWndProc = wndproc;
    wc.lpszClassName = L"ShapedNative";
    wc.hCursor = LoadCursorW(NULL, IDC_CROSS);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wc.hIcon = LoadIcon(hi, MAKEINTRESOURCE(2));
    RegisterClassW(&wc);
    RECT wr = {0, 0, 640, 480};
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);
    g_hwnd = CreateWindowW(wc.lpszClassName, APP_TITLE, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, wr.right - wr.left, wr.bottom - wr.top, NULL, NULL, hi, NULL);
    wchar_t coltabs[MAX_PATH];
    _snwprintf(coltabs, MAX_PATH, L"%ls\\COLTABS.DAT", g_snes_data_dir);
    coltabs[MAX_PATH - 1] = 0;
    load_colour_tables_path(coltabs);
    int argc;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argc == 3 && !wcscmp(argv[1], L"--test-editor-callbacks")) {
        int ok = run_editor_callback_regression(argv[2]);
        LocalFree(argv);
        DestroyWindow(g_hwnd);
        return ok ? 0 : 1;
    }
    if (argc == 2 && (!_wcsicmp(argv[1], L"--help") || !wcscmp(argv[1], L"-?"))) {
        /*
        MessageBoxW(g_hwnd, L"Shaped.exe --bsp [input [output]]\nShaped.exe --export-bsp input [output]\nShaped.exe -b input [output]\n\nWhen output is omitted, input.ext becomes input.asm.", L"SHAPED command line", MB_OK | MB_ICONINFORMATION);
        LocalFree(argv);
        DestroyWindow(g_hwnd);
        */
        printf(
			"Shaped -Shape Editor- Carl N Graham -Argonaut Software- 1991-1992\n"
			"\n"
			"Shaped.exe --bsp [input [output]]\n"
			"Shaped.exe -b input [output]\n"
			"Shaped.exe --export-[format] input [output]\n"
			"\n"
			"[format] can be:\n"
			"gzs\npc\ninternal\n3dg1"
			"\n\n"
			"When output is omitted, input.ext becomes input.asm."
		);
        return 0;
    }
    if (argc == 2 && !_wcsicmp(argv[1], L"--bsp")) {
        ShowWindow(g_hwnd, show);
        UpdateWindow(g_hwnd);
        wchar_t input[MAX_PATH] = L"", output[MAX_PATH];
        int accepted = choose_file(0, L"Load Shape for BSP export", input), ok = 0;
        if (accepted && derive_asm_path(input, output))
            ok = export_bsp_cli(input, output, 0);
        if (accepted)
            MessageBoxW(g_hwnd, ok ? output : L"Could not load or export the selected shape.", ok ? L"BSP assembler saved" : L"BSP export failed", MB_OK | (ok ? MB_ICONINFORMATION : MB_ICONERROR));
        LocalFree(argv);
        DestroyWindow(g_hwnd);
        return accepted ? (ok ? 0 : 1) : 2;
    }
    if ((argc == 3 || argc == 4) && is_auto_bsp_switch(argv[1])) {
        wchar_t output[MAX_PATH];
        const wchar_t *destination = argc == 4 ? argv[3] : output;
        int ok = (argc == 4 || derive_asm_path(argv[2], output)) && export_bsp_cli(argv[2], destination, is_classic_bsp_switch(argv[1]));
        LocalFree(argv);
        DestroyWindow(g_hwnd);
        return ok ? 0 : 1;
    }
    if (argc == 4 && !wcscmp(argv[1], L"--test-twist")) {
        int loaded = load_shape(argv[2]), ok = 0;
        if (loaded) {
            wcscpy(g_path, argv[2]);
            ok = save_twist_report(argv[3]);
        }
        LocalFree(argv);
        DestroyWindow(g_hwnd);
        return ok ? 0 : 1;
    }
    if (argc == 4 && !wcsncmp(argv[1], L"--export-", 9)) {
        int loaded = load_shape(argv[2]), ok = 0;
        if (loaded) {
            wcscpy(g_path, argv[2]);
            if (!wcscmp(argv[1], L"--export-gzs"))
                ok = save_gzs(argv[3]);
            else if (!wcscmp(argv[1], L"--export-pc"))
                ok = save_pc_asm(argv[3]);
            else if (!wcscmp(argv[1], L"--export-internal"))
                ok = save_internal(argv[3]);
            else if (!wcscmp(argv[1], L"--export-3dg1"))
                ok = save_shape(argv[3]);
        }
        LocalFree(argv);
        DestroyWindow(g_hwnd);
        return ok ? 0 : 1;
    }
    ShowWindow(g_hwnd, show);
    UpdateWindow(g_hwnd);
    if (argc > 1 && load_shape(argv[1]))
        wcscpy(g_path, argv[1]);
    LocalFree(argv);
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
