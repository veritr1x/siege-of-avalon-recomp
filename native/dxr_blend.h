// dxr_blend.h - native replacements for the game's software blend.
//
// Every alpha-drawn thing in this game's interface goes through
// DXEffects.DrawAlpha, which calls DXRender.dxrCopyRectBlend. That routine
// COMPILES a per-pixel loop into its own machine at run time and jumps to it.
// Translated code cannot run code the guest generated, so the call does
// nothing at all: the training-style list, the character stat values and
// every dimmed panel simply never appear, while text drawn without alpha
// (BltFast with a colour key) is fine.
//
// The replacement does the same blit natively. The address below is this
// build's dxrCopyRectBlend; game.toml names this header as its [translate]
// overrides, so funcs.h defines FN_<addr> to the function here and every call
// site, tail call and jump-table case follows.
#pragma once
#include "x86.h"

// The generated sources see x86.h only, so the arena bound is spelled out
// here rather than taken from the runtime's own guest.h.
static inline int dxr_in_arena(uint32_t at, uint32_t n) {
    return at != 0 && at < GUEST_SIZE && n <= GUEST_SIZE - at;
}

// TDXR_Surface (graphics/DXRender.pas). The record is PACKED and its first
// field is a one-byte enum, so every dword after it sits at an odd offset:
// the routine at 0x00a321c0 reads Width at +1 and Height at +5, which is what
// fixed these numbers. Reading them off the source's field order alone gives
// a layout four bytes out and a blit that draws nothing.
enum {
    DXR_SURF_COLORTYPE = 0,  // 1 byte
    DXR_SURF_WIDTH = 1,
    DXR_SURF_HEIGHT = 5,
    DXR_SURF_WIDTHBIT = 9,
    DXR_SURF_HEIGHTBIT = 13,
    DXR_SURF_WIDTH2 = 17,
    DXR_SURF_HEIGHT2 = 21,
    DXR_SURF_WIDTHMASK = 25,
    DXR_SURF_HEIGHTMASK = 29,
    DXR_SURF_BITCOUNT = 33,
    DXR_SURF_BITS = 37,
    DXR_SURF_PITCH = 41,
    DXR_SURF_PITCHBIT = 45,
    DXR_SURF_MIPMAP = 49,
    // The variant part. Indexed surfaces start with an index channel; RGB
    // ones start with red. Each TDXR_ColorChannel is {Mask, BitCount, rshift,
    // lshift}, sixteen bytes.
    DXR_SURF_CHANNELS = 53,
    DXR_CHANNEL_STRIDE = 16,
    DXR_CHANNEL_MASK = 0,
    DXR_CHANNEL_BITCOUNT = 4,
    DXR_CHANNEL_RSHIFT = 8,
    DXR_CHANNEL_LSHIFT = 12,
};

// TDXR_Blend, in declaration order (DXRender.pas:40). DrawAlpha uses only the
// first two: a straight copy for an indexed destination or a fully opaque
// alpha, and the constant-alpha blend otherwise.
enum {
    DXR_BLEND_ZERO = 0,
    DXR_BLEND_ONE1 = 1,
    DXR_BLEND_SRCALPHA1_ADD_INVSRCALPHA2 = 10,
};

// One 16-bit channel's value, expanded to 0..255.
static inline uint32_t dxr_channel_get(uint32_t surface, uint32_t index, uint32_t pixel) {
    const uint32_t ch = surface + DXR_SURF_CHANNELS + index * DXR_CHANNEL_STRIDE;
    const uint32_t mask = rd32(ch + DXR_CHANNEL_MASK);
    const uint32_t bits = rd32(ch + DXR_CHANNEL_BITCOUNT);
    if (!mask || !bits)
        return 0;
    const uint32_t rshift = rd32(ch + DXR_CHANNEL_RSHIFT);
    const uint32_t value = (pixel & mask) >> rshift;
    const uint32_t top = (1u << bits) - 1u;
    return top ? value * 255u / top : 0u;
}

// The inverse: 0..255 back into the surface's own channel.
static inline uint32_t dxr_channel_put(uint32_t surface, uint32_t index, uint32_t value) {
    const uint32_t ch = surface + DXR_SURF_CHANNELS + index * DXR_CHANNEL_STRIDE;
    const uint32_t mask = rd32(ch + DXR_CHANNEL_MASK);
    const uint32_t bits = rd32(ch + DXR_CHANNEL_BITCOUNT);
    if (!mask || !bits)
        return 0;
    const uint32_t rshift = rd32(ch + DXR_CHANNEL_RSHIFT);
    const uint32_t top = (1u << bits) - 1u;
    return (((value * top + 127u) / 255u) << rshift) & mask;
}

