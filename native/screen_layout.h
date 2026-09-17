// screen_layout.h - Siege of Avalon's 1920x1080 layout, fitted to the display.
//
// The 1.19 build has three layouts - 800x600, 1280x720 and 1920x1080 - each a
// packed record of screen size, map viewport and HUD positions
// (TScreenResolutionData, engine/SoAOS.Types.pas in the game's source). The
// game copies the chosen one into its global ScreenMetrics with _CopyRecord
// and lays everything out from that copy. So the 1080 layout adapts to any
// display by rewriting its record before that copy: the screen grows to the
// display's shape (1920 wide and taller, or 1080 tall and wider), the map
// viewport grows with it, and each HUD position follows the edge it belongs
// to - the sidebar the right edge, the bottom bar and its buttons the bottom.
//
// Three pieces of HUD art are sized for 1080 and drawn at their full size, so
// they are regenerated at the new size into the player's profile, which the
// kit's file overlay reads before the game directory: the sidebar repeats its
// middle panels, the bottom and spell bars repeat their plain parchment, and
// the menu backdrop grows around its centred hole.
//
// Two handlers compare the cursor with the spell bar's 1080 rows as numbers in
// code rather than record fields; while the spell bar is open they are handed
// the cursor moved back by the same amount.
//
// A display of the 1080 layout's own shape changes nothing.
#pragma once
#include "x86.h"
#include "native_seam.h"
#include "platform/os.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---- addresses in the patch's 1.19 Siege.exe --------------------------------
#define SIEGE_CFULLHD 0x00c852f2u     // cFullHD, the 1080 record in .data
#define SIEGE_COPY_RECORD 0x0080dbb4u // System._CopyRecord(Dest, Source, TypeInfo)
#define SIEGE_MOUSE_DOWN 0x00c3fcbcu  // TfrmMain.FormMouseDown
#define SIEGE_MOUSE_MOVE 0x00c4090cu  // TfrmMain.FormMouseMove
#define SIEGE_SPELLBAR_ACTIVE 0x478u  // TfrmMain.FSpellBarActive, a byte

// ---- the record, packed -----------------------------------------------------
enum {
    SL_SCREEN_W = 0x00,
    SL_SCREEN_H = 0x04,
    SL_PREMAP_W = 0x17,
    SL_PREMAP_H = 0x1b,
    SL_GAME_W = 0x1f,
    SL_GAME_H = 0x23,
    SL_SPELLBAR_X = 0x2f,
    SL_SPELLBAR_Y = 0x33,
    SL_STATS_X = 0x37,
    SL_STATS_Y = 0x3b,
    SL_HELPBOX_Y = 0x47,
    SL_NPCBAR_Y = 0x57,
    SL_MANA_EMPTY_X = 0x5b,
    SL_LIFE_EMPTY_X = 0x5f,
    SL_RECTS = 0xa3, // 17 TRects, 16 bytes each
};
// The HUD's hit rectangles, in record order, and the edges each one follows.
enum { SL_LEFT_TOP = 0, SL_RIGHT = 1, SL_BOTTOM = 2 };
static const unsigned char sl_rect_anchor[17] = {
    SL_RIGHT | SL_BOTTOM, // popInventoryRect: the sidebar's foot
    SL_RIGHT | SL_BOTTOM, // popMapRect: the bottom bar's right end
    SL_BOTTOM,            // popQuestRect
    SL_BOTTOM,            // popAdventureRect
    SL_BOTTOM,            // popNoteLogRect
    SL_BOTTOM,            // popJournalRect
    SL_BOTTOM,            // popAwardsRect
    SL_BOTTOM,            // popMessageRect
    SL_RIGHT,             // popStatsRect: the sidebar's head
    SL_RIGHT,             // popManaRect
    SL_RIGHT,             // popHealthRect
    SL_BOTTOM,            // popSpellRect
    SL_BOTTOM,            // popRosterRect
    SL_BOTTOM,            // popParty1Rect
    SL_BOTTOM,            // popParty2Rect
    SL_BOTTOM,            // popParty3Rect
    SL_BOTTOM,            // popParty4Rect
};
// The values the record ships with, so rewriting it is the same whenever it
// happens and however often.
static const int32_t sl_rects_1080[17][4] = {
    {1846, 885, 1894, 931},   {1852, 991, 1903, 1035}, {1148, 991, 1196, 1011},
    {1139, 1015, 1205, 1036}, {1076, 992, 1133, 1009}, {1142, 1040, 1206, 1058},
    {1088, 1023, 1127, 1063}, {561, 997, 1072, 1062},  {1835, 10, 1898, 104},
    {1828, 146, 1885, 203},   {1831, 258, 1879, 348},  {506, 1027, 541, 1062},
    {344, 1019, 425, 1051},   {3, 990, 65, 1066},      {80, 990, 151, 1066},
    {166, 990, 233, 1066},    {248, 990, 319, 1066},
};

