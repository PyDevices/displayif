// SPDX-License-Identifier: MIT
// displayif_fb_fill_rect_packed against a naive per-pixel reference.
//
//   cc -I src/include -o /tmp/t tests/test_fb_fill_packed.c && /tmp/t
//
// Why a C test and not a board: `picodvi.Framebuffer` is the one framebuffer
// backend here that is not fixed at 16bpp -- `color_depth` is 1, 2, 4, 8, 16
// or 32 -- so its fill needs partial-byte masking at the left and right edges
// that none of the other five has to think about (displayif#34). That masking
// is arithmetic, it is where this will be wrong if it is wrong, and it does
// not need a DVI monitor to check.
//
// What it does NOT prove is on the issue and in the header: that the LOW bits
// of a byte are the LEFT pixel. That is read off the RP2350 scanout and is
// owed a look at a real screen.
//
// The reference is deliberately stupid -- one pixel at a time, no cleverness
// to share a bug with the thing under test -- and the canary bytes around
// every buffer are there because the first failure this kind of code has is
// writing outside the rectangle, not inside it.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "displayif/fb_fill.h"

#define WIDTH 61                 // not a multiple of anything: every depth's
#define HEIGHT 9                 // partial byte gets exercised by the edges
#define GUARD 16
#define STRIDE 64                // bytes, > WIDTH * 32bpp / 8 is not needed:
                                 // the widest depth gets its own stride below

static int failures;

static void ref_set(uint8_t *buf, size_t stride, int x, int y, uint32_t color,
    int bpp) {
    if (bpp == 32) {
        ((uint32_t *)(buf + (size_t)y * stride))[x] = color;
    } else if (bpp == 16) {
        ((uint16_t *)(buf + (size_t)y * stride))[x] = (uint16_t)color;
    } else if (bpp == 8) {
        buf[(size_t)y * stride + (size_t)x] = (uint8_t)color;
    } else {
        const int per_byte = 8 / bpp;
        const uint8_t mask = (uint8_t)((1u << bpp) - 1u);
        const int shift = (x % per_byte) * bpp;
        uint8_t *cell = &buf[(size_t)y * stride + (size_t)(x / per_byte)];
        *cell = (uint8_t)((*cell & (uint8_t)~(mask << shift))
            | (uint8_t)(((uint8_t)color & mask) << shift));
    }
}

static void check(int bpp, size_t stride, int x, int y, int w, int h,
    uint32_t color) {
    const size_t bytes = stride * HEIGHT;
    uint8_t *got = malloc(bytes + 2 * GUARD);
    uint8_t *want = malloc(bytes + 2 * GUARD);
    // A noisy start: a fill that is right must overwrite exactly its own
    // pixels, and a zeroed buffer cannot tell "wrote a 0" from "left alone".
    for (size_t i = 0; i < bytes + 2 * GUARD; i++) {
        got[i] = want[i] = (uint8_t)(0xA5 ^ (i * 31));
    }

    displayif_fb_fill_rect_packed(got + GUARD, stride, x, y, w, h, color, bpp);
    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            ref_set(want + GUARD, stride, x + col, y + row, color, bpp);
        }
    }

    if (memcmp(got, want, bytes + 2 * GUARD) != 0) {
        size_t at = 0;
        while (at < bytes + 2 * GUARD && got[at] == want[at]) {
            at++;
        }
        const char *where = at < GUARD ? " (BEFORE the buffer)"
            : (at >= GUARD + bytes ? " (AFTER the buffer)" : "");
        printf("FAIL bpp=%2d rect=(%d,%d %dx%d) colour=0x%x: first differing "
               "byte %zu%s, got 0x%02x want 0x%02x\n",
               bpp, x, y, w, h, (unsigned)color, at, where, got[at], want[at]);
        failures++;
    }
    free(got);
    free(want);
}

int main(void) {
    const int depths[] = {1, 2, 4, 8, 16, 32};
    int cases = 0;
    for (size_t d = 0; d < sizeof(depths) / sizeof(depths[0]); d++) {
        const int bpp = depths[d];
        // One row, rounded up to a word, exactly as picodvi sizes its pitch.
        const size_t row_bytes = ((size_t)WIDTH * bpp + 7) / 8;
        const size_t stride = ((row_bytes + 3) / 4) * 4;
        const uint32_t colour = bpp >= 16 ? 0xF81F3C7Au : 0xFFFFFFFFu;
        // Every start and every width the byte boundaries can produce, which
        // for 1bpp is eight of each -- so this walks them all rather than
        // picking a few and hoping the interesting one was among them.
        for (int x = 0; x < 17; x++) {
            for (int w = 1; w + x <= WIDTH && w < 25; w++) {
                for (int y = 0; y < 2; y++) {
                    check(bpp, stride, x, y, w, HEIGHT - y - 1, colour);
                    cases++;
                }
            }
        }
        // And a colour that is not all-ones, which is the one that catches a
        // fill that ORs instead of replacing.
        check(bpp, stride, 3, 1, 21, 4, 1u);
        check(bpp, stride, 0, 0, WIDTH, HEIGHT, 0u);
        cases += 2;
    }
    printf("%d cases over %zu depths: %d failures\n", cases,
           sizeof(depths) / sizeof(depths[0]), failures);
    return failures ? 1 : 0;
}
