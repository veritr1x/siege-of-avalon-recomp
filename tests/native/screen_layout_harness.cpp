// screen_layout_harness.cpp - drives native/screen_layout.h: the fitted size
// for display shapes, the rewritten 1080 record, the regenerated art and the
// spell bar's cursor rows, with the runtime reduced to stubs and the game and
// profile directories under argv[1]. Prints one line per check and exits
// non-zero on any failure. tests/test_screen_layout.py builds and runs it.
#include "screen_layout.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/stat.h>
#include <vector>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#define make_dir(path) _mkdir(path)
#define R_OK 4
#else
#include <unistd.h>
#define make_dir(path) mkdir(path, 0755)
#endif

static std::vector<uint8_t> arena(0x01000000);
uint8_t *g_mem = arena.data();
uint32_t g_watch_base = 0, g_watch_len = 0;
RecompDirty g_dirty[RECOMP_DIRTY_SLOTS];
uint32_t g_dirty_count = 0;
void recomp_watch_hit(uint32_t, uint32_t, uint64_t) {}
void recomp_callback_return(X86 *) {}
int recomp_is_call_return(uint32_t) {
    return 0;
}
int recomp_module_is_call_return(uint32_t) {
    return 0;
}
int32_t recomp_index_of(uint32_t) {
    return -1;
}
int recomp_module_lookup(uint32_t) {
    return -1;
}
void recomp_call(X86 *, uint32_t) {}

int os_mkdir(const char *path) {
    return make_dir(path);
}
int os_rename(const char *from, const char *to) {
    return rename(from, to);
}
int os_unlink(const char *path) {
    return unlink(path);
}

static std::string root;
static int display_w, display_h;
int host_display_screen_size(int *w, int *h) {
    *w = display_w;
    *h = display_h;
    return display_w > 0;
}
// Guest paths are backslashed and relative to the game directory.
static std::string host_path(const char *tier, const char *guest) {
    std::string p = root + "/" + tier + "/" + guest;
    for (char &ch : p)
        if (ch == '\\')
            ch = '/';
    return p;
}
int recomp_writable_path(const char *guest, char *out, size_t n) {
    snprintf(out, n, "%s", host_path("profile", guest).c_str());
    return 1;
}
int recomp_readable_path(const char *guest, char *out, size_t n) {
    std::string p = host_path("profile", guest);
    if (access(p.c_str(), R_OK) != 0)
        p = host_path("game", guest);
    if (access(p.c_str(), R_OK) != 0)
        return 0;
    snprintf(out, n, "%s", p.c_str());
    return 1;
}
static int modes_offered;
int ddraw_add_mode(int, int, int) {
    return ++modes_offered;
}
static int copies;
void fn_0080dbb4(X86 *) {
    ++copies;
}
static int32_t seen_y;
void fn_00c3fcbc(X86 *c) {
    seen_y = (int32_t)rd32(c->r[R_ESP] + 4);
}
void fn_00c4090c(X86 *c) {
    seen_y = (int32_t)rd32(c->r[R_ESP] + 4);
}

static int failures = 0;
static void check(bool ok, const char *what, long got, long want) {
    std::printf("%s %s (got %ld, want %ld)\n", ok ? "ok  " : "FAIL", what, got, want);
    if (!ok)
        ++failures;
}

// Each pixel says where it came from: row in blue and green, column in red.
static void write_fixture(const char *guest, int w, int h) {
    SiegeImage img = {w, h, (unsigned char *)malloc((size_t)w * h * 3)};
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            unsigned char *p = img.px + ((size_t)y * w + x) * 3;
            p[0] = (unsigned char)y;
            p[1] = (unsigned char)(y >> 8);
            p[2] = (unsigned char)(x * 251 / (w > 1 ? w - 1 : 1));
        }
    std::string path = host_path("game", guest);
    for (size_t i = root.size() + 1; i < path.size(); ++i)
        if (path[i] == '/') {
            path[i] = 0;
            make_dir(path.c_str());
            path[i] = '/';
        }
    if (!siege_bmp_write(path.c_str(), &img))
        std::printf("FAIL could not write %s\n", guest), ++failures;
    free(img.px);
}
static int source_row(const SiegeImage &img, int y) {
    const unsigned char *p = img.px + (size_t)y * img.w * 3;
    return p[0] | p[1] << 8;
}

