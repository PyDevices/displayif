// SPDX-License-Identifier: MIT

#include <string.h>
#include <stdlib.h>

#include "py/runtime.h"
#include "py/mphal.h"
#include "displayif/soft_reset.h"
#include "picodvi_rp2040.h"

#if defined(PICO_RP2040) && !defined(PICO_RP2350)

#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/structs/mpu.h"
#include "hardware/pio.h"
#include "hardware/pwm.h"
#include "hardware/sync.h"
#include "hardware/vreg.h"
#include "pico/multicore.h"
#include "RP2040.h"

#include "picodvi/libdvi/dvi.h"
#include "picodvi/libdvi/tmds_encode.h"

static picodvi_framebuffer_obj_t *active_picodvi;

typedef struct {
    bool hw_live;
    int8_t pin_pair[4];
    uint pwm_slice;
    uint tmds_lock;
    uint colour_lock;
    int dma_data[N_TMDS_LANES];
    int dma_ctrl[N_TMDS_LANES];
} picodvi_hw_shadow_t;

static picodvi_hw_shadow_t s_hw;
static bool s_soft_reset_registered;

uint8_t dvi_vertical_repeat;
bool dvi_monochrome_tmds;

static void __not_in_flash_func(picodvi_core1_main)(void) {
    picodvi_framebuffer_obj_t *self = active_picodvi;
    dvi_register_irqs_this_core(&self->dvi, DMA_IRQ_1);

    while (queue_is_empty(&self->dvi.q_colour_valid)) {
        __wfe();
    }
    dvi_start(&self->dvi);

    MPU->CTRL = MPU_CTRL_PRIVDEFENA_Msk | MPU_CTRL_ENABLE_Msk;
    MPU->RNR = 6;
    MPU->RBAR = XIP_MAIN_BASE | MPU_RBAR_VALID_Msk;
    MPU->RASR = MPU_RASR_XN_Msk | MPU_RASR_ENABLE_Msk | (0x1b << MPU_RASR_SIZE_Pos);
    MPU->RNR = 7;

    uint y = 0;
    while (1) {
        uint32_t *scanbuf;
        queue_remove_blocking_u32(&self->dvi.q_colour_valid, &scanbuf);

        uint32_t *tmdsbuf;
        queue_remove_blocking_u32(&self->dvi.q_tmds_free, &tmdsbuf);
        size_t old_fb = tmdsbuf[self->tmdsbuf_size - 1];
        if (old_fb != (uint32_t)self->framebuffer) {
            size_t index = ((uint32_t)(tmdsbuf - old_fb)) / self->tmdsbuf_size;
            while (index >= DVI_N_TMDS_BUFFERS) {
            }
            tmdsbuf = self->framebuffer + self->framebuffer_len + (self->tmdsbuf_size * index);
            tmdsbuf[self->tmdsbuf_size - 1] = (uint32_t)self->framebuffer;
        }
        uint pixwidth = self->dvi.timing->h_active_pixels;
        uint words_per_channel = pixwidth / DVI_SYMBOLS_PER_WORD;
        if (self->color_depth == 8) {
            tmds_encode_data_channel_8bpp(scanbuf, tmdsbuf + 0 * words_per_channel, pixwidth / 2, DVI_8BPP_BLUE_MSB, DVI_8BPP_BLUE_LSB);
            tmds_encode_data_channel_8bpp(scanbuf, tmdsbuf + 1 * words_per_channel, pixwidth / 2, DVI_8BPP_GREEN_MSB, DVI_8BPP_GREEN_LSB);
            tmds_encode_data_channel_8bpp(scanbuf, tmdsbuf + 2 * words_per_channel, pixwidth / 2, DVI_8BPP_RED_MSB, DVI_8BPP_RED_LSB);
        } else if (self->color_depth == 16) {
            tmds_encode_data_channel_16bpp(scanbuf, tmdsbuf + 0 * words_per_channel, pixwidth / 2, DVI_16BPP_BLUE_MSB, DVI_16BPP_BLUE_LSB);
            tmds_encode_data_channel_16bpp(scanbuf, tmdsbuf + 1 * words_per_channel, pixwidth / 2, DVI_16BPP_GREEN_MSB, DVI_16BPP_GREEN_LSB);
            tmds_encode_data_channel_16bpp(scanbuf, tmdsbuf + 2 * words_per_channel, pixwidth / 2, DVI_16BPP_RED_MSB, DVI_16BPP_RED_LSB);
        } else if (self->color_depth == 1) {
            tmds_encode_1bpp(scanbuf, tmdsbuf, pixwidth);
        } else if (self->color_depth == 2) {
            tmds_encode_2bpp(scanbuf, tmdsbuf, pixwidth);
        }
        queue_add_blocking_u32(&self->dvi.q_tmds_valid, &tmdsbuf);
        queue_add_blocking_u32(&self->dvi.q_colour_free, &scanbuf);
        ++y;
        if (y == self->dvi.timing->v_active_lines) {
            y = 0;
        }
    }
    __builtin_unreachable();
}

