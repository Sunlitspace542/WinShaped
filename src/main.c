/* Portable CLI-only SHAPED exporter. */
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <wchar.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#define MAX_PATH PATH_MAX
#define MAX_DOTS 500
#define MAX_POLYS 500
#define MAX_POLY_VERTS 16
#define MAX_FRAMES 128
#define MAX_SNES_ENTRIES 24
#define _snwprintf swprintf

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
    int poly, front, back, leaf;
} BSPNode;
typedef struct {
    char name[50];
    wchar_t label[50];
    int value;
} SNESEntry;

static Shape g_shape;
static size_t g_current_frame;
static uint16_t g_current_group = 1, g_poly_type = 7;
static wchar_t g_path[MAX_PATH];
static BSPNode g_bsp_nodes[MAX_POLYS];
static int g_bsp_coplanar_head[MAX_POLYS], g_bsp_coplanar_next[MAX_POLYS];
static int g_bsp_root = -1, g_bsp_count, g_bsp_valid, g_bsp_spanning, g_bsp_flat, g_bsp_diag_mode;
static double g_plane_weight = 1.0;
static SNESEntry g_coltab_entries[MAX_SNES_ENTRIES];
static int g_coltab_count, g_coltab_index, g_smooth_shade;

static FILE *portable_wfopen(const wchar_t *path, const wchar_t *mode) {
    char narrow[MAX_PATH], flags[8];
    if (wcstombs(narrow, path, sizeof(narrow)) == (size_t)-1 || wcstombs(flags, mode, sizeof(flags)) == (size_t)-1)
        return NULL;
    return fopen(narrow, flags);
}
/* C text streams only translate newlines on Windows. Normalize explicitly so
   every generated interchange file has CRLF records on every supported host. */
