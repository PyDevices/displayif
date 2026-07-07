// SPDX-License-Identifier: MIT
// HUB75 RGB LED matrix — Protomatter (hardware timer) or portable GPIO bitbang fallback.

#include <string.h>
#include <stdlib.h>

#include "py/runtime.h"
#include "py/obj.h"
#include "py/binary.h"
#include "py/mphal.h"
#include "displayif/mp_helpers.h"
#include "displayif/rgbmatrix_pm.h"

#if defined(DISPLAYIF_RGBMATRIX_USE_PROTOMATTER)
#include "protomatter/core.h"
#endif

#define RGBMATRIX_MAX_ADDR_PINS 6
#define RGBMATRIX_RGB_PINS 6
#define RGBMATRIX_MAX_PM_PINS 64

typedef struct _rgbmatrix_obj_t {
    mp_obj_base_t base;
    uint16_t width;
    uint16_t height;
    uint16_t row_stride;
    uint8_t bit_depth;
    mp_obj_t rgb_pins[RGBMATRIX_RGB_PINS];
    mp_obj_t addr_pins[RGBMATRIX_MAX_ADDR_PINS];
    size_t addr_pin_count;
    mp_obj_t clock_pin;
    mp_obj_t latch_pin;
    mp_obj_t oe_pin;
    bool doublebuffer;
    int tile;
    bool serpentine;
    bool external_fb;
    uint8_t *draw_buf;
    uint8_t *scan_buf;
    size_t buf_len;
#if defined(DISPLAYIF_RGBMATRIX_USE_PROTOMATTER)
    Protomatter_core pm;
    void *pm_timer;
    bool pm_started;
    uint8_t pm_rgb_pins[RGBMATRIX_RGB_PINS];
    uint8_t pm_addr_pins[RGBMATRIX_MAX_ADDR_PINS];
    uint8_t pm_clock_pin;
    uint8_t pm_latch_pin;
    uint8_t pm_oe_pin;
#endif
} rgbmatrix_obj_t;

static const mp_obj_type_t rgbmatrix_type;

static uint8_t *rgbmatrix_alloc(size_t nbytes) {
    return m_malloc(nbytes);
}

static void rgbmatrix_free(uint8_t *ptr) {
    if (ptr != NULL) {
        m_free(ptr);
    }
}

static mp_obj_t rgbmatrix_make_pin(mp_obj_t pin_or_int) {
    return displayif_pin_resolve(pin_or_int);
}

static void rgbmatrix_set_addr(rgbmatrix_obj_t *self, uint8_t row) {
    for (size_t i = 0; i < self->addr_pin_count; i++) {
        displayif_pin_set(self->addr_pins[i], (row >> i) & 1);
    }
}

static void rgbmatrix_pulse_clock(rgbmatrix_obj_t *self) {
    displayif_pin_set(self->clock_pin, 1);
    displayif_pin_set(self->clock_pin, 0);
}

static void rgbmatrix_pulse_latch(rgbmatrix_obj_t *self) {
    displayif_pin_set(self->latch_pin, 1);
    displayif_pin_set(self->latch_pin, 0);
}

static uint8_t rgbmatrix_expand_channel(uint16_t pixel, uint8_t channel, uint8_t bit_depth) {
    uint8_t val;
    uint8_t src_bits;
    if (channel == 0) {
        val = (pixel >> 11) & 0x1F;
        src_bits = 5;
    } else if (channel == 1) {
        val = (pixel >> 5) & 0x3F;
        src_bits = 6;
    } else {
        val = pixel & 0x1F;
        src_bits = 5;
    }
    if (bit_depth >= src_bits) {
        return val << (bit_depth - src_bits);
    }
    return val >> (src_bits - bit_depth);
}

static uint16_t rgbmatrix_get_pixel(const rgbmatrix_obj_t *self, int x, int y) {
    if (self->serpentine && (y & 1)) {
        x = self->width - 1 - x;
    }
    const uint16_t *pixels = (const uint16_t *)self->scan_buf;
    return pixels[(size_t)y * self->width + x];
}

