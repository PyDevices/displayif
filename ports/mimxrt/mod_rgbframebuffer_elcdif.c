// SPDX-License-Identifier: MIT
// Dot-clock RGB framebuffer for mimxrt (NXP eLCDIF) — MIMXRT1060-EVK / RK043 shield.
//
// Constructor pin arguments are validated against the EVK LCDIF pad routing
// (BOARD_InitLCDPins / RK043 shield). IOMUX is applied from mimxrt1060_lcd_pins.c
// and is not configurable for arbitrary pins.
// Lifecycle: idempotent deinit/__del__/ctor + soft-reset teardown (see soft_reset.h).

#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "py/runtime.h"
#include "py/obj.h"
#include "py/binary.h"
#include "py/mphal.h"
#include "displayif/mp_helpers.h"
#include "displayif/soft_reset.h"
#include "displayif_mimxrt.h"
#include "pin.h"

#include "fsl_elcdif.h"
#include "fsl_clock.h"

#define RGBFB_LCDIF_FREQ_MIN_HZ 1000000u
#define RGBFB_LCDIF_FREQ_MAX_HZ 65000000u
#define RGBFB_LCDIF_DATA_PIN_COUNT 16u

#if defined(MIMXRT1062) || defined(CPU_MIMXRT1062) || defined(__IMXRT1062__)

typedef struct _rgbframebuffer_obj_t {
    mp_obj_base_t base;
    uint16_t width;
    uint16_t height;
    uint16_t row_stride;
    uint8_t *buf;
    size_t buf_len;
    uint32_t frequency;
    uint16_t hsync_pulse_width;
    uint16_t hsync_front_porch;
    uint16_t hsync_back_porch;
    uint16_t vsync_pulse_width;
    uint16_t vsync_front_porch;
    uint16_t vsync_back_porch;
    uint32_t polarity_flags;
    bool elcdif_ready;
    bool deinited;
} rgbframebuffer_obj_t;

static bool s_elcdif_live;
static bool s_soft_reset_registered;

static const mp_obj_type_t rgbframebuffer_type;

static void rgbfb_host_teardown(void) {
    if (s_elcdif_live) {
        ELCDIF_RgbModeStop(LCDIF);
        ELCDIF_Deinit(LCDIF);
        s_elcdif_live = false;
    }
}

static void rgbfb_ensure_soft_reset_registered(void) {
    if (!s_soft_reset_registered) {
        displayif_register_soft_reset(rgbfb_host_teardown);
        s_soft_reset_registered = true;
    }
}

static void rgbframebuffer_deinit_internal(rgbframebuffer_obj_t *self) {
    if (self->deinited) {
        return;
    }
    rgbfb_host_teardown();
    if (self->buf != NULL) {
        m_free(self->buf);
        self->buf = NULL;
    }
    self->elcdif_ready = false;
    self->deinited = true;
}

static uint32_t rgbframebuffer_build_polarity(bool hsync_idle_low, bool vsync_idle_low,
    bool de_idle_high, bool pclk_active_high) {
    uint32_t flags = 0;
    if (hsync_idle_low) {
        flags |= kELCDIF_HsyncActiveLow;
    } else {
        flags |= kELCDIF_HsyncActiveHigh;
    }
    if (vsync_idle_low) {
        flags |= kELCDIF_VsyncActiveLow;
    } else {
        flags |= kELCDIF_VsyncActiveHigh;
    }
    if (de_idle_high) {
        flags |= kELCDIF_DataEnableActiveHigh;
    } else {
        flags |= kELCDIF_DataEnableActiveLow;
    }
    if (pclk_active_high) {
        flags |= kELCDIF_DriveDataOnRisingClkEdge;
    } else {
        flags |= kELCDIF_DriveDataOnFallingClkEdge;
    }
    return flags;
}

static void rgbframebuffer_raise_status(status_t status) {
    if (status == kStatus_Success) {
        return;
    }
    mp_raise_msg_varg(&mp_type_OSError, MP_ERROR_TEXT("NXP SDK status %d"), (int)status);
}

static const clock_name_t rgbframebuffer_lcdif_mux_sources[] = {
    kCLOCK_SysPllClk,
    kCLOCK_Usb1PllPfd3Clk,
    kCLOCK_VideoPllClk,
    kCLOCK_SysPllPfd0Clk,
    kCLOCK_SysPllPfd1Clk,
    kCLOCK_Usb1PllPfd1Clk,
};

