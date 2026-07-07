// SPDX-License-Identifier: MIT
// MIPI DSI host for MIMXRT1176 (LCDIFV2 + NXP mipi_dsi_split).

#include <string.h>
#include <stdlib.h>

#include "py/runtime.h"
#include "py/obj.h"
#include "py/binary.h"
#include "py/mphal.h"
#include "displayif/mp_helpers.h"

#if defined(MIMXRT1176_SERIES) || defined(CPU_MIMXRT1176) || defined(CPU_MIMXRT1176DVMAA_cm7)

#include "mimxrt1176_dsi_display.h"
#include "fsl_cache.h"

typedef struct _mipidsi_bus_obj_t {
    mp_obj_base_t base;
    uint8_t num_lanes;
    uint32_t lane_bit_rate_hz;
    bool ready;
} mipidsi_bus_obj_t;

typedef struct _mipidsi_display_obj_t {
    mp_obj_base_t base;
    mipidsi_bus_obj_t *bus;
    uint16_t width;
    uint16_t height;
    uint16_t row_stride;
    uint8_t *buf;
    size_t buf_len;
    int backlight_pin;
    bool backlight_on_high;
    bool lcdif_ready;
} mipidsi_display_obj_t;

static const mp_obj_type_t mipidsi_bus_type;
static const mp_obj_type_t mipidsi_display_type;

static void mipidsi_raise_status(status_t status) {
    if (status == kStatus_Success) {
        return;
    }
    mp_raise_msg_varg(&mp_type_OSError, MP_ERROR_TEXT("NXP SDK status %d"), (int)status);
}

static mp_obj_t mipidsi_bus_make(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    enum {
        ARG_frequency,
        ARG_num_lanes,
    };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_frequency, MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_num_lanes, MP_ARG_KW_ONLY | MP_ARG_INT, { .u_int = 2 } },
    };
    mp_arg_val_t vals[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, args, MP_ARRAY_SIZE(allowed_args), allowed_args, vals);

    if (vals[ARG_frequency].u_int <= 0) {
        mp_raise_ValueError(MP_ERROR_TEXT("frequency must be positive"));
    }
    if (vals[ARG_num_lanes].u_int <= 0 || vals[ARG_num_lanes].u_int > 4) {
        mp_raise_ValueError(MP_ERROR_TEXT("num_lanes out of range"));
    }

    mipidsi_bus_obj_t *self = mp_obj_malloc(mipidsi_bus_obj_t, type);
    self->num_lanes = vals[ARG_num_lanes].u_int;
    self->lane_bit_rate_hz = (uint32_t)vals[ARG_frequency].u_int;
    self->ready = false;

    mipidsi_raise_status(displayif_mimxrt1176_dsi_bus_init(self->num_lanes, self->lane_bit_rate_hz));
    self->ready = true;
    return MP_OBJ_FROM_PTR(self);
}

static mp_obj_t mipidsi_display_make(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    if (n_args < 1) {
        mp_raise_TypeError(MP_ERROR_TEXT("Display requires a Bus instance"));
    }
    mp_obj_t bus_obj = args[0];

    enum {
        ARG_init_sequence,
        ARG_width,
        ARG_height,
        ARG_color_depth,
        ARG_pixel_clock_frequency,
        ARG_hsync_pulse_width,
        ARG_hsync_front_porch,
        ARG_hsync_back_porch,
        ARG_vsync_pulse_width,
        ARG_vsync_front_porch,
        ARG_vsync_back_porch,
        ARG_reset_pin,
        ARG_backlight_pin,
        ARG_backlight_on_high,
    };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_init_sequence, MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_OBJ, { .u_obj = MP_OBJ_NULL } },
        { MP_QSTR_width, MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_height, MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_color_depth, MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_INT, { .u_int = 16 } },
        { MP_QSTR_pixel_clock_frequency, MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_hsync_pulse_width, MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_hsync_front_porch, MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_hsync_back_porch, MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_vsync_pulse_width, MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_vsync_front_porch, MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_vsync_back_porch, MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_reset_pin, MP_ARG_KW_ONLY | MP_ARG_INT, { .u_int = -1 } },
        { MP_QSTR_backlight_pin, MP_ARG_KW_ONLY | MP_ARG_INT, { .u_int = -1 } },
        { MP_QSTR_backlight_on_high, MP_ARG_KW_ONLY | MP_ARG_BOOL, { .u_bool = true } },
    };
    mp_arg_val_t vals[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args - 1, n_kw, args + 1, MP_ARRAY_SIZE(allowed_args), allowed_args, vals);

    mipidsi_bus_obj_t *bus = MP_OBJ_TO_PTR(bus_obj);
    if (!mp_obj_is_type(bus_obj, &mipidsi_bus_type)) {
        mp_raise_TypeError(MP_ERROR_TEXT("bus must be mipidsi.Bus"));
    }
    if (!bus->ready) {
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("MIPI DSI bus is not initialized"));
    }
    if (vals[ARG_color_depth].u_int != 16) {
        mp_raise_ValueError(MP_ERROR_TEXT("only color_depth=16 is supported"));
    }
    if (vals[ARG_width].u_int <= 0 || vals[ARG_height].u_int <= 0) {
        mp_raise_ValueError(MP_ERROR_TEXT("width and height must be positive"));
    }
    if (vals[ARG_pixel_clock_frequency].u_int <= 0) {
        mp_raise_ValueError(MP_ERROR_TEXT("pixel_clock_frequency must be positive"));
    }

    mp_buffer_info_t init_bufinfo;
    mp_get_buffer_raise(vals[ARG_init_sequence].u_obj, &init_bufinfo, MP_BUFFER_READ);

    mipidsi_display_obj_t *self = mp_obj_malloc(mipidsi_display_obj_t, type);
    self->bus = bus;
    self->width = vals[ARG_width].u_int;
    self->height = vals[ARG_height].u_int;
    self->row_stride = self->width * sizeof(uint16_t);
    self->buf_len = (size_t)self->row_stride * self->height;
    self->buf = m_malloc(self->buf_len);
    if (self->buf == NULL) {
        mp_raise_msg(&mp_type_MemoryError, MP_ERROR_TEXT("MIPI DSI framebuffer allocation failed"));
    }
    memset(self->buf, 0, self->buf_len);
    self->backlight_pin = vals[ARG_backlight_pin].u_int;
    self->backlight_on_high = vals[ARG_backlight_on_high].u_bool;
    self->lcdif_ready = false;

    displayif_mimxrt1176_dsi_timings_t timings = {
        .width = self->width,
        .height = self->height,
        .pixel_clock_hz = (uint32_t)vals[ARG_pixel_clock_frequency].u_int,
        .hsw = vals[ARG_hsync_pulse_width].u_int,
        .hfp = vals[ARG_hsync_front_porch].u_int,
        .hbp = vals[ARG_hsync_back_porch].u_int,
        .vsw = vals[ARG_vsync_pulse_width].u_int,
        .vfp = vals[ARG_vsync_front_porch].u_int,
        .vbp = vals[ARG_vsync_back_porch].u_int,
        .num_lanes = bus->num_lanes,
        .lane_bit_rate_hz = bus->lane_bit_rate_hz,
    };

    mipidsi_raise_status(displayif_mimxrt1176_dsi_display_start(&timings,
        init_bufinfo.buf, init_bufinfo.len,
        vals[ARG_reset_pin].u_int, self->backlight_pin, self->backlight_on_high));

    displayif_mimxrt1176_dsi_lcdifv2_start(self->buf);
    self->lcdif_ready = true;

    return MP_OBJ_FROM_PTR(self);
}