static void check_layout(int sw, int sh, int w, int h) {
    SiegeLayout l;
    const int grows = siege_layout_for(sw, sh, &l);
    char what[96];
    snprintf(what, sizeof what, "%dx%d is fitted as %dx%d (width)", sw, sh, w, h);
    check(l.w == w, what, l.w, w);
    snprintf(what, sizeof what, "%dx%d is fitted as %dx%d (height)", sw, sh, w, h);
    check(l.h == h, what, l.h, h);
    snprintf(what, sizeof what, "%dx%d grows the layout", sw, sh);
    check(grows == (w != 1920 || h != 1080), what, grows, w != 1920 || h != 1080);
}

int main(int argc, char **argv) {
    if (argc < 2)
        return 2;
    root = argv[1];

    check_layout(1920, 1080, 1920, 1080);
    check_layout(3840, 2160, 1920, 1080);
    check_layout(1366, 768, 1920, 1080);  // within the tolerance of 16:9
    check_layout(2388, 1668, 1920, 1342); // an 11-inch iPad Pro, in pixels
    check_layout(1210, 834, 1920, 1324);  // the 11-inch M4, in points
    check_layout(1920, 1200, 1920, 1200);
    check_layout(1024, 768, 1920, 1440);
    check_layout(768, 1024, 1920, 1440); // portrait is held to 4:3
    check_layout(2560, 1080, 2560, 1080);
    check_layout(3440, 1440, 2580, 1080);
    check_layout(7680, 1080, 3840, 1080); // held to 32:9
    check_layout(0, 0, 1920, 1080);

    // The shipped record, then a wide display.
    const uint32_t r = SIEGE_CFULLHD;
    wr32(r + SL_SCREEN_W, 1920);
    wr32(r + SL_SCREEN_H, 1080);
    wr32(r + SL_GAME_W, 1823);
    for (int i = 0; i < 17; ++i)
        for (int k = 0; k < 4; ++k)
            wr32(r + SL_RECTS + 16u * i + 4u * k, (uint32_t)sl_rects_1080[i][k]);
    write_fixture("Interface\\english\\sidebarFullHD.bmp", 117, 966);
    write_fixture("Interface\\english\\bottombarFullHD.bmp", 1920, 114);
    write_fixture("Interface\\gMainMenuOverlay1080.bmp", 1920, 1080);
    display_w = 2560;
    display_h = 1080;
    X86 c = {};
    c.r[R_EDX] = 0x1000; // another record is copied untouched
    siege_copy_record(&c);
    check(rd32(r + SL_SCREEN_W) == 1920, "another record's copy leaves the 1080 one", rd32(r),
          1920);
    c.r[R_EDX] = SIEGE_CFULLHD;
    siege_copy_record(&c);
    check(copies == 2, "the copy itself still runs", copies, 2);
    check(rd32(r + SL_SCREEN_W) == 2560, "screen width", rd32(r + SL_SCREEN_W), 2560);
    check(rd32(r + SL_SCREEN_H) == 1080, "screen height", rd32(r + SL_SCREEN_H), 1080);
    check(rd32(r + SL_PREMAP_W) == 2560, "map surface width", rd32(r + SL_PREMAP_W), 2560);
    check(rd32(r + SL_GAME_W) == 2463, "map viewport width", rd32(r + SL_GAME_W), 2463);
    check(rd32(r + SL_SPELLBAR_X) == 2443, "sidebar x", rd32(r + SL_SPELLBAR_X), 2443);
    check(rd32(r + SL_SPELLBAR_Y) == 966, "bar y", rd32(r + SL_SPELLBAR_Y), 966);
    check(rd32(r + SL_LIFE_EMPTY_X) == 2469, "life bar x", rd32(r + SL_LIFE_EMPTY_X), 2469);
    check(rd32(r + SL_RECTS + 16 * 1) == 2492, "map button left", rd32(r + SL_RECTS + 16), 2492);
    check(rd32(r + SL_RECTS + 16 * 2) == 1148, "quest button stays", rd32(r + SL_RECTS + 32), 1148);
    check(rd32(r + SL_RECTS + 16 * 8 + 8) == 2538, "stats right", rd32(r + SL_RECTS + 16 * 8 + 8),
          2538);
    check(modes_offered == 1, "the fitted mode is offered", modes_offered, 1);
    check(siege_layout_active.dx == 640, "the active growth", siege_layout_active.dx, 640);
    c.r[R_EDX] = SIEGE_CFULLHD;
    siege_copy_record(&c);
    check(rd32(r + SL_GAME_W) == 2463, "a second copy changes nothing", rd32(r + SL_GAME_W), 2463);

    // The bar: columns before 1700 as they were, the growth from the band
    // 1300..1700, the map button's columns moved right.
    SiegeImage bar;
    const std::string bar_path = host_path("profile", "Interface\\english\\bottombarFullHD.bmp");
    check(siege_bmp_read(bar_path.c_str(), &bar) && bar.w == 2560 && bar.h == 114,
          "the bottom bar is regenerated 2560 wide", bar.w, 2560);
    if (bar.px) {
        const auto col = [&](int x) { return (int)bar.px[(size_t)x * 3 + 2]; };
        check(col(1699) == 1699 * 251 / 1919, "a column before the growth", col(1699),
              1699 * 251 / 1919);
        check(col(1700) == 1300 * 251 / 1919, "the growth starts the band", col(1700),
              1300 * 251 / 1919);
        check(col(2559) == 1919 * 251 / 1919, "the map button's last column", col(2559), 251);
        free(bar.px);
    }
    SiegeImage back;
    const std::string back_path = host_path("profile", "Interface\\gMainMenuOverlay1080.bmp");
    check(siege_bmp_read(back_path.c_str(), &back) && back.w == 2560,
          "the backdrop is regenerated 2560 wide", back.w, 2560);
    if (back.px) {
        const int hole = (2560 - 800) / 2;
        check(back.px[(size_t)hole * 3 + 2] == 560 * 251 / 1919, "the hole's first column",
              back.px[(size_t)hole * 3 + 2], 560 * 251 / 1919);
        check(back.px[(size_t)(hole + 799) * 3 + 2] == 1359 * 251 / 1919, "the hole's last column",
              back.px[(size_t)(hole + 799) * 3 + 2], 1359 * 251 / 1919);
        free(back.px);
    }
    struct stat st;
    check(stat(host_path("profile", "Interface\\english\\sidebarFullHD.bmp").c_str(), &st) != 0,
          "the sidebar is not copied when only the width grows", 0, 0);

    // A tall display, as a later launch would see it.
    siege_layout_active = {1920, 1080, 0, 0};
    siege_fit_all_art(0, 262);
    SiegeImage side;
    const std::string side_path = host_path("profile", "Interface\\english\\sidebarFullHD.bmp");
    check(siege_bmp_read(side_path.c_str(), &side) && side.h == 1228,
          "the sidebar is regenerated 1228 tall", side.h, 1228);
    if (side.px) {
        check(source_row(side, 759) == 759, "a row before the growth", source_row(side, 759), 759);
        check(source_row(side, 760) == 660, "the growth repeats the last panel",
              source_row(side, 760), 660);
        check(source_row(side, 860) == 660, "a period later", source_row(side, 860), 660);
        check(source_row(side, 1227) == 965, "the inventory button's last row",
              source_row(side, 1227), 965);
        free(side.px);
    }
    check(stat(bar_path.c_str(), &st) != 0,
          "a bar grown for another shape is removed, and the game's own is read", 0, 0);
    siege_fit_all_art(0, 0);
    check(stat(side_path.c_str(), &st) != 0, "a 16:9 display removes the fitted sidebar", 0, 0);

    // The spell bar's rows, 262 lower.
    siege_layout_active = {1920, 1342, 0, 262};
    const uint32_t self = 0x2000, sp = 0x3000;
    c.r[R_EAX] = self;
    c.r[R_ESP] = sp;
    wr8(self + SIEGE_SPELLBAR_ACTIVE, 1);
    const auto move = [&](int32_t y, bool down) {
        wr32(sp + 4, (uint32_t)y);
        if (down)
            siege_form_mouse_down(&c);
        else
            siege_form_mouse_move(&c);
        return seen_y;
    };
    check(move(1233, true) == 971, "a click on the fitted bar", seen_y, 971);
    check(move(1336, false) == 1074, "the bar's last row", seen_y, 1074);
    check(move(1000, true) == 965, "the map above the bar is not the bar", seen_y, 965);
    check(move(1340, false) == 1340, "below the bar", seen_y, 1340);
    check(move(500, true) == 500, "the map", seen_y, 500);
    wr8(self + SIEGE_SPELLBAR_ACTIVE, 0);
    check(move(1233, true) == 1233, "a closed bar is left alone", seen_y, 1233);

    std::printf("%d failures\n", failures);
    return failures ? 1 : 0;
}
