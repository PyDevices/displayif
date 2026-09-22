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

#endif // DISPLAYIF_FB_FILL_H