static mp_obj_t mipidsi_display_refresh(mp_obj_t self_in) {
    mipidsi_display_obj_t *self = MP_OBJ_TO_PTR(self_in);
    DCACHE_CleanByRange((uint32_t)self->buf, self->buf_len);
    displayif_mimxrt1176_dsi_set_framebuffer(self->buf);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mipidsi_display_refresh_obj, mipidsi_display_refresh);

static void mipidsi_display_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    mipidsi_display_obj_t *self = MP_OBJ_TO_PTR(self_in);
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

static mp_int_t mipidsi_display_get_buffer(mp_obj_t self_in, mp_buffer_info_t *bufinfo, mp_uint_t flags) {
    (void)flags;
    mipidsi_display_obj_t *self = MP_OBJ_TO_PTR(self_in);
    bufinfo->buf = self->buf;
    bufinfo->len = self->buf_len;
    bufinfo->typecode = 'H';
    return 0;
}

static mp_obj_t mipidsi_display_del(mp_obj_t self_in) {
    mipidsi_display_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (self->lcdif_ready) {
        displayif_mimxrt1176_dsi_display_stop();
        self->lcdif_ready = false;
    }
    if (self->buf != NULL) {
        m_free(self->buf);
        self->buf = NULL;
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mipidsi_display_del_obj, mipidsi_display_del);

static const mp_rom_map_elem_t mipidsi_display_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_refresh), MP_ROM_PTR(&mipidsi_display_refresh_obj) },
    { MP_ROM_QSTR(MP_QSTR___del__), MP_ROM_PTR(&mipidsi_display_del_obj) },
};
static MP_DEFINE_CONST_DICT(mipidsi_display_locals_dict, mipidsi_display_locals_dict_table);

static MP_DEFINE_CONST_OBJ_TYPE(
    mipidsi_display_type,
    MP_QSTR_Display,
    MP_TYPE_FLAG_NONE,
    make_new, mipidsi_display_make,
    attr, mipidsi_display_attr,
    locals_dict, &mipidsi_display_locals_dict,
    buffer, mipidsi_display_get_buffer
);

static MP_DEFINE_CONST_OBJ_TYPE(
    mipidsi_bus_type,
    MP_QSTR_Bus,
    MP_TYPE_FLAG_NONE,
    make_new, mipidsi_bus_make
);

static const mp_rom_map_elem_t mipidsi_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_mipidsi) },
    { MP_ROM_QSTR(MP_QSTR_Bus), MP_ROM_PTR(&mipidsi_bus_type) },
    { MP_ROM_QSTR(MP_QSTR_Display), MP_ROM_PTR(&mipidsi_display_type) },
};
static MP_DEFINE_CONST_DICT(mipidsi_module_globals, mipidsi_module_globals_table);

const mp_obj_module_t mipidsi_user_cmodule = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&mipidsi_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_mipidsi, mipidsi_user_cmodule);

#endif /* MIMXRT1176 */