// The blit itself, in guest memory. Source and destination are guest
// addresses of TDXR_Surface records; the rectangles are guest TRects.
//
// Scaling matches the original: the increments are 16.16 fixed point, built
// from the ratio of source span to destination span, and the source is
// sampled at (sx >> 16, sy >> 16). Clipping is by the destination surface's
// own bounds, which is what BltClip did before the compiled loop ran.
static void dxr_blit_blend(uint32_t dst, uint32_t src, const int32_t dr[4], const int32_t sr[4],
                           uint32_t blend, int32_t alpha, int color_key_enable,
                           uint32_t color_key) {
    const uint32_t dst_bits = rd32(dst + DXR_SURF_BITS), src_bits = rd32(src + DXR_SURF_BITS);
    const int32_t dst_pitch = (int32_t)rd32(dst + DXR_SURF_PITCH);
    const int32_t src_pitch = (int32_t)rd32(src + DXR_SURF_PITCH);
    const uint32_t dst_bpp = rd32(dst + DXR_SURF_BITCOUNT);
    const uint32_t src_bpp = rd32(src + DXR_SURF_BITCOUNT);
    const int32_t dst_w = (int32_t)rd32(dst + DXR_SURF_WIDTH);
    const int32_t dst_h = (int32_t)rd32(dst + DXR_SURF_HEIGHT);
    const int32_t src_w = (int32_t)rd32(src + DXR_SURF_WIDTH);
    const int32_t src_h = (int32_t)rd32(src + DXR_SURF_HEIGHT);
    if (!dst_bits || !src_bits || dst_bpp != 16 || src_bpp != 16)
        return; // this game's surfaces are 16-bit; anything else keeps the original behaviour
    int32_t dl = dr[0], dt = dr[1], drr = dr[2], db = dr[3];
    const int32_t sl = sr[0], st = sr[1], sright = sr[2], sbottom = sr[3];
    if (dl >= drr || dt >= db || sl >= sright || st >= sbottom)
        return;
    if (sl < 0 || st < 0 || sright > src_w || sbottom > src_h)
        return;
    const int64_t inc_x = ((int64_t)(sright - sl) << 16) / (drr - dl);
    const int64_t inc_y = ((int64_t)(sbottom - st) << 16) / (db - dt);
    // Clip against the destination, carrying the source origin with it.
    int64_t sx0 = (int64_t)sl << 16, sy0 = (int64_t)st << 16;
    if (dl < 0) {
        sx0 += -(int64_t)dl * inc_x;
        dl = 0;
    }
    if (dt < 0) {
        sy0 += -(int64_t)dt * inc_y;
        dt = 0;
    }
    if (drr > dst_w)
        drr = dst_w;
    if (db > dst_h)
        db = dst_h;
    if (dl >= drr || dt >= db)
        return;

    const uint32_t a = alpha < 0 ? 0u : (alpha > 255 ? 255u : (uint32_t)alpha);
    const int straight = blend != DXR_BLEND_SRCALPHA1_ADD_INVSRCALPHA2 || a >= 255;
    int64_t sy = sy0;
    for (int32_t y = dt; y < db; ++y, sy += inc_y) {
        const int32_t syi = (int32_t)(sy >> 16);
        if (syi < 0 || syi >= src_h)
            continue;
        int64_t sx = sx0;
        for (int32_t x = dl; x < drr; ++x, sx += inc_x) {
            const int32_t sxi = (int32_t)(sx >> 16);
            if (sxi < 0 || sxi >= src_w)
                continue;
            const uint32_t sp = src_bits + (uint32_t)(syi * src_pitch) + (uint32_t)(sxi * 2);
            const uint32_t dp = dst_bits + (uint32_t)(y * dst_pitch) + (uint32_t)(x * 2);
            if (!dxr_in_arena(sp, 2) || !dxr_in_arena(dp, 2))
                return;
            const uint32_t s = rd16(sp);
            if (color_key_enable && s == (color_key & 0xffffu))
                continue;
            if (straight) {
                wr16(dp, (uint16_t)s);
                continue;
            }
            const uint32_t d = rd16(dp);
            uint32_t out = 0;
            for (uint32_t ch = 0; ch < 3; ++ch) {
                const uint32_t cs = dxr_channel_get(src, ch, s);
                const uint32_t cd = dxr_channel_get(dst, ch, d);
                out |= dxr_channel_put(dst, ch, (cs * a + cd * (255u - a) + 127u) / 255u);
            }
            wr16(dp, (uint16_t)out);
        }
    }
}

// dxrCopyRectBlend at 0x00a321c0 in the pinned 2021 build.
//
// Delphi's register convention: Self-less unit procedure, so the first three
// parameters arrive in EAX, EDX and ECX and the rest on the stack, pushed
// left to right, with the callee clearing them - the body ends RET 0x14, so
// five stack parameters. Confirmed against the listing: it reads SrcRect from
// [EBP+0x18], Blend and ColorKeyEnable as bytes from [EBP+0x14] and [EBP+0xc],
// and Alpha and ColorKey as dwords from [EBP+0x10] and [EBP+8]. At entry ESP
// points at the return address, which is four less than EBP inside the body.
static void siege_dxr_copy_rect_blend(X86 *c) {
    const uint32_t sp = c->r[R_ESP];
    const uint32_t ret = rd32(sp);
    const uint32_t dst = c->r[R_EAX], src = c->r[R_EDX], dst_rect = c->r[R_ECX];
    const uint32_t src_rect = rd32(sp + 0x14);
    const uint32_t blend = rd32(sp + 0x10) & 0xffu;
    const int32_t alpha = (int32_t)rd32(sp + 0x0c);
    const uint32_t key_on = rd32(sp + 0x08) & 0xffu;
    const uint32_t key = rd32(sp + 0x04);
    if (dst && src && dst_rect && src_rect && dxr_in_arena(dst_rect, 16) && dxr_in_arena(src_rect, 16) &&
        dxr_in_arena(dst, DXR_SURF_CHANNELS + 4 * DXR_CHANNEL_STRIDE) &&
        dxr_in_arena(src, DXR_SURF_CHANNELS + 4 * DXR_CHANNEL_STRIDE)) {
        int32_t dr[4], sr[4];
        for (int i = 0; i < 4; ++i) {
            dr[i] = (int32_t)rd32(dst_rect + 4u * (uint32_t)i);
            sr[i] = (int32_t)rd32(src_rect + 4u * (uint32_t)i);
        }
        dxr_blit_blend(dst, src, dr, sr, blend, alpha, key_on != 0, key);
    }
    // Return as the original does: the five stack parameters go with it.
    c->r[R_ESP] = sp + 4u + 0x14u;
    c->eip = ret;
    recomp_return(c);
}

#define FN_00a321c0 siege_dxr_copy_rect_blend