static void rgbframebuffer_validate_evk_pin(const machine_pin_obj_t *pin, GPIO_Type *exp_gpio, uint32_t exp_bit) {
    if (pin->gpio != exp_gpio || pin->pin != exp_bit) {
        mp_raise_ValueError(MP_ERROR_TEXT(
            "pin does not match MIMXRT1060-EVK LCDIF routing; IOMUX is EVK-fixed"));
    }
}

static void rgbframebuffer_validate_evk_pins(mp_obj_t de, mp_obj_t vsync, mp_obj_t hsync, mp_obj_t dclk, mp_obj_t data) {
    rgbframebuffer_validate_evk_pin(pin_find(de), GPIO2, 1);
    rgbframebuffer_validate_evk_pin(pin_find(vsync), GPIO2, 3);
    rgbframebuffer_validate_evk_pin(pin_find(hsync), GPIO2, 2);
    rgbframebuffer_validate_evk_pin(pin_find(dclk), GPIO2, 0);

    mp_obj_t data_pins[RGBFB_LCDIF_DATA_PIN_COUNT];
    size_t count = displayif_pin_seq_to_objs(data, data_pins, RGBFB_LCDIF_DATA_PIN_COUNT);
    if (count != RGBFB_LCDIF_DATA_PIN_COUNT) {
        mp_raise_ValueError(MP_ERROR_TEXT("data must be a 16-pin sequence for RGB565"));
    }
    for (size_t i = 0; i < RGBFB_LCDIF_DATA_PIN_COUNT; i++) {
        rgbframebuffer_validate_evk_pin(pin_find(data_pins[i]), GPIO2, 4 + i);
    }
}

static void rgbframebuffer_init_lcdif_clock(uint32_t target_hz) {
    CLOCK_DisableClock(kCLOCK_LcdPixel);

    uint32_t best_diff = UINT32_MAX;
    uint32_t best_mux = 5;
    uint32_t best_pre = 1;
    uint32_t best_div = 3;

    for (size_t mux = 0; mux < MP_ARRAY_SIZE(rgbframebuffer_lcdif_mux_sources); mux++) {
        uint32_t src_hz = CLOCK_GetFreq(rgbframebuffer_lcdif_mux_sources[mux]);
        if (src_hz == 0) {
            continue;
        }
        for (uint32_t pre = 0; pre < 8; pre++) {
            uint32_t mid_hz = src_hz / (pre + 1);
            for (uint32_t div = 0; div < 8; div++) {
                uint32_t out_hz = mid_hz / (div + 1);
                uint32_t diff = (out_hz > target_hz) ? (out_hz - target_hz) : (target_hz - out_hz);
                if (diff < best_diff) {
                    best_diff = diff;
                    best_mux = (uint32_t)mux;
                    best_pre = pre;
                    best_div = div;
                }
            }
        }
    }

    if (best_diff == UINT32_MAX) {
        mp_raise_msg(&mp_type_OSError, MP_ERROR_TEXT("unable to derive LCDIF pixel clock"));
    }

    CLOCK_SetMux(kCLOCK_LcdifPreMux, best_mux);
    CLOCK_SetDiv(kCLOCK_LcdifPreDiv, best_pre);
    CLOCK_SetDiv(kCLOCK_LcdifDiv, best_div);
    CLOCK_EnableClock(kCLOCK_LcdPixel);
}

static void rgbframebuffer_init_elcdif(rgbframebuffer_obj_t *self) {
    if (self->elcdif_ready) {
        return;
    }

    displayif_mimxrt1060_init_lcd_pins();
    rgbframebuffer_init_lcdif_clock(self->frequency);

    elcdif_rgb_mode_config_t config;
    ELCDIF_RgbModeGetDefaultConfig(&config);
    config.panelWidth = self->width;
    config.panelHeight = self->height;
    config.hsw = self->hsync_pulse_width;
    config.hfp = self->hsync_front_porch;
    config.hbp = self->hsync_back_porch;
    config.vsw = self->vsync_pulse_width;
    config.vfp = self->vsync_front_porch;
    config.vbp = self->vsync_back_porch;
    config.polarityFlags = self->polarity_flags;
    config.pixelFormat = kELCDIF_PixelFormatRGB565;
    config.dataBus = kELCDIF_DataBus16Bit;
    config.bufferAddr = (uint32_t)self->buf;

    ELCDIF_RgbModeInit(LCDIF, &config);
    ELCDIF_RgbModeStart(LCDIF);
    self->elcdif_ready = true;
    s_elcdif_live = true;
}