static void rgbmatrix_shift_row(rgbmatrix_obj_t *self, uint8_t addr, uint8_t plane) {
    const uint16_t half = self->height / 2;

    for (uint16_t x = 0; x < self->width; x++) {
        uint16_t top = rgbmatrix_get_pixel(self, x, addr);
        uint16_t bottom = rgbmatrix_get_pixel(self, x, addr + half);

        uint8_t r1 = (rgbmatrix_expand_channel(top, 0, self->bit_depth) >> plane) & 1;
        uint8_t g1 = (rgbmatrix_expand_channel(top, 1, self->bit_depth) >> plane) & 1;
        uint8_t b1 = (rgbmatrix_expand_channel(top, 2, self->bit_depth) >> plane) & 1;
        uint8_t r2 = (rgbmatrix_expand_channel(bottom, 0, self->bit_depth) >> plane) & 1;
        uint8_t g2 = (rgbmatrix_expand_channel(bottom, 1, self->bit_depth) >> plane) & 1;
        uint8_t b2 = (rgbmatrix_expand_channel(bottom, 2, self->bit_depth) >> plane) & 1;

        displayif_pin_set(self->rgb_pins[0], r1);
        displayif_pin_set(self->rgb_pins[1], g1);
        displayif_pin_set(self->rgb_pins[2], b1);
        displayif_pin_set(self->rgb_pins[3], r2);
        displayif_pin_set(self->rgb_pins[4], g2);
        displayif_pin_set(self->rgb_pins[5], b2);
        rgbmatrix_pulse_clock(self);
    }
}

#if defined(DISPLAYIF_RGBMATRIX_USE_PROTOMATTER)

static void rgbmatrix_pm_status_raise(ProtomatterStatus stat) {
    switch (stat) {
        case PROTOMATTER_ERR_PINS:
            mp_raise_ValueError(MP_ERROR_TEXT("rgbmatrix pin/port layout invalid"));
        case PROTOMATTER_ERR_MALLOC:
            mp_raise_msg(&mp_type_MemoryError, MP_ERROR_TEXT("rgbmatrix allocation failed"));
        case PROTOMATTER_ERR_ARG:
            mp_raise_ValueError(MP_ERROR_TEXT("invalid rgbmatrix arguments"));
        default:
            mp_raise_msg_varg(&mp_type_RuntimeError, MP_ERROR_TEXT("rgbmatrix error %d"), (int)stat);
    }
}

static uint8_t rgbmatrix_pm_assign_pin(rgbmatrix_obj_t *self, uint8_t *next_pm_pin, mp_obj_t pin_obj) {
    (void)self;
    uint8_t pm_slot = *next_pm_pin;
    if (pm_slot >= RGBMATRIX_MAX_PM_PINS) {
        mp_raise_ValueError(MP_ERROR_TEXT("too many rgbmatrix pins"));
    }
    if (pin_obj == MP_OBJ_NULL) {
        mp_raise_ValueError(MP_ERROR_TEXT("invalid rgbmatrix pin"));
    }
    uint8_t pm_pin = displayif_rgbmatrix_pm_pin_index(pm_slot, pin_obj);
    *next_pm_pin = pm_slot + 1;
    return pm_pin;
}

static void rgbmatrix_pm_start(rgbmatrix_obj_t *self) {
    if (!displayif_rgbmatrix_pm_available()) {
        return;
    }

    self->pm_timer = displayif_rgbmatrix_pm_timer_alloc();
    if (self->pm_timer == NULL) {
        mp_raise_ValueError(MP_ERROR_TEXT("no timer available for rgbmatrix"));
    }

    memset(&self->pm, 0, sizeof(self->pm));
    int8_t pm_tile = self->serpentine ? -(int8_t)self->tile : (int8_t)self->tile;
    ProtomatterStatus stat = _PM_init(
        &self->pm,
        self->width,
        self->bit_depth,
        1,
        self->pm_rgb_pins,
        self->addr_pin_count,
        self->pm_addr_pins,
        self->pm_clock_pin,
        self->pm_latch_pin,
        self->pm_oe_pin,
        self->doublebuffer,
        pm_tile,
        self->pm_timer);
    if (stat != PROTOMATTER_OK) {
        displayif_rgbmatrix_pm_timer_free(self->pm_timer);
        self->pm_timer = NULL;
        rgbmatrix_pm_status_raise(stat);
    }

    displayif_rgbmatrix_pm_set_active(&self->pm);
    displayif_rgbmatrix_pm_timer_enable(self->pm_timer);
    stat = _PM_begin(&self->pm);
    if (stat != PROTOMATTER_OK) {
        _PM_deallocate(&self->pm);
        displayif_rgbmatrix_pm_timer_free(self->pm_timer);
        self->pm_timer = NULL;
        displayif_rgbmatrix_pm_set_active(NULL);
        rgbmatrix_pm_status_raise(stat);
    }

    _PM_convert_565(&self->pm, (uint16_t *)self->draw_buf, self->width);
    _PM_swapbuffer_maybe(&self->pm);
    self->pm_started = true;
}