// ---- the fitted size ----------------------------------------------------------
typedef struct SiegeLayout {
    int w, h, dx, dy; // the screen, and how far it grew from 1920x1080
} SiegeLayout;

// 1920 wide and taller for a display narrower than 16:9, 1080 tall and wider
// for one wider; even sizes, and nothing outside 4:3 to 32:9.
__attribute__((weak)) int siege_layout_for(int sw, int sh, SiegeLayout *out) {
    out->w = 1920;
    out->h = 1080;
    out->dx = out->dy = 0;
    if (sw <= 0 || sh <= 0)
        return 0;
    double aspect = (double)sw / (double)sh;
    if (aspect < 4.0 / 3.0)
        aspect = 4.0 / 3.0;
    if (aspect > 32.0 / 9.0)
        aspect = 32.0 / 9.0;
    if (aspect < 16.0 / 9.0 - 0.01)
        out->h = ((int)(1920.0 / aspect + 0.5) + 1) & ~1;
    else if (aspect > 16.0 / 9.0 + 0.01)
        out->w = ((int)(1080.0 * aspect + 0.5) + 1) & ~1;
    out->dx = out->w - 1920;
    out->dy = out->h - 1080;
    return out->dx != 0 || out->dy != 0;
}

// The layout in force, once the record has been rewritten; zero growth before.
__attribute__((weak)) SiegeLayout siege_layout_active = {1920, 1080, 0, 0};

// ---- 24-bit BMP files ----------------------------------------------------------
typedef struct SiegeImage {
    int w, h;          // top-down in `px`
    unsigned char *px; // w * h * 3, BGR
} SiegeImage;

static inline uint32_t sl_le32(const unsigned char *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static inline void sl_put32(unsigned char *p, uint32_t v) {
    p[0] = (unsigned char)v;
    p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16);
    p[3] = (unsigned char)(v >> 24);
}

// A 24-bit uncompressed BMP, or zero.
__attribute__((weak)) int siege_bmp_read(const char *path, SiegeImage *img) {
    memset(img, 0, sizeof *img);
    FILE *f = fopen(path, "rb");
    if (!f)
        return 0;
    unsigned char head[54];
    int ok = fread(head, 1, sizeof head, f) == sizeof head && head[0] == 'B' && head[1] == 'M';
    int32_t w = ok ? (int32_t)sl_le32(head + 18) : 0, h = ok ? (int32_t)sl_le32(head + 22) : 0;
    ok = ok && head[28] == 24 && sl_le32(head + 30) == 0 && w > 0 && w <= 16384 && h != 0 &&
         h <= 16384 && h >= -16384;
    const int bottom_up = h > 0;
    if (h < 0)
        h = -h;
    if (ok) {
        img->w = w;
        img->h = h;
        img->px = (unsigned char *)malloc((size_t)w * (size_t)h * 3u);
        ok = img->px != NULL && fseek(f, (long)sl_le32(head + 10), SEEK_SET) == 0;
    }
    const size_t stride = ((size_t)w * 3u + 3u) & ~(size_t)3u;
    unsigned char *row = ok ? (unsigned char *)malloc(stride) : NULL;
    for (int32_t y = 0; ok && y < h; ++y) {
        ok = row && fread(row, 1, stride, f) == stride;
        const int32_t dest = bottom_up ? h - 1 - y : y;
        if (ok)
            memcpy(img->px + (size_t)dest * (size_t)w * 3u, row, (size_t)w * 3u);
    }
    free(row);
    fclose(f);
    if (!ok) {
        free(img->px);
        memset(img, 0, sizeof *img);
    }
    return ok;
}