static mp_obj_t rgbframebuffer_make(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    enum {
        ARG_de,
        ARG_vsync,
        ARG_hsync,
        ARG_dclk,
        ARG_red,
        ARG_green,
        ARG_blue,
        ARG_data,
        ARG_frequency,
        ARG_width,
        ARG_height,
        ARG_hsync_pulse_width,
        ARG_hsync_front_porch,
        ARG_hsync_back_porch,
        ARG_vsync_pulse_width,
        ARG_vsync_front_porch,
        ARG_vsync_back_porch,
        ARG_hsync_idle_low,
        ARG_vsync_idle_low,
        ARG_de_idle_high,
        ARG_pclk_active_high,
        ARG_pclk_idle_high,
        ARG_overscan_left,
    };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_de, MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_OBJ, { .u_obj = MP_OBJ_NULL } },
        { MP_QSTR_vsync, MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_OBJ, { .u_obj = MP_OBJ_NULL } },
        { MP_QSTR_hsync, MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_OBJ, { .u_obj = MP_OBJ_NULL } },
        { MP_QSTR_dclk, MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_OBJ, { .u_obj = MP_OBJ_NULL } },
        { MP_QSTR_red, MP_ARG_KW_ONLY | MP_ARG_OBJ, { .u_obj = MP_OBJ_NULL } },
        { MP_QSTR_green, MP_ARG_KW_ONLY | MP_ARG_OBJ, { .u_obj = MP_OBJ_NULL } },
        { MP_QSTR_blue, MP_ARG_KW_ONLY | MP_ARG_OBJ, { .u_obj = MP_OBJ_NULL } },
        { MP_QSTR_data, MP_ARG_KW_ONLY | MP_ARG_OBJ, { .u_obj = MP_OBJ_NULL } },
        { MP_QSTR_frequency, MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_width, MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_height, MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_hsync_pulse_width, MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_hsync_front_porch, MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_hsync_back_porch, MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_vsync_pulse_width, MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_vsync_front_porch, MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_vsync_back_porch, MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_hsync_idle_low, MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_BOOL, { .u_bool = false } },
        { MP_QSTR_vsync_idle_low, MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_BOOL, { .u_bool = false } },
        { MP_QSTR_de_idle_high, MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_BOOL, { .u_bool = false } },
        { MP_QSTR_pclk_active_high, MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_BOOL, { .u_bool = false } },
        { MP_QSTR_pclk_idle_high, MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_BOOL, { .u_bool = false } },
        { MP_QSTR_overscan_left, MP_ARG_KW_ONLY | MP_ARG_INT, { .u_int = 0 } },
    };
    mp_arg_val_t vals[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, args, MP_ARRAY_SIZE(allowed_args), allowed_args, vals);

    bool rgb666 = vals[ARG_red].u_obj != MP_OBJ_NULL;
    bool rgb565 = vals[ARG_data].u_obj != MP_OBJ_NULL;
    if (rgb666 == rgb565) {
        mp_raise_ValueError(MP_ERROR_TEXT("Specify exactly one of red/green/blue or data pin layout"));
    }
    if (rgb666) {
        mp_raise_ValueError(MP_ERROR_TEXT("RGB-666 not supported on mimxrt eLCDIF yet"));
    }
    if (vals[ARG_width].u_int <= 0 || vals[ARG_height].u_int <= 0) {
        mp_raise_ValueError(MP_ERROR_TEXT("width and height must be positive"));
    }
    if (vals[ARG_frequency].u_int < RGBFB_LCDIF_FREQ_MIN_HZ
        || vals[ARG_frequency].u_int > RGBFB_LCDIF_FREQ_MAX_HZ) {
        mp_raise_ValueError(MP_ERROR_TEXT("frequency out of range (1-65 MHz)"));
    }
    rgbframebuffer_validate_evk_pins(vals[ARG_de].u_obj, vals[ARG_vsync].u_obj,
        vals[ARG_hsync].u_obj, vals[ARG_dclk].u_obj, vals[ARG_data].u_obj);
    (void)vals[ARG_overscan_left];
    (void)vals[ARG_pclk_idle_high];

    rgbfb_host_teardown();
    rgbfb_ensure_soft_reset_registered();

    rgbframebuffer_obj_t *self = mp_obj_malloc(rgbframebuffer_obj_t, type);
    self->width = vals[ARG_width].u_int;
    self->height = vals[ARG_height].u_int;
    self->row_stride = self->width * sizeof(uint16_t);
    self->buf_len = (size_t)self->row_stride * self->height;
    self->buf = m_malloc(self->buf_len);
    if (self->buf == NULL) {
        mp_raise_msg(&mp_type_MemoryError, MP_ERROR_TEXT("RGB framebuffer allocation failed"));
    }
    memset(self->buf, 0, self->buf_len);

    self->frequency = vals[ARG_frequency].u_int;
    self->hsync_pulse_width = vals[ARG_hsync_pulse_width].u_int;
    self->hsync_front_porch = vals[ARG_hsync_front_porch].u_int;
    self->hsync_back_porch = vals[ARG_hsync_back_porch].u_int;
    self->vsync_pulse_width = vals[ARG_vsync_pulse_width].u_int;
    self->vsync_front_porch = vals[ARG_vsync_front_porch].u_int;
    self->vsync_back_porch = vals[ARG_vsync_back_porch].u_int;
    self->polarity_flags = rgbframebuffer_build_polarity(
        vals[ARG_hsync_idle_low].u_bool,
        vals[ARG_vsync_idle_low].u_bool,
        vals[ARG_de_idle_high].u_bool,
        vals[ARG_pclk_active_high].u_bool);
    self->elcdif_ready = false;
    self->deinited = false;

    rgbframebuffer_init_elcdif(self);
    return MP_OBJ_FROM_PTR(self);
}

