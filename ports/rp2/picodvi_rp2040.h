// SPDX-License-Identifier: MIT
// PicoDVI / libdvi backend for RP2040 (PIO + multicore TMDS encode).

#ifndef PICODVI_RP2040_H
#define PICODVI_RP2040_H

#include "py/obj.h"

typedef struct _picodvi_framebuffer_obj_t {
    mp_obj_base_t base;
    uint16_t width;
    uint16_t height;
    uint8_t color_depth;
    uint32_t pitch;
    uint32_t framebuffer_len;
    uint32_t tmdsbuf_size;
    uint32_t *framebuffer;
    int8_t pin_pair[4];
    uint pwm_slice;
    uint tmds_lock;
    uint colour_lock;
    struct dvi_inst dvi;
    uint next_scanline;
} picodvi_framebuffer_obj_t;

bool picodvi_rp2040_construct(picodvi_framebuffer_obj_t *self, uint16_t width, uint16_t height,
    int clk_dp, int clk_dn, int red_dp, int red_dn, int green_dp, int green_dn, int blue_dp, int blue_dn,
    uint8_t color_depth);
void picodvi_rp2040_deinit(picodvi_framebuffer_obj_t *self);
bool picodvi_rp2040_active(void);

#endif /* PICODVI_RP2040_H */
