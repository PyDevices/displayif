// SPDX-License-Identifier: MIT
// Dot-clock RGB framebuffer for mimxrt (NXP eLCDIF) — MIMXRT1060-EVK / RK043 shield.

#include <string.h>
#include <stdlib.h>

#include "py/runtime.h"
#include "py/obj.h"
#include "py/binary.h"
#include "py/mphal.h"
#include "displayif/mp_helpers.h"
#include "displayif_mimxrt.h"

#include "fsl_elcdif.h"
#include "fsl_clock.h"

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
} rgbframebuffer_obj_t;

static const mp_obj_type_t rgbframebuffer_type;

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

static void rgbframebuffer_init_elcdif(rgbframebuffer_obj_t *self) {
    if (self->elcdif_ready) {
        return;
    }

    displayif_mimxrt1060_init_lcd_pins();

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
    (void)self->frequency;
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
    if (vals[ARG_frequency].u_int <= 0) {
        mp_raise_ValueError(MP_ERROR_TEXT("frequency must be positive"));
    }
    (void)vals[ARG_overscan_left];
    (void)vals[ARG_pclk_idle_high];
    (void)vals[ARG_de];
    (void)vals[ARG_vsync];
    (void)vals[ARG_hsync];
    (void)vals[ARG_dclk];

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

    rgbframebuffer_init_elcdif(self);
    return MP_OBJ_FROM_PTR(self);
}

static mp_obj_t rgbframebuffer_refresh(mp_obj_t self_in) {
    rgbframebuffer_obj_t *self = MP_OBJ_TO_PTR(self_in);
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
    bufinfo->buf = self->buf;
    bufinfo->len = self->buf_len;
    bufinfo->typecode = 'H';
    return 0;
}

static mp_obj_t rgbframebuffer_del(mp_obj_t self_in) {
    rgbframebuffer_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (self->elcdif_ready) {
        ELCDIF_RgbModeStop(LCDIF);
        ELCDIF_Deinit(LCDIF);
        self->elcdif_ready = false;
    }
    if (self->buf != NULL) {
        m_free(self->buf);
        self->buf = NULL;
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(rgbframebuffer_del_obj, rgbframebuffer_del);

static const mp_rom_map_elem_t rgbframebuffer_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_refresh), MP_ROM_PTR(&rgbframebuffer_refresh_obj) },
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