static int finish_text_output(const wchar_t *path, FILE *output) {
    if (fclose(output) != 0)
        return 0;
    FILE *input = portable_wfopen(path, L"rb"), *temporary = tmpfile();
    if (!input || !temporary) {
        if (input)
            fclose(input);
        if (temporary)
            fclose(temporary);
        return 0;
    }
    int previous = 0, ch;
    while ((ch = fgetc(input)) != EOF) {
        if (ch == '\n' && previous != '\r' && fputc('\r', temporary) == EOF) {
            fclose(input);
            fclose(temporary);
            return 0;
        }
        if (fputc(ch, temporary) == EOF) {
            fclose(input);
            fclose(temporary);
            return 0;
        }
        previous = ch;
    }
    if (ferror(input) || fclose(input) != 0 || fflush(temporary) != 0 || fseek(temporary, 0, SEEK_SET) != 0) {
        fclose(temporary);
        return 0;
    }
    output = portable_wfopen(path, L"wb");
    if (!output) {
        fclose(temporary);
        return 0;
    }
    while ((ch = fgetc(temporary)) != EOF)
        if (fputc(ch, output) == EOF) {
            fclose(temporary);
            fclose(output);
            return 0;
        }
    int ok = !ferror(temporary) && fclose(temporary) == 0 && fclose(output) == 0;
    return ok;
}
static int portable_strcasecmp(const char *left, const char *right) {
    while (*left && tolower((unsigned char)*left) == tolower((unsigned char)*right)) {
        ++left;
        ++right;
    }
    return tolower((unsigned char)*left) - tolower((unsigned char)*right);
}
#define _wfopen portable_wfopen
#define strcasecmp portable_strcasecmp
static void free_frames(Shape *shape);
static int copy_shape(Shape *dst, const Shape *src);
static void build_bsp(void);
static int begin_export_bsp(Shape *saved, Shape *working);
static void end_export_bsp(Shape *saved, Shape *working);
static int polygon_plane(size_t pi, double plane[4]);
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
static double dos_coord(double value) {
    if (!isfinite(value))
        return 0.0;
    int64_t whole = (int64_t)trunc(value);
    uint16_t bits = (uint16_t)((uint64_t)whole & 0xffffu);
    return (double)(int16_t)bits;
}

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
static int dot_active(size_t index) { return dot_active_at_frame(index, g_current_frame); }
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
    return finish_text_output(path, f);
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
    return finish_text_output(path, f);
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
    if (count > 0) {
        size_t written = wcstombs(out, start, (size_t)count);
        if (written == (size_t)-1)
            out[0] = 'S', count = 1;
    } else
        out[0] = 'S', count = 1;
    out[count] = 0;
}
static void source_name(char out[MAX_PATH]) {
    if (g_path[0] && wcstombs(out, g_path, MAX_PATH) != (size_t)-1) {
    } else
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
    const char *coltab = g_coltab_count && g_coltab_index >= 0 && g_coltab_index < g_coltab_count ? g_coltab_entries[g_coltab_index].name : "id_0_c"; // default to id_0_c if no coltab specified
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
    if (count != 0) {
        fprintf(f, "\tVizis\t%llu\n", (unsigned long long)count);
    } else {
        // Create dummy vizi if there are none (fixes crash for models composed of all lines)
        fprintf(f, "\tVizis\t1\n");
    }
    for (size_t i = 0; i < g_shape.poly_count; i++) {
        Poly *p = &g_shape.polys[i];
        if (!p->flags || p->count <= 2)
            continue;
        fprintf(f, "\tViz\t%u,%u,%u\t;%llu\n", p->index[0], p->index[1], p->index[2], (unsigned long long)i);
    }
    // Create dummy vizi if there are none (fixes crash for models composed of all lines)
    if (count == 0) {
        fprintf(f, "\tViz\t0,0,0\t; 0\n");
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
    return finish_text_output(path, f);
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
    write_asm_header(f, name, NULL, 0);
    write_asm_points(f, name);
    fprintf(f, "%s_F\n", name);
    write_asm_vizis(f);
    if (g_smooth_shade)
        write_asm_vertex_normals(f, name, NULL, 0);
    if (g_bsp_flat && g_bsp_root >= 0) {
        fprintf(f, "\n%s_f1\tFaces\n", name);
        int number = 2;
        write_bsp_faces(f, name, g_bsp_root, &number);
        fputs("\tFend\n\tEndShape\n\n\tendc\n", f);
    } else {
        fprintf(f, "\tBSPInit\t%s_EBSP\n", name);
        int number = 1;
        write_bsp_tree(f, name, g_bsp_root, &number);
        if (g_bsp_root >= 0) {
            fprintf(f, "\n%s_f1\tFaces\n", name);
            number = 2;
            write_bsp_faces(f, name, g_bsp_root, &number);
            fprintf(f, "\tFendQ\n%s_EBSP\n\tEndShape\n\n\tendc\n", name);
        } else
            fprintf(f, "\tBSPEND\n%s_EBSP\n\tEndShape\n\n\tendc\n", name);
    }
    return finish_text_output(path, f);
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
        pc_write_flat_primitives(f, g_bsp_root, vertex_offsets, intensity_offsets, visibility_offsets);
    } else if (g_bsp_root >= 0) {
        int number = 1;
        pc_write_bsp_commands(f, name, g_bsp_root, &number, visibility_offsets);
        fprintf(f, "\n%s_f1\tlabel word\n", name);
        number = 2;
        pc_write_bsp_primitives(f, name, g_bsp_root, &number, vertex_offsets, intensity_offsets, visibility_offsets);
    } else
        for (size_t i = 0; i < g_shape.poly_count; i++)
            pc_write_primitive(f, &g_shape.polys[i], i, vertex_offsets, intensity_offsets, visibility_offsets);
    fprintf(f, "\tDW CMD_QUIT\n%s_end\tlabel\tword\n\tdw CMD_QUIT\n", name);
    free(frame_maps);
    return finish_text_output(path, f);
}
static int save_pc_asm(const wchar_t *path) {
    Shape saved, working = {0};
    if (!begin_export_bsp(&saved, &working))
        return 0;
    int ok = save_pc_asm_body(path);
    end_export_bsp(&saved, &working);
    return ok;
}
static void snes_entry(SNESEntry *entry, const char *name, int value) {
    size_t length = strlen(name);
    if (length >= sizeof(entry->name))
        length = sizeof(entry->name) - 1;
    memcpy(entry->name, name, length);
    entry->name[length] = 0;
    mbstowcs(entry->label, entry->name, sizeof(entry->label) / sizeof(entry->label[0]));
    entry->label[49] = 0;
    entry->value = value;
}
static int load_colour_tables_path(const wchar_t *path) {
    FILE *f = _wfopen(path, L"rb");
    if (!f)
        return 0;
    g_coltab_count = 0;
    char kind[32], name[50];
    int value;
    while (fscanf(f, "%31s %49s %d", kind, name, &value) == 3) {
        if (!strcmp(kind, "COLTAB") && g_coltab_count < MAX_SNES_ENTRIES) {
            snes_entry(&g_coltab_entries[g_coltab_count], name, value);
            g_coltab_count++;
        }
    }
    fclose(f);
    if (g_coltab_index >= g_coltab_count)
        g_coltab_index = 0;
    g_smooth_shade = g_coltab_count && g_coltab_entries[g_coltab_index].value < 0;
    return 1;
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
    FILE *f = _wfopen(path, L"rb");
    if (!f)
        return 0;
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
    free_frames(&g_shape);
    g_shape = n;
    g_current_frame = 0;
    return 1;
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
    return finish_text_output(path, f);
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
        if (g_shape.polys[i].count >= 2 && g_shape.polys[i].flags)
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

static int is_bsp_switch(const char *arg) {
    return !strcasecmp(arg, "--bsp") || !strcasecmp(arg, "--export-bsp") || !strcasecmp(arg, "-b");
}
static int to_wide(const char *input, wchar_t output[MAX_PATH]) { return mbstowcs(output, input, MAX_PATH) != (size_t)-1; }
static int derive_asm_path(const char *input, char output[MAX_PATH]) {
    size_t length = strlen(input);
    if (!length || length >= MAX_PATH)
        return 0;
    const char *leaf = input, *dot = NULL;
    for (const char *p = input; *p; ++p) {
        if (*p == '/' || *p == '\\') {
            leaf = p + 1;
            dot = NULL;
        } else if (*p == '.')
            dot = p;
    }
    size_t stem = (size_t)((dot && dot > leaf ? dot : input + length) - input);
    if (stem + 4 >= MAX_PATH)
        return 0;
    memcpy(output, input, stem);
    memcpy(output + stem, ".asm", 5);
    return 1;
}
static int export_shape(const char *mode, const char *input, const char *output) {
    wchar_t wide_input[MAX_PATH], wide_output[MAX_PATH];
    if (!to_wide(input, wide_input) || !to_wide(output, wide_output) || !load_shape(wide_input))
        return 0;
    if (!strcasecmp(mode, "--export-gzs"))
        return save_gzs(wide_output);
    if (!strcasecmp(mode, "--export-bsp") || !strcasecmp(mode, "--bsp") || !strcasecmp(mode, "-b"))
        return save_bsp_asm(wide_output);
    if (!strcasecmp(mode, "--export-pc"))
        return save_pc_asm(wide_output);
    if (!strcasecmp(mode, "--export-internal"))
        return save_internal(wide_output);
    if (!strcasecmp(mode, "--export-3dg1"))
        return save_shape(wide_output);
    if (!strcasecmp(mode, "--test-twist"))
        return save_twist_report(wide_output);
    return 0;
}
static void print_usage(FILE *stream) {
    fprintf(stream, "Usage: shaped <command> <input> [output]\n\nCommands:\n  --export-gzs input output\n  --export-bsp input [output]  (--bsp and -b are aliases)\n  --export-pc input output\n  --export-internal input output\n  --export-3dg1 input output\n  --test-twist input output\n");
}
int main(int argc, char **argv) {
    if (argc == 2 && (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-?") || !strcmp(argv[1], "-h"))) {
        print_usage(stdout);
        return 0;
    }
    if (argc < 3 || argc > 4) {
        print_usage(stderr);
        return 2;
    }
    const char *output = argc == 4 ? argv[3] : NULL;
    char derived[MAX_PATH];
    if (!output && is_bsp_switch(argv[1])) {
        if (!derive_asm_path(argv[2], derived)) {
            fputs("Could not derive output path.\n", stderr);
            return 2;
        }
        output = derived;
    }
    if (!output) {
        fputs("This command requires an output path.\n", stderr);
        return 2;
    }
    load_colour_tables_path(L"COLTABS.DAT");
    if (!export_shape(argv[1], argv[2], output)) {
        fprintf(stderr, "Export failed: %s\n", argv[2]);
        return 1;
    }
    return 0;
}