static void rgbmatrix_pm_stop(rgbmatrix_obj_t *self) {
    if (!self->pm_started) {
        return;
    }
    displayif_rgbmatrix_pm_timer_disable(self->pm_timer);
    _PM_stop(&self->pm);
    _PM_deallocate(&self->pm);
    displayif_rgbmatrix_pm_timer_free(self->pm_timer);
    self->pm_timer = NULL;
    displayif_rgbmatrix_pm_set_active(NULL);
    self->pm_started = false;
    memset(&self->pm, 0, sizeof(self->pm));
}

#endif /* DISPLAYIF_RGBMATRIX_USE_PROTOMATTER */

static mp_obj_t rgbmatrix_make(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    enum {
        ARG_width,
        ARG_bit_depth,
        ARG_rgb_pins,
        ARG_addr_pins,
        ARG_clock_pin,
        ARG_latch_pin,
        ARG_output_enable_pin,
        ARG_doublebuffer,
        ARG_framebuffer,
        ARG_height,
        ARG_tile,
        ARG_serpentine,
    };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_width, MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_bit_depth, MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_rgb_pins, MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_OBJ, { .u_obj = MP_OBJ_NULL } },
        { MP_QSTR_addr_pins, MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_OBJ, { .u_obj = MP_OBJ_NULL } },
        { MP_QSTR_clock_pin, MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_OBJ, { .u_obj = MP_OBJ_NULL } },
        { MP_QSTR_latch_pin, MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_OBJ, { .u_obj = MP_OBJ_NULL } },
        { MP_QSTR_output_enable_pin, MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_OBJ, { .u_obj = MP_OBJ_NULL } },
        { MP_QSTR_doublebuffer, MP_ARG_KW_ONLY | MP_ARG_BOOL, { .u_bool = true } },
        { MP_QSTR_framebuffer, MP_ARG_KW_ONLY | MP_ARG_OBJ, { .u_obj = MP_OBJ_NULL } },
        { MP_QSTR_height, MP_ARG_KW_ONLY | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_tile, MP_ARG_KW_ONLY | MP_ARG_INT, { .u_int = 1 } },
        { MP_QSTR_serpentine, MP_ARG_KW_ONLY | MP_ARG_BOOL, { .u_bool = true } },
    };
    mp_arg_val_t vals[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, args, MP_ARRAY_SIZE(allowed_args), allowed_args, vals);

    if (vals[ARG_width].u_int <= 0) {
        mp_raise_ValueError(MP_ERROR_TEXT("width must be positive"));
    }
    if (vals[ARG_bit_depth].u_int < 1 || vals[ARG_bit_depth].u_int > 6) {
        mp_raise_ValueError(MP_ERROR_TEXT("bit_depth must be 1..6"));
    }
    if (vals[ARG_clock_pin].u_obj == MP_OBJ_NULL || vals[ARG_latch_pin].u_obj == MP_OBJ_NULL
        || vals[ARG_output_enable_pin].u_obj == MP_OBJ_NULL) {
        mp_raise_ValueError(MP_ERROR_TEXT("clock, latch, and output_enable pins required"));
    }
    if (vals[ARG_tile].u_int < 1) {
        mp_raise_ValueError(MP_ERROR_TEXT("tile must be >= 1"));
    }

    mp_obj_t rgb_pin_objs[RGBMATRIX_RGB_PINS];
    size_t rgb_count = displayif_pin_seq_to_objs(vals[ARG_rgb_pins].u_obj, rgb_pin_objs, RGBMATRIX_RGB_PINS);
    if (rgb_count != RGBMATRIX_RGB_PINS) {
        mp_raise_ValueError(MP_ERROR_TEXT("rgb_pins must contain 6 pins"));
    }

    mp_obj_t addr_pin_objs[RGBMATRIX_MAX_ADDR_PINS];
    size_t addr_count = displayif_pin_seq_to_objs(vals[ARG_addr_pins].u_obj, addr_pin_objs, RGBMATRIX_MAX_ADDR_PINS);
    if (addr_count == 0 || addr_count > RGBMATRIX_MAX_ADDR_PINS) {
        mp_raise_ValueError(MP_ERROR_TEXT("addr_pins must contain 1..6 pins"));
    }

    int tile = vals[ARG_tile].u_int;
    int computed_height = (int)(rgb_count / 3) * (1 << addr_count) * tile;
    uint16_t height = vals[ARG_height].u_int;
    if (height == 0) {
        height = computed_height;
    } else if (height != computed_height) {
        mp_raise_ValueError(MP_ERROR_TEXT("height does not match addr_pins/rgb_pins/tile"));
    }
    if (height <= 0 || (height % 2) != 0) {
        mp_raise_ValueError(MP_ERROR_TEXT("height must be a positive even number"));
    }

