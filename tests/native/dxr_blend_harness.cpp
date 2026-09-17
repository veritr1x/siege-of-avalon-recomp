// dxr_blend_harness.cpp - drives native/dxr_blend.h against hand-made
// TDXR_Surface records in a small arena, with the runtime reduced to the
// symbols the header's inline helpers reference. Prints one line per check
// and exits non-zero on any failure. tests/test_dxr_blend.py builds and runs it.
#include "dxr_blend.h"
#include <cstdio>
#include <cstdlib>
#include <vector>

static std::vector<uint8_t> arena(0x20000);
uint8_t *g_mem = arena.data();
uint32_t g_watch_base = 0, g_watch_len = 0;
RecompDirty g_dirty[RECOMP_DIRTY_SLOTS];
uint32_t g_dirty_count = 0;
void recomp_watch_hit(uint32_t, uint32_t, uint64_t) {}
static int returned = 0;
void recomp_callback_return(X86 *) { ++returned; }
int recomp_is_call_return(uint32_t) { return 0; }
int recomp_module_is_call_return(uint32_t) { return 0; }
int32_t recomp_index_of(uint32_t) { return -1; }
int recomp_module_lookup(uint32_t) { return -1; }
void recomp_call(X86 *, uint32_t) {}

static int failures = 0;
static void check(bool ok, const char *what, unsigned got, unsigned want) {
    std::printf("%s %s (got %04x, want %04x)\n", ok ? "ok  " : "FAIL", what, got, want);
    if (!ok)
        ++failures;
}

// An RGB565 surface: red 5 bits at 11, green 6 at 5, blue 5 at 0; the pixels
// follow the record.
static uint32_t surface(uint32_t at, uint32_t w, uint32_t h) {
    wr8(at + DXR_SURF_COLORTYPE, 1);
    wr32(at + DXR_SURF_WIDTH, w);
    wr32(at + DXR_SURF_HEIGHT, h);
    wr32(at + DXR_SURF_BITCOUNT, 16);
    wr32(at + DXR_SURF_BITS, at + 0x100);
    wr32(at + DXR_SURF_PITCH, w * 2);
    const uint32_t masks[3] = {0xf800, 0x07e0, 0x001f}, bits[3] = {5, 6, 5}, shifts[3] = {11, 5, 0};
    for (uint32_t i = 0; i < 3; ++i) {
        const uint32_t ch = at + DXR_SURF_CHANNELS + i * DXR_CHANNEL_STRIDE;
        wr32(ch + DXR_CHANNEL_MASK, masks[i]);
        wr32(ch + DXR_CHANNEL_BITCOUNT, bits[i]);
        wr32(ch + DXR_CHANNEL_RSHIFT, shifts[i]);
    }
    return at;
}
static void fill(uint32_t s, uint16_t v) {
    const uint32_t n = rd32(s + DXR_SURF_WIDTH) * rd32(s + DXR_SURF_HEIGHT);
    for (uint32_t i = 0; i < n; ++i)
        wr16(rd32(s + DXR_SURF_BITS) + i * 2, v);
}
static uint16_t pixel(uint32_t s, uint32_t x, uint32_t y) {
    return rd16(rd32(s + DXR_SURF_BITS) + (y * rd32(s + DXR_SURF_WIDTH) + x) * 2);
}
static void rect(uint32_t at, int32_t l, int32_t t, int32_t r, int32_t b) {
    wr32(at, uint32_t(l));
    wr32(at + 4, uint32_t(t));
    wr32(at + 8, uint32_t(r));
    wr32(at + 12, uint32_t(b));
}
// What a 0..255 value reads back as after a trip through an n-bit channel:
// dxr_ch_put rounds to the nearest step, dxr_ch_get scales the step down.
static uint32_t trip(uint32_t v, uint32_t bits) {
    const uint32_t top = (1u << bits) - 1;
    return ((v * top + 127) / 255) * 255 / top;
}
// A 565 word from 0..255 channels, rounded as dxr_ch_put rounds.
static uint16_t rgb(uint32_t r, uint32_t g, uint32_t b) {
    return uint16_t(((r * 31 + 127) / 255) << 11 | ((g * 63 + 127) / 255) << 5 | ((b * 31 + 127) / 255));
}