static void __not_in_flash_func(picodvi_core1_scanline_callback)(void) {
    picodvi_framebuffer_obj_t *self = active_picodvi;
    uint32_t *next_scanline_buf = self->framebuffer + (self->pitch * self->next_scanline);
    queue_add_blocking_u32(&self->dvi.q_colour_valid, &next_scanline_buf);
    while (queue_try_remove_u32(&self->dvi.q_colour_free, &next_scanline_buf)) {
    }
    self->next_scanline += 1;
    if (self->next_scanline >= self->height) {
        self->next_scanline = 0;
    }
}

static void picodvi_turn_off_dma(uint8_t channel) {
    dma_channel_config c = dma_channel_get_default_config(channel);
    channel_config_set_enable(&c, false);
    dma_channel_set_config(channel, &c, false);
    if (dma_channel_is_busy(channel)) {
        dma_channel_abort(channel);
    }
    dma_channel_set_irq1_enabled(channel, false);
    dma_channel_unclaim(channel);
}

static void picodvi_hw_stop(void) {
    if (!s_hw.hw_live) {
        return;
    }
    spin_lock_t *tmds_lock = spin_lock_instance(s_hw.tmds_lock);
    spin_lock_t *colour_lock = spin_lock_instance(s_hw.colour_lock);
    uint32_t tmds_save = spin_lock_blocking(tmds_lock);
    uint32_t colour_save = spin_lock_blocking(colour_lock);
    multicore_reset_core1();
    spin_unlock(colour_lock, colour_save);
    spin_unlock(tmds_lock, tmds_save);

    for (int i = 0; i < N_TMDS_LANES; ++i) {
        picodvi_turn_off_dma(s_hw.dma_data[i]);
        picodvi_turn_off_dma(s_hw.dma_ctrl[i]);
    }

    pwm_set_enabled(s_hw.pwm_slice, false);
    for (size_t i = 0; i < 4; i++) {
        gpio_deinit(s_hw.pin_pair[i]);
        gpio_deinit(s_hw.pin_pair[i] + 1);
    }

    spin_lock_unclaim(s_hw.tmds_lock);
    spin_lock_unclaim(s_hw.colour_lock);
    active_picodvi = NULL;
    s_hw.hw_live = false;
}

static void picodvi_ensure_soft_reset_registered(void) {
    if (!s_soft_reset_registered) {
        displayif_register_soft_reset(picodvi_hw_stop);
        s_soft_reset_registered = true;
    }
}

void picodvi_rp2040_force_teardown(void) {
    picodvi_hw_stop();
}

bool picodvi_rp2040_active(void) {
    return s_hw.hw_live;
}