// Written beside and renamed over, so a reader never sees half a file.
__attribute__((weak)) int siege_bmp_write(const char *path, const SiegeImage *img) {
    char temporary[1100];
    if (snprintf(temporary, sizeof temporary, "%s.tmp", path) >= (int)sizeof temporary)
        return 0;
    FILE *f = fopen(temporary, "wb");
    if (!f)
        return 0;
    const size_t stride = ((size_t)img->w * 3u + 3u) & ~(size_t)3u;
    unsigned char head[54] = {'B', 'M'};
    sl_put32(head + 2, (uint32_t)(54 + stride * (size_t)img->h));
    sl_put32(head + 10, 54);
    sl_put32(head + 14, 40);
    sl_put32(head + 18, (uint32_t)img->w);
    sl_put32(head + 22, (uint32_t)img->h);
    head[26] = 1;
    head[28] = 24;
    sl_put32(head + 34, (uint32_t)(stride * (size_t)img->h));
    int ok = fwrite(head, 1, sizeof head, f) == sizeof head;
    unsigned char *row = (unsigned char *)calloc(1, stride);
    for (int y = img->h - 1; ok && y >= 0; --y) {
        memcpy(row, img->px + (size_t)y * (size_t)img->w * 3u, (size_t)img->w * 3u);
        ok = row && fwrite(row, 1, stride, f) == stride;
    }
    free(row);
    ok = fclose(f) == 0 && ok;
    if (ok)
        ok = os_rename(temporary, path) == 0;
    if (!ok)
        os_unlink(temporary);
    return ok;
}

// The size of the BMP at `path`, or zero.
__attribute__((weak)) int siege_bmp_size(const char *path, int *w, int *h) {
    FILE *f = fopen(path, "rb");
    if (!f)
        return 0;
    unsigned char head[26];
    const int ok =
        fread(head, 1, sizeof head, f) == sizeof head && head[0] == 'B' && head[1] == 'M';
    fclose(f);
    if (!ok)
        return 0;
    *w = (int32_t)sl_le32(head + 18);
    *h = (int32_t)sl_le32(head + 22);
    if (*h < 0)
        *h = -*h;
    return 1;
}

// Source row (or column) for output position `i` when `extra` positions are
// inserted before `at`, repeating the band [band0, band1) that ends there.
static inline int sl_source(int i, int at, int extra, int band0, int band1) {
    if (i < at)
        return i;
    if (i >= at + extra)
        return i - extra;
    const int n = band1 - band0;
    return band0 + (i - at) % n;
}

// How one axis of an image grows. Repeating inserts all of it before at[0],
// from the band [band0, band1). Stretching keeps [at[0], at[1]) as it is and
// spreads what lies either side over half the growth each, so that span
// stays centred - for art with no seam-free band, like a vignette.
typedef struct SiegeGrowth {
    int at[2], band0, band1, stretch;
} SiegeGrowth;
static inline int sl_axis(int i, int n, int extra, const SiegeGrowth *g) {
    if (!g->stretch)
        return sl_source(i, g->at[0], extra, g->band0, g->band1);
    const int a = g->at[0], b = g->at[1], first = extra / 2;
    const int a_out = a + first, b_out = b + first;
    if (i < a_out)
        return (int)((int64_t)i * a / a_out);
    if (i < b_out)
        return i - first;
    const int tail_in = n - b, tail_out = n + extra - b_out;
    return b + (int)((int64_t)(i - b_out) * tail_in / tail_out);
}
// A copy of `src` grown by dx columns and dy rows.
__attribute__((weak)) int siege_grow(const SiegeImage *src, int dx, int dy, const SiegeGrowth *gx,
                                     const SiegeGrowth *gy, SiegeImage *out) {
    out->w = src->w + dx;
    out->h = src->h + dy;
    out->px = (unsigned char *)malloc((size_t)out->w * (size_t)out->h * 3u);
    if (!out->px)
        return 0;
    for (int y = 0; y < out->h; ++y) {
        const int sy = dy ? sl_axis(y, src->h, dy, gy) : y;
        const unsigned char *srow = src->px + (size_t)sy * (size_t)src->w * 3u;
        unsigned char *drow = out->px + (size_t)y * (size_t)out->w * 3u;
        for (int x = 0; x < out->w; ++x) {
            const int sx = dx ? sl_axis(x, src->w, dx, gx) : x;
            memcpy(drow + (size_t)x * 3u, srow + (size_t)sx * 3u, 3);
        }
    }
    return 1;
}