// Calls the override as the translated caller would: register arguments,
// then the stack parameters pushed left to right under the return address.
static void copy(uint32_t dst, uint32_t src, uint32_t blend, int32_t alpha, bool key_on, uint32_t key) {
    X86 c{};
    rect(0x10000, 0, 0, 2, 1);
    rect(0x10010, 0, 0, 2, 1);
    uint32_t sp = 0x1f000;
    wr32(sp + 0x14, 0x10010);
    wr32(sp + 0x10, blend);
    wr32(sp + 0x0c, uint32_t(alpha));
    wr32(sp + 0x08, key_on);
    wr32(sp + 0x04, key);
    wr32(sp, GUEST_RETURN_SENTINEL);
    c.r[R_ESP] = sp;
    c.r[R_EAX] = dst;
    c.r[R_EDX] = src;
    c.r[R_ECX] = 0x10000;
    returned = 0;
    siege_dxr_copy_rect_blend(&c);
    check(returned == 1 && c.r[R_ESP] == sp + 0x18, "copy returns past its five parameters",
          c.r[R_ESP] - sp, 0x18);
}
static void fill_blend(uint32_t dst, uint32_t blend, uint32_t col) {
    X86 c{};
    rect(0x10020, 0, 0, 2, 1);
    uint32_t sp = 0x1f000;
    wr32(sp + 0x04, col);
    wr32(sp, GUEST_RETURN_SENTINEL);
    c.r[R_ESP] = sp;
    c.r[R_EAX] = dst;
    c.r[R_EDX] = 0x10020;
    c.r[R_ECX] = blend;
    returned = 0;
    siege_dxr_fill_rect_color_blend(&c);
    check(returned == 1 && c.r[R_ESP] == sp + 0x08, "fill returns past its one parameter",
          c.r[R_ESP] - sp, 0x08);
}

int main() {
    const uint32_t dst = surface(0x1000, 2, 1), src = surface(0x4000, 2, 1);
    const uint16_t grey = rgb(200, 200, 200), dark = rgb(80, 80, 80);

    fill(dst, grey);
    fill(src, dark);
    copy(dst, src, DXR_BLEND_ONE1, 255, false, 0);
    check(pixel(dst, 0, 0) == dark, "ONE1 copies the source", pixel(dst, 0, 0), dark);

    // 200 and 80 as the blend sees them, through 5- and 6-bit channels.
    const uint32_t d5 = trip(200, 5), d6 = trip(200, 6), s5 = trip(80, 5), s6 = trip(80, 6);

    // DrawSub at full alpha: destination minus source, channel by channel.
    fill(dst, grey);
    copy(dst, src, DXR_BLEND_ONE2_SUB_ONE1, 255, false, 0);
    {
        const uint16_t want = rgb(d5 - s5, d6 - s6, d5 - s5);
        check(pixel(dst, 0, 0) == want, "ONE2_SUB_ONE1 subtracts the source", pixel(dst, 0, 0), want);
    }
    // A source brighter than the destination saturates at black.
    fill(dst, dark);
    fill(src, grey);
    copy(dst, src, DXR_BLEND_ONE2_SUB_ONE1, 255, false, 0);
    check(pixel(dst, 0, 0) == 0, "ONE2_SUB_ONE1 never goes below black", pixel(dst, 0, 0), 0);

    // DrawSub below full alpha: the source is scaled first, (s * a) >> 8.
    fill(dst, grey);
    fill(src, dark);
    copy(dst, src, DXR_BLEND_ONE2_SUB_SRCALPHA1, 170, false, 0);
    {
        const uint32_t r = d5 - (s5 * 170 >> 8), g = d6 - (s6 * 170 >> 8);
        const uint16_t want = rgb(r, g, r);
        check(pixel(dst, 0, 0) == want, "ONE2_SUB_SRCALPHA1 subtracts the scaled source",
              pixel(dst, 0, 0), want);
    }
    // Keyed source pixels leave the destination alone, whatever the blend.
    fill(dst, grey);
    fill(src, dark);
    copy(dst, src, DXR_BLEND_ONE2_SUB_SRCALPHA1, 170, true, dark);
    check(pixel(dst, 0, 0) == grey, "a keyed pixel is not subtracted", pixel(dst, 0, 0), grey);

    // FillRectSub: a constant colour subtracted from every pixel.
    fill(dst, grey);
    fill_blend(dst, DXR_BLEND_ONE2_SUB_ONE1, 0x00505050u); // COLORREF 80,80,80
    {
        const uint16_t want = rgb(d5 - 80, d6 - 80, d5 - 80); // the colour is not rounded
        check(pixel(dst, 1, 0) == want, "FillRectSub subtracts its colour", pixel(dst, 1, 0), want);
    }
    // FillRectAlpha: black at alpha 128 over grey.
    fill(dst, grey);
    fill_blend(dst, DXR_BLEND_SRCALPHA1_ADD_INVSRCALPHA2, 0x80000000u);
    {
        const uint32_t r = (d5 * 127 + 127) / 255, g = (d6 * 127 + 127) / 255;
        const uint16_t want = rgb(r, g, r);
        check(pixel(dst, 1, 0) == want, "FillRectAlpha blends its colour", pixel(dst, 1, 0), want);
    }
    std::printf("%d failures\n", failures);
    return failures ? 1 : 0;
}
