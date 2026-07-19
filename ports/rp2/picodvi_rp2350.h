// SPDX-License-Identifier: MIT

#ifndef PICODVI_RP2350_H
#define PICODVI_RP2350_H

#include "py/obj.h"
#include <stddef.h>
#include <stdint.h>

typedef struct {
    mp_obj_base_t base;
    uint32_t *framebuffer;
    size_t framebuffer_len;
    uint32_t *dma_commands;
    size_t dma_commands_len;
    uint16_t width;
    uint16_t height;
    uint16_t output_width;
    uint16_t pitch;
    uint8_t color_depth;
    int dma_pixel_channel;
    int dma_command_channel;
} picodvi_framebuffer_obj_t;

void picodvi_rp2350_construct(picodvi_framebuffer_obj_t *self, uint16_t width, uint16_t height,
    int clk_dp, int clk_dn, int red_dp, int red_dn, int green_dp, int green_dn, int blue_dp, int blue_dn,
    uint8_t color_depth);
void picodvi_rp2350_deinit(picodvi_framebuffer_obj_t *self);
void picodvi_rp2350_force_teardown(void);
bool picodvi_rp2350_active(void);

#endif /* PICODVI_RP2350_H */
