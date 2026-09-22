// SPDX-License-Identifier: MIT
// One RGB565 rectangle fill, shared by every backend that owns a framebuffer.
//
// Why this is worth a C function at all: displaydev's FBDisplay.fill_rect falls
// back to per-row memoryview assigns when the raw buffer has no native fill, and
// into PSRAM that is roughly 14 ms a row. On an ESP32-P4 Touch-LCD-4B a 100x100
// fill measured 1.387 s that way (displayif#28). The backend has the pointer and
// the stride; filling there is a loop.
//
// Every caller is 16bpp: displaydev's framebuffer path is RGB565 throughout
// (FBDisplay.fill_rect masks the colour with 0xFFFF), and a backend whose buffer
// is not 16bpp must not use these.

#ifndef DISPLAYIF_FB_FILL_H
#define DISPLAYIF_FB_FILL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Bounds, in one place so every backend refuses the same rectangles. w or h of
// zero is refused rather than silently doing nothing: it is a caller bug, and
// the display drivers never ask for one.
static inline bool displayif_fb_rect_ok(int x, int y, int w, int h, int width, int height) {
    return x >= 0 && y >= 0 && w > 0 && h > 0 && x + w <= width && y + h <= height;
}

// Fill w x h pixels at (x, y). The caller has already checked bounds, and owns
// whatever cache sync or refresh its hardware needs afterwards.
static inline void displayif_fb_fill_rect16(uint8_t *buf, size_t row_stride,
    int x, int y, int w, int h, uint16_t color) {
    for (int row = 0; row < h; row++) {
        uint16_t *dst = (uint16_t *)(buf + (size_t)(y + row) * row_stride + (size_t)x * sizeof(uint16_t));
        for (int col = 0; col < w; col++) {
            dst[col] = color;
        }
    }
}

// Fill w x h pixels at (x, y) in a buffer of `bpp` bits per pixel, where bpp
// may be smaller than a byte. For `picodvi.Framebuffer`, the one backend here
// that is not fixed at 16bpp: its `color_depth` is 1, 2, 4, 8, 16 or 32 and
// defaults to 8 (displayif#34). Everything else should use the 16bpp fill
// above, which is this one's fast path written out.
//
// --- which end of the byte is the LEFT pixel ------------------------------
//
// LSB first: the pixel at x=0 of a byte lives in its LOW bits. That is not a
// choice, it is read off the scanout. On RP2350 the HSTX command expander
// pops pixels out of a 32-bit word by SHIFTING IT RIGHT --
// `hstx_ctrl_hw->expand_shift`'s `ENC_SHIFT` is `color_depth` bits per pixel
// and the shift register empties from the bottom -- so the first pixel on the
// wire is the low bits, and the framebuffer is little-endian words, so byte 0
// of a word holds the leftmost pixels.
//
// Getting this backwards does not crash and does not look like a bug in this
// function: a fill comes out mirrored within each byte, which at 1bpp is a
// four-pixel stagger along an edge and at 4bpp is two swapped pixels. Nobody
// reading a screenshot would call that a fill_rect defect. It is stated here,
// once, so that the board bring-up has something to check rather than
// something to discover.
//
// The partial bytes at the two ends are the whole difficulty: a rectangle
// whose left or right edge falls inside a byte must not touch the pixels
// beside it, which belong to whatever was already on the screen.
static inline void displayif_fb_fill_rect_packed(uint8_t *buf, size_t row_stride,
    int x, int y, int w, int h, uint32_t color, int bpp) {
    if (bpp == 16) {
        displayif_fb_fill_rect16(buf, row_stride, x, y, w, h, (uint16_t)color);
        return;
    }
    if (bpp == 32) {
        for (int row = 0; row < h; row++) {
            uint32_t *dst = (uint32_t *)(buf + (size_t)(y + row) * row_stride)
                + (size_t)x;
            for (int col = 0; col < w; col++) {
                dst[col] = color;
            }
        }
        return;
    }
    if (bpp == 8) {
        for (int row = 0; row < h; row++) {
            uint8_t *dst = buf + (size_t)(y + row) * row_stride + (size_t)x;
            for (int col = 0; col < w; col++) {
                dst[col] = (uint8_t)color;
            }
        }
        return;
    }
    // 1, 2 or 4 bits a pixel.
    const int per_byte = 8 / bpp;
    const uint8_t pixel_mask = (uint8_t)((1u << bpp) - 1u);
    const uint8_t value = (uint8_t)(color & pixel_mask);
    uint8_t whole = 0;
    for (int slot = 0; slot < per_byte; slot++) {
        whole = (uint8_t)(whole | (uint8_t)(value << (slot * bpp)));
    }
    const int end = x + w;
    for (int row = 0; row < h; row++) {
        uint8_t *line = buf + (size_t)(y + row) * row_stride;
        int px = x;
        // The partial byte at the left, if the rectangle starts inside one.
        while (px < end && (px % per_byte) != 0) {
            const int shift = (px % per_byte) * bpp;
            line[px / per_byte] = (uint8_t)((line[px / per_byte]
                & (uint8_t)~(pixel_mask << shift)) | (uint8_t)(value << shift));
            px++;
        }
        // Every byte the rectangle owns outright.
        while (px + per_byte <= end) {
            line[px / per_byte] = whole;
            px += per_byte;
        }
        // And the partial byte at the right.
        while (px < end) {
            const int shift = (px % per_byte) * bpp;
            line[px / per_byte] = (uint8_t)((line[px / per_byte]
                & (uint8_t)~(pixel_mask << shift)) | (uint8_t)(value << shift));
            px++;
        }
    }
}

#endif // DISPLAYIF_FB_FILL_H