#if !defined(DISPLAYIF_RGBMATRIX_USE_PROTOMATTER)
    if (tile != 1) {
        mp_raise_NotImplementedError(MP_ERROR_TEXT("tile requires Protomatter backend"));
    }
#endif

    rgbmatrix_obj_t *self = mp_obj_malloc(rgbmatrix_obj_t, type);
    self->width = vals[ARG_width].u_int;
    self->height = height;
    self->row_stride = self->width * sizeof(uint16_t);
    self->buf_len = (size_t)self->row_stride * self->height;
    self->bit_depth = vals[ARG_bit_depth].u_int;
    self->addr_pin_count = addr_count;
    self->doublebuffer = vals[ARG_doublebuffer].u_bool;
    self->tile = tile;
    self->serpentine = vals[ARG_serpentine].u_bool;
    self->external_fb = false;
    self->draw_buf = NULL;
    self->scan_buf = NULL;
#if defined(DISPLAYIF_RGBMATRIX_USE_PROTOMATTER)
    self->pm_timer = NULL;
    self->pm_started = false;
    memset(&self->pm, 0, sizeof(self->pm));
#endif

    for (size_t i = 0; i < RGBMATRIX_RGB_PINS; i++) {
        self->rgb_pins[i] = rgb_pin_objs[i];
    }
    for (size_t i = 0; i < addr_count; i++) {
        self->addr_pins[i] = addr_pin_objs[i];
    }
    self->clock_pin = rgbmatrix_make_pin(vals[ARG_clock_pin].u_obj);
    self->latch_pin = rgbmatrix_make_pin(vals[ARG_latch_pin].u_obj);
    self->oe_pin = rgbmatrix_make_pin(vals[ARG_output_enable_pin].u_obj);

#if defined(DISPLAYIF_RGBMATRIX_USE_PROTOMATTER)
    if (displayif_rgbmatrix_pm_available()) {
        uint8_t next_pm_pin = 0;
        for (size_t i = 0; i < RGBMATRIX_RGB_PINS; i++) {
            self->pm_rgb_pins[i] = rgbmatrix_pm_assign_pin(self, &next_pm_pin, self->rgb_pins[i]);
        }
        for (size_t i = 0; i < addr_count; i++) {
            self->pm_addr_pins[i] = rgbmatrix_pm_assign_pin(self, &next_pm_pin, self->addr_pins[i]);
        }
        self->pm_clock_pin = rgbmatrix_pm_assign_pin(self, &next_pm_pin, self->clock_pin);
        self->pm_latch_pin = rgbmatrix_pm_assign_pin(self, &next_pm_pin, self->latch_pin);
        self->pm_oe_pin = rgbmatrix_pm_assign_pin(self, &next_pm_pin, self->oe_pin);
    }
#endif

    displayif_pin_set(self->clock_pin, 0);
    displayif_pin_set(self->latch_pin, 0);
    displayif_pin_set(self->oe_pin, 1);

    if (vals[ARG_framebuffer].u_obj != MP_OBJ_NULL) {
        mp_buffer_info_t bufinfo;
        mp_get_buffer_raise(vals[ARG_framebuffer].u_obj, &bufinfo, MP_BUFFER_READ);
        if (bufinfo.len < self->buf_len) {
            mp_raise_ValueError(MP_ERROR_TEXT("framebuffer too small"));
        }
        self->draw_buf = bufinfo.buf;
        self->scan_buf = bufinfo.buf;
        self->external_fb = true;
        if (self->doublebuffer) {
            mp_raise_ValueError(MP_ERROR_TEXT("doublebuffer requires internal framebuffer"));
        }
    } else {
        self->draw_buf = rgbmatrix_alloc(self->buf_len);
        if (self->draw_buf == NULL) {
            mp_raise_msg(&mp_type_MemoryError, MP_ERROR_TEXT("rgbmatrix framebuffer allocation failed"));
        }
        memset(self->draw_buf, 0, self->buf_len);
        if (self->doublebuffer) {
            self->scan_buf = rgbmatrix_alloc(self->buf_len);
            if (self->scan_buf == NULL) {
                rgbmatrix_free(self->draw_buf);
                mp_raise_msg(&mp_type_MemoryError, MP_ERROR_TEXT("rgbmatrix back buffer allocation failed"));
            }
            memset(self->scan_buf, 0, self->buf_len);
        } else {
            self->scan_buf = self->draw_buf;
        }
    }

