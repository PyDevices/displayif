// SPDX-License-Identifier: MIT
// MIMXRT1176 MIPI DSI + LCDIFV2 display bridge helpers.

#ifndef DISPLAYIF_MIMXRT1176_DSI_DISPLAY_H
#define DISPLAYIF_MIMXRT1176_DSI_DISPLAY_H

#include <stdint.h>
#include <stdbool.h>
#include "fsl_common.h"

typedef struct {
    uint16_t width;
    uint16_t height;
    uint32_t pixel_clock_hz;
    uint16_t hsw;
    uint16_t hfp;
    uint16_t hbp;
    uint16_t vsw;
    uint16_t vfp;
    uint16_t vbp;
    uint8_t num_lanes;
    uint32_t lane_bit_rate_hz;
} displayif_mimxrt1176_dsi_timings_t;

status_t displayif_mimxrt1176_dsi_bus_init(uint8_t num_lanes, uint32_t lane_bit_rate_hz);
void displayif_mimxrt1176_dsi_bus_deinit(void);

status_t displayif_mimxrt1176_dsi_display_start(const displayif_mimxrt1176_dsi_timings_t *timings,
    const uint8_t *init_sequence, size_t init_len, int reset_pin, int backlight_pin, bool backlight_on_high);
void displayif_mimxrt1176_dsi_display_stop(void);

void displayif_mimxrt1176_dsi_set_framebuffer(uint8_t *buf);
void displayif_mimxrt1176_dsi_lcdifv2_start(uint8_t *buf);
status_t displayif_mimxrt1176_dsi_send_init_sequence(const uint8_t *init_sequence, size_t init_len);

// TC358762 DSI-to-DPI bridge (Raspberry Pi / Waveshare 50H-800480-IPS class panels).
status_t displayif_mimxrt1176_dsi_generic_write_reg(uint16_t reg, uint32_t value);
status_t displayif_mimxrt1176_dsi_init_tc358762_bridge(uint8_t num_lanes);

#endif /* DISPLAYIF_MIMXRT1176_DSI_DISPLAY_H */