// ---- the art -------------------------------------------------------------------
// Regenerates guest file `name` (under the game directory) grown by dx, dy
// into the profile, or removes a stale one when there is no growth. A file
// already the right size is left alone.
__attribute__((weak)) void siege_fit_art(const char *name, int dx, int dy, const SiegeGrowth *gx,
                                         const SiegeGrowth *gy, int w0, int h0) {
    char target[1024], source[1024];
    if (!recomp_writable_path(name, target, sizeof target))
        return; // no profile tier: never write into the game directory
    int tw = 0, th = 0;
    const int present = siege_bmp_size(target, &tw, &th);
    if (!dx && !dy) {
        // The profile copy exists only because a fitted layout made it.
        if (present && (tw != w0 || th != h0))
            os_unlink(target);
        return;
    }
    if (present && tw == w0 + dx && th == h0 + dy)
        return;
    // The original: the game directory's, never an earlier fitted copy.
    if (present)
        os_unlink(target);
    if (!recomp_readable_path(name, source, sizeof source))
        return;
    SiegeImage src, grown;
    if (!siege_bmp_read(source, &src))
        return;
    if (src.w == w0 && src.h == h0 && siege_grow(&src, dx, dy, gx, gy, &grown)) {
        // The profile's own copy of a directory the game directory has.
        char dir[1024];
        snprintf(dir, sizeof dir, "%s", target);
        for (char *p = dir + 1; *p; ++p)
            if (*p == '/' || *p == '\\') {
                const char keep = *p;
                *p = 0;
                os_mkdir(dir);
                *p = keep;
            }
        if (!siege_bmp_write(target, &grown))
            fprintf(stderr, "[siege] could not write the fitted %s\n", name);
        free(grown.px);
    }
    free(src.px);
}

__attribute__((weak)) void siege_fit_all_art(int dx, int dy) {
    // Sidebar: its middle panels repeat, a period of 100 rows.
    static const SiegeGrowth side_rows = {{760, 0}, 660, 760, 0};
    // Bars: plain parchment between the journal buttons and the map button.
    static const SiegeGrowth bar_cols = {{1700, 0}, 1300, 1700, 0};
    // The menu backdrop is a vignette: stretched around its 800x600 hole.
    static const SiegeGrowth back_cols = {{560, 1360}, 0, 0, 1};
    static const SiegeGrowth back_rows = {{240, 840}, 0, 0, 1};
    static const char *const languages[] = {"english", "german",  "czech",  "polish",
                                            "russian", "spanish", "french", "italian"};
    char name[256];
    for (size_t i = 0; i < sizeof languages / sizeof languages[0]; ++i) {
        snprintf(name, sizeof name, "Interface\\%s\\sidebarFullHD.bmp", languages[i]);
        siege_fit_art(name, 0, dy, NULL, &side_rows, 117, 966);
        snprintf(name, sizeof name, "Interface\\%s\\bottombarFullHD.bmp", languages[i]);
        siege_fit_art(name, dx, 0, &bar_cols, NULL, 1920, 114);
        snprintf(name, sizeof name, "Interface\\%s\\spellbarFullHD.bmp", languages[i]);
        siege_fit_art(name, dx, 0, &bar_cols, NULL, 1920, 114);
    }
    siege_fit_art("Interface\\gMainMenuOverlay1080.bmp", dx, dy, &back_cols, &back_rows, 1920,
                  1080);
}