#if defined(DISPLAYIF_RGBMATRIX_USE_PROTOMATTER)
    if (displayif_rgbmatrix_pm_available()) {
        rgbmatrix_pm_start(self);
    }
#endif

    return MP_OBJ_FROM_PTR(self);
}

static mp_obj_t rgbmatrix_refresh(mp_obj_t self_in) {
    rgbmatrix_obj_t *self = MP_OBJ_TO_PTR(self_in);

#if defined(DISPLAYIF_RGBMATRIX_USE_PROTOMATTER)
    if (self->pm_started) {
        _PM_convert_565(&self->pm, (uint16_t *)self->draw_buf, self->width);
        _PM_swapbuffer_maybe(&self->pm);
        return mp_const_none;
    }
#endif

    if (self->doublebuffer && self->scan_buf != self->draw_buf) {
        memcpy(self->scan_buf, self->draw_buf, self->buf_len);
    }

    const uint16_t half_rows = self->height / 2;
    const uint32_t base_time_us = 20;

    for (int8_t plane = self->bit_depth - 1; plane >= 0; plane--) {
        for (uint16_t addr = 0; addr < half_rows; addr++) {
            displayif_pin_set(self->oe_pin, 1);
            rgbmatrix_set_addr(self, addr);
            mp_hal_delay_us(1);
            rgbmatrix_shift_row(self, addr, plane);
            rgbmatrix_pulse_latch(self);
            displayif_pin_set(self->oe_pin, 0);
            mp_hal_delay_us(base_time_us << plane);
        }
    }

    if (self->doublebuffer && self->scan_buf != self->draw_buf) {
        uint8_t *tmp = self->draw_buf;
        self->draw_buf = self->scan_buf;
        self->scan_buf = tmp;
    }

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(rgbmatrix_refresh_obj, rgbmatrix_refresh);

static void rgbmatrix_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    rgbmatrix_obj_t *self = MP_OBJ_TO_PTR(self_in);
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

static mp_int_t rgbmatrix_get_buffer(mp_obj_t self_in, mp_buffer_info_t *bufinfo, mp_uint_t flags) {
    (void)flags;
    rgbmatrix_obj_t *self = MP_OBJ_TO_PTR(self_in);
    bufinfo->buf = self->draw_buf;
    bufinfo->len = self->buf_len;
    bufinfo->typecode = 'H';
    return 0;
}

static mp_obj_t rgbmatrix_del(mp_obj_t self_in) {
    rgbmatrix_obj_t *self = MP_OBJ_TO_PTR(self_in);
#if defined(DISPLAYIF_RGBMATRIX_USE_PROTOMATTER)
    rgbmatrix_pm_stop(self);
#endif
    if (!self->external_fb) {
        if (self->doublebuffer && self->scan_buf != NULL && self->scan_buf != self->draw_buf) {
            rgbmatrix_free(self->scan_buf);
        }
        rgbmatrix_free(self->draw_buf);
    }
    self->draw_buf = NULL;
    self->scan_buf = NULL;
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(rgbmatrix_del_obj, rgbmatrix_del);

static const mp_rom_map_elem_t rgbmatrix_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_refresh), MP_ROM_PTR(&rgbmatrix_refresh_obj) },
    { MP_ROM_QSTR(MP_QSTR___del__), MP_ROM_PTR(&rgbmatrix_del_obj) },
};
static MP_DEFINE_CONST_DICT(rgbmatrix_locals_dict, rgbmatrix_locals_dict_table);

static MP_DEFINE_CONST_OBJ_TYPE(
    rgbmatrix_type,
    MP_QSTR_RGBMatrix,
    MP_TYPE_FLAG_NONE,
    make_new, rgbmatrix_make,
    attr, rgbmatrix_attr,
    locals_dict, &rgbmatrix_locals_dict,
    buffer, rgbmatrix_get_buffer
);

static const mp_rom_map_elem_t rgbmatrix_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_rgbmatrix) },
    { MP_ROM_QSTR(MP_QSTR_RGBMatrix), MP_ROM_PTR(&rgbmatrix_type) },
};
static MP_DEFINE_CONST_DICT(rgbmatrix_module_globals, rgbmatrix_module_globals_table);

const mp_obj_module_t rgbmatrix_user_cmodule = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&rgbmatrix_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_rgbmatrix, rgbmatrix_user_cmodule);