bool picodvi_rp2040_construct(picodvi_framebuffer_obj_t *self, uint16_t width, uint16_t height,
    int clk_dp, int clk_dn, int red_dp, int red_dn, int green_dp, int green_dn, int blue_dp, int blue_dn,
    uint8_t color_depth) {

    if (s_hw.hw_live) {
        picodvi_hw_stop();
    }
    picodvi_ensure_soft_reset_registered();

    bool color_framebuffer = color_depth >= 8;
    const struct dvi_timing *timing = NULL;
    if ((width == 640 && height == 480) || (width == 320 && height == 240) || (width == 640 && height == 240)) {
        timing = &dvi_timing_640x480p_60hz;
    } else if ((width == 800 && height == 480) || (width == 400 && height == 240) || (width == 800 && height == 240)) {
        timing = &dvi_timing_800x480p_60hz;
    } else {
        if (height != 480 && height != 240) {
            mp_raise_ValueError(MP_ERROR_TEXT("Invalid height"));
        }
        mp_raise_ValueError(MP_ERROR_TEXT("Invalid width"));
    }

    if ((width > 400) == color_framebuffer || color_depth == 4 || color_depth == 32) {
        mp_raise_ValueError(MP_ERROR_TEXT("Invalid color_depth"));
    }

    bool invert_diffpairs = clk_dn < clk_dp;
    int8_t other_pins[4];
    int8_t *a = invert_diffpairs ? other_pins : self->pin_pair;
    int8_t *b = invert_diffpairs ? self->pin_pair : other_pins;
    a[0] = clk_dp;
    a[1] = red_dp;
    a[2] = green_dp;
    a[3] = blue_dp;
    b[0] = clk_dn;
    b[1] = red_dn;
    b[2] = green_dn;
    b[3] = blue_dn;
    for (size_t i = 0; i < 4; i++) {
        if (other_pins[i] - self->pin_pair[i] != 1) {
            mp_raise_ValueError(MP_ERROR_TEXT("DVI pin pairs must be neighboring"));
        }
    }

    uint8_t slice = pwm_gpio_to_slice_num(self->pin_pair[0]);

    pio_program_t program_struct = {
        .instructions = NULL,
        .length = 2,
        .origin = -1,
    };
    size_t pio_index = NUM_PIOS;
    int free_state_machines[4];
    for (size_t i = 0; i < NUM_PIOS; i++) {
        PIO pio = pio_get_instance(i);
        uint8_t free_count = 0;
        for (size_t sm = 0; sm < NUM_PIO_STATE_MACHINES; sm++) {
            if (!pio_sm_is_claimed(pio, sm)) {
                free_state_machines[free_count++] = sm;
            }
        }
        if (free_count >= 3 && pio_can_add_program(pio, &program_struct)) {
            pio_index = i;
            break;
        }
    }
    if (pio_index == NUM_PIOS) {
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("All state machines in use"));
    }

    self->width = width;
    self->height = height;
    self->color_depth = color_depth;

    size_t tmds_bufs_per_scanline = color_framebuffer ? 3 : 1;
    size_t scanline_width = color_framebuffer ? width * 2 : width;
    dvi_monochrome_tmds = !color_framebuffer;
    dvi_vertical_repeat = timing->v_active_lines / self->height;
    self->pitch = (self->width * color_depth) / 8;
    if (self->pitch % sizeof(uint32_t) != 0) {
        self->pitch += sizeof(uint32_t) - (self->pitch % sizeof(uint32_t));
    }
    self->pitch /= sizeof(uint32_t);
    size_t framebuffer_size = self->pitch * self->height;
    self->tmdsbuf_size = tmds_bufs_per_scanline * scanline_width / DVI_SYMBOLS_PER_WORD + 1;
    size_t total_allocation_size = sizeof(uint32_t) * (framebuffer_size + DVI_N_TMDS_BUFFERS * self->tmdsbuf_size);
    self->framebuffer = (uint32_t *)m_malloc(total_allocation_size);
    if (self->framebuffer == NULL) {
        mp_raise_msg(&mp_type_MemoryError, MP_ERROR_TEXT("picodvi framebuffer allocation failed"));
    }
    memset(self->framebuffer, 0, total_allocation_size);

    gpio_set_function(self->pin_pair[0], GPIO_FUNC_PWM);
    gpio_set_function(self->pin_pair[0] + 1, GPIO_FUNC_PWM);
    pwm_config cfg = pwm_get_default_config();
    pwm_config_set_clkdiv(&cfg, 1);
    pwm_init(slice, &cfg, true);

    for (size_t i = 0; i < 4; i++) {
        gpio_set_function(self->pin_pair[i], GPIO_FUNC_PIO0 + pio_index);
        gpio_set_function(self->pin_pair[i] + 1, GPIO_FUNC_PIO0 + pio_index);
    }

    self->framebuffer_len = framebuffer_size;
    self->dvi.timing = timing;
    self->dvi.ser_cfg.pio = pio_get_instance(pio_index);
    self->dvi.ser_cfg.sm_tmds[0] = free_state_machines[0];
    self->dvi.ser_cfg.sm_tmds[1] = free_state_machines[1];
    self->dvi.ser_cfg.sm_tmds[2] = free_state_machines[2];
    self->dvi.ser_cfg.pins_clk = self->pin_pair[0];
    self->dvi.ser_cfg.pins_tmds[0] = self->pin_pair[1];
    self->dvi.ser_cfg.pins_tmds[1] = self->pin_pair[2];
    self->dvi.ser_cfg.pins_tmds[2] = self->pin_pair[3];
    self->dvi.ser_cfg.invert_diffpairs = invert_diffpairs;
    self->dvi.scanline_callback = picodvi_core1_scanline_callback;

    vreg_set_voltage(VREG_VOLTAGE_1_20);
    mp_hal_delay_ms(10);
    set_sys_clock_khz(timing->bit_clk_khz, true);
    self->pwm_slice = slice;
    self->tmds_lock = spin_lock_claim_unused(true);
    self->colour_lock = spin_lock_claim_unused(true);
    dvi_init(&self->dvi, self->tmds_lock, self->colour_lock);

    for (int i = 0; i < DVI_N_TMDS_BUFFERS; ++i) {
        uint32_t *tmdsbuf = self->framebuffer + (self->framebuffer_len + self->tmdsbuf_size * i);
        tmdsbuf[self->tmdsbuf_size - 1] = (uint32_t)self->framebuffer;
        queue_add_blocking_u32(&self->dvi.q_tmds_free, &tmdsbuf);
    }

    active_picodvi = self;
    multicore_launch_core1(picodvi_core1_main);

    s_hw.pin_pair[0] = self->pin_pair[0];
    s_hw.pin_pair[1] = self->pin_pair[1];
    s_hw.pin_pair[2] = self->pin_pair[2];
    s_hw.pin_pair[3] = self->pin_pair[3];
    s_hw.pwm_slice = self->pwm_slice;
    s_hw.tmds_lock = self->tmds_lock;
    s_hw.colour_lock = self->colour_lock;
    for (int i = 0; i < N_TMDS_LANES; ++i) {
        s_hw.dma_data[i] = self->dvi.dma_cfg[i].chan_data;
        s_hw.dma_ctrl[i] = self->dvi.dma_cfg[i].chan_ctrl;
    }
    s_hw.hw_live = true;

    self->next_scanline = 0;
    uint32_t *next_scanline_buf = self->framebuffer + (self->pitch * self->next_scanline);
    queue_add_blocking_u32(&self->dvi.q_colour_valid, &next_scanline_buf);
    self->next_scanline += 1;
    next_scanline_buf = self->framebuffer + (self->pitch * self->next_scanline);
    queue_add_blocking_u32(&self->dvi.q_colour_valid, &next_scanline_buf);
    self->next_scanline += 1;

    while (queue_get_level(&self->dvi.q_colour_valid) == 2) {
    }
    return true;
}

void picodvi_rp2040_deinit(picodvi_framebuffer_obj_t *self) {
    if (self->framebuffer == NULL) {
        return;
    }

    picodvi_hw_stop();
    m_free(self->framebuffer);
    self->framebuffer = NULL;
}

#endif /* PICO_RP2040 */