// ---- the record ------------------------------------------------------------------
__attribute__((weak)) void siege_fit_record(const SiegeLayout *l) {
    const uint32_t r = SIEGE_CFULLHD;
    wr32(r + SL_SCREEN_W, (uint32_t)l->w);
    wr32(r + SL_SCREEN_H, (uint32_t)l->h);
    wr32(r + SL_PREMAP_W, (uint32_t)l->w);
    wr32(r + SL_PREMAP_H, (uint32_t)l->h);
    wr32(r + SL_GAME_W, (uint32_t)(1823 + l->dx));
    wr32(r + SL_GAME_H, (uint32_t)(997 + l->dy));
    wr32(r + SL_SPELLBAR_X, (uint32_t)(1803 + l->dx));
    wr32(r + SL_SPELLBAR_Y, (uint32_t)(966 + l->dy));
    wr32(r + SL_STATS_X, (uint32_t)(1819 + l->dx));
    wr32(r + SL_STATS_Y, (uint32_t)(966 + l->dy));
    wr32(r + SL_HELPBOX_Y, (uint32_t)(935 + l->dy));
    wr32(r + SL_NPCBAR_Y, (uint32_t)(1061 + l->dy));
    wr32(r + SL_MANA_EMPTY_X, (uint32_t)(1819 + l->dx));
    wr32(r + SL_LIFE_EMPTY_X, (uint32_t)(1829 + l->dx));
    for (int i = 0; i < 17; ++i) {
        const int ox = (sl_rect_anchor[i] & SL_RIGHT) ? l->dx : 0;
        const int oy = (sl_rect_anchor[i] & SL_BOTTOM) ? l->dy : 0;
        const uint32_t at = r + SL_RECTS + 16u * (uint32_t)i;
        wr32(at, (uint32_t)(sl_rects_1080[i][0] + ox));
        wr32(at + 4, (uint32_t)(sl_rects_1080[i][1] + oy));
        wr32(at + 8, (uint32_t)(sl_rects_1080[i][2] + ox));
        wr32(at + 12, (uint32_t)(sl_rects_1080[i][3] + oy));
    }
}

// The record is rewritten the first time the game copies it - which is when
// it applies the ScreenResolution=1080 it read from siege.ini - and only if
// it still holds the shipped values, which is what this port pins.
__attribute__((weak)) void siege_fit_layout(void) {
    static int done;
    if (done)
        return;
    done = 1;
    if (rd32(SIEGE_CFULLHD + SL_SCREEN_W) != 1920 || rd32(SIEGE_CFULLHD + SL_GAME_W) != 1823 ||
        rd32(SIEGE_CFULLHD + SL_RECTS) != 1846)
        return;
    int sw = 0, sh = 0;
    SiegeLayout l;
    const int grows = host_display_screen_size(&sw, &sh) && siege_layout_for(sw, sh, &l);
    siege_fit_all_art(grows ? l.dx : 0, grows ? l.dy : 0);
    if (!grows)
        return;
    siege_fit_record(&l);
    siege_layout_active = l;
    ddraw_add_mode(l.w, l.h, 16);
    fprintf(stderr, "[siege] the 1920x1080 layout fits the %dx%d display as %dx%d\n", sw, sh, l.w,
            l.h);
}

void fn_0080dbb4(X86 *c);
__attribute__((weak)) void siege_copy_record(X86 *c) {
    if (c->r[R_EDX] == SIEGE_CFULLHD)
        siege_fit_layout();
    fn_0080dbb4(c);
}
#define FN_0080dbb4 siege_copy_record

// The spell bar's hit test compares Y with its 1080 rows, 966 to 1075, as
// constants (TfrmMain.FormMouseDown and FormMouseMove, engine/AniDemo.pas).
// While the bar is open, a cursor on its fitted rows is handed to the handler
// moved back up by the growth, and one on the map that the constants would
// take for the bar is handed over as the row above it; the handlers' other
// branches do not run while the bar is open.
static inline void siege_spellbar_y(X86 *c) {
    const int dy = siege_layout_active.dy;
    if (!dy || !rd8(c->r[R_EAX] + SIEGE_SPELLBAR_ACTIVE))
        return;
    const uint32_t at = c->r[R_ESP] + 4u; // Y, the last parameter, is pushed last
    const int32_t y = (int32_t)rd32(at);
    if (y >= 966 + dy && y < 1075 + dy)
        wr32(at, (uint32_t)(y - dy));
    else if (y >= 966 && y < 1075)
        wr32(at, 965);
}
void fn_00c3fcbc(X86 *c);
__attribute__((weak)) void siege_form_mouse_down(X86 *c) {
    siege_spellbar_y(c);
    fn_00c3fcbc(c);
}
#define FN_00c3fcbc siege_form_mouse_down
void fn_00c4090c(X86 *c);
__attribute__((weak)) void siege_form_mouse_move(X86 *c) {
    siege_spellbar_y(c);
    fn_00c4090c(c);
}
#define FN_00c4090c siege_form_mouse_move