static mp_obj_t rgbframebuffer_refresh(mp_obj_t self_in) {
    rgbframebuffer_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (self->deinited || self->buf == NULL) {
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("RGB framebuffer is deinited"));
    }
    rgbframebuffer_init_elcdif(self);
    ELCDIF_SetNextBufferAddr(LCDIF, ELCDIF_ADDR_CPU_2_IP((uint32_t)self->buf));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(rgbframebuffer_refresh_obj, rgbframebuffer_refresh);

static void rgbframebuffer_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    rgbframebuffer_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (dest[0] == MP_OBJ_NULL) {
        if (attr == MP_QSTR_width) {
            dest[0] = mp_obj_new_int(self->width);
        } else if (attr == MP_QSTR_height) {
            dest[0] = mp_obj_new_int(self->height);
        } else if (attr == MP_QSTR_row_stride) {
            dest[0] = mp_obj_new_int(self->row_stride);
        }
    }
}

static mp_int_t rgbframebuffer_get_buffer(mp_obj_t self_in, mp_buffer_info_t *bufinfo, mp_uint_t flags) {
    (void)flags;
    rgbframebuffer_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (self->deinited || self->buf == NULL) {
        bufinfo->buf = NULL;
        bufinfo->len = 0;
        bufinfo->typecode = 'H';
        return 0;
    }
    bufinfo->buf = self->buf;
    bufinfo->len = self->buf_len;
    bufinfo->typecode = 'H';
    return 0;
}

static mp_obj_t rgbframebuffer_del(mp_obj_t self_in) {
    rgbframebuffer_obj_t *self = MP_OBJ_TO_PTR(self_in);
    rgbframebuffer_deinit_internal(self);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(rgbframebuffer_del_obj, rgbframebuffer_del);

static const mp_rom_map_elem_t rgbframebuffer_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_refresh), MP_ROM_PTR(&rgbframebuffer_refresh_obj) },
    { MP_ROM_QSTR(MP_QSTR_deinit), MP_ROM_PTR(&rgbframebuffer_del_obj) },
    { MP_ROM_QSTR(MP_QSTR___del__), MP_ROM_PTR(&rgbframebuffer_del_obj) },
};
static MP_DEFINE_CONST_DICT(rgbframebuffer_locals_dict, rgbframebuffer_locals_dict_table);

static MP_DEFINE_CONST_OBJ_TYPE(
    rgbframebuffer_type,
    MP_QSTR_RGBFrameBuffer,
    MP_TYPE_FLAG_NONE,
    make_new, rgbframebuffer_make,
    attr, rgbframebuffer_attr,
    locals_dict, &rgbframebuffer_locals_dict,
    buffer, rgbframebuffer_get_buffer
);

static const mp_rom_map_elem_t rgbframebuffer_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_rgbframebuffer) },
    { MP_ROM_QSTR(MP_QSTR_RGBFrameBuffer), MP_ROM_PTR(&rgbframebuffer_type) },
};
static MP_DEFINE_CONST_DICT(rgbframebuffer_module_globals, rgbframebuffer_module_globals_table);

const mp_obj_module_t rgbframebuffer_user_cmodule = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&rgbframebuffer_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_rgbframebuffer, rgbframebuffer_user_cmodule);

#endif /* MIMXRT1062 */
