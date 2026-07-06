// SPDX-License-Identifier: MIT
// Dot-clock RGB framebuffer for esp32 port (ESP-IDF scanout backend — WIP).

#include <string.h>
#include <stdlib.h>

#include "py/runtime.h"
#include "py/obj.h"
#include "py/binary.h"
#include "displayif/mp_helpers.h"

#if defined(ESP_PLATFORM)
#include "esp_heap_caps.h"
#endif

typedef struct _rgbframebuffer_obj_t {
    mp_obj_base_t base;
    uint16_t width;
    uint16_t height;
    uint16_t row_stride;
    uint8_t *buf;
    size_t buf_len;
    mp_obj_t pin_args;
} rgbframebuffer_obj_t;

static const mp_obj_type_t rgbframebuffer_type;

static uint8_t *rgbframebuffer_alloc(size_t nbytes) {
#if defined(ESP_PLATFORM)
    uint8_t *ptr = heap_caps_malloc(nbytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (ptr == NULL) {
        ptr = heap_caps_malloc(nbytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    return ptr;
#else
    return malloc(nbytes);
#endif
}

static void rgbframebuffer_free(uint8_t *ptr) {
#if defined(ESP_PLATFORM)
    heap_caps_free(ptr);
#else
    free(ptr);
#endif
}

static mp_obj_t rgbframebuffer_store_pin(mp_obj_t dict, qstr key, mp_obj_t value) {
    mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(key), value);
    return value;
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

    rgbframebuffer_obj_t *self = mp_obj_malloc(rgbframebuffer_obj_t, type);
    self->width = vals[ARG_width].u_int;
    self->height = vals[ARG_height].u_int;
    self->row_stride = self->width * sizeof(uint16_t);
    self->buf_len = (size_t)self->row_stride * self->height;
    self->buf = rgbframebuffer_alloc(self->buf_len);
    if (self->buf == NULL) {
        mp_raise_msg(&mp_type_MemoryError, MP_ERROR_TEXT("RGB framebuffer allocation failed"));
    }
    memset(self->buf, 0, self->buf_len);

    self->pin_args = mp_obj_new_dict(0);
    rgbframebuffer_store_pin(self->pin_args, MP_QSTR_de, vals[ARG_de].u_obj);
    rgbframebuffer_store_pin(self->pin_args, MP_QSTR_vsync, vals[ARG_vsync].u_obj);
    rgbframebuffer_store_pin(self->pin_args, MP_QSTR_hsync, vals[ARG_hsync].u_obj);
    rgbframebuffer_store_pin(self->pin_args, MP_QSTR_dclk, vals[ARG_dclk].u_obj);
    if (rgb666) {
        if (vals[ARG_green].u_obj == MP_OBJ_NULL || vals[ARG_blue].u_obj == MP_OBJ_NULL) {
            mp_raise_ValueError(MP_ERROR_TEXT("red/green/blue pin tuples required together"));
        }
        rgbframebuffer_store_pin(self->pin_args, MP_QSTR_red, vals[ARG_red].u_obj);
        rgbframebuffer_store_pin(self->pin_args, MP_QSTR_green, vals[ARG_green].u_obj);
        rgbframebuffer_store_pin(self->pin_args, MP_QSTR_blue, vals[ARG_blue].u_obj);
    } else {
        rgbframebuffer_store_pin(self->pin_args, MP_QSTR_data, vals[ARG_data].u_obj);
    }

    (void)vals[ARG_frequency];
    (void)vals[ARG_hsync_pulse_width];
    (void)vals[ARG_hsync_front_porch];
    (void)vals[ARG_hsync_back_porch];
    (void)vals[ARG_vsync_pulse_width];
    (void)vals[ARG_vsync_front_porch];
    (void)vals[ARG_vsync_back_porch];
    (void)vals[ARG_hsync_idle_low];
    (void)vals[ARG_vsync_idle_low];
    (void)vals[ARG_de_idle_high];
    (void)vals[ARG_pclk_active_high];
    (void)vals[ARG_pclk_idle_high];
    (void)vals[ARG_overscan_left];

    return MP_OBJ_FROM_PTR(self);
}

static mp_obj_t rgbframebuffer_refresh(mp_obj_t self_in) {
    (void)self_in;
    mp_raise_msg(&mp_type_NotImplementedError, MP_ERROR_TEXT("ESP-IDF dotclock scanout not implemented yet"));
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
    if (self->buf != NULL) {
        rgbframebuffer_free(self->buf);
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
