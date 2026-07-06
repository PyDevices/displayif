// SPDX-License-Identifier: MIT
// pydisplay-compatible I2C display bus (SSD1306-class OLED controllers).

#include <string.h>

#include "py/runtime.h"
#include "py/obj.h"
#include "py/misc.h"
#include "displayif/mp_helpers.h"

#define CO_CMD 0x00
#define CO_DATA 0x40

typedef struct _i2cbus_obj_t {
    mp_obj_base_t base;
    mp_obj_t i2c;
    uint8_t address;
    vstr_t vstr;
} i2cbus_obj_t;

static const mp_obj_type_t i2cbus_type;

static mp_obj_t i2cbus_make(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    enum {
        ARG_i2c,
        ARG_address,
    };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_i2c, MP_ARG_REQUIRED | MP_ARG_OBJ, { .u_obj = MP_OBJ_NULL } },
        { MP_QSTR_address, MP_ARG_KW_ONLY | MP_ARG_INT, { .u_int = 0x3C } },
    };
    mp_arg_val_t vals[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, args, MP_ARRAY_SIZE(allowed_args), allowed_args, vals);

    i2cbus_obj_t *self = mp_obj_malloc(i2cbus_obj_t, type);
    self->i2c = vals[ARG_i2c].u_obj;
    self->address = vals[ARG_address].u_int;
    vstr_init(&self->vstr, 32);
    return MP_OBJ_FROM_PTR(self);
}

static void i2cbus_writeto(i2cbus_obj_t *self, const uint8_t *data, size_t len) {
    mp_obj_t args[2] = {
        mp_obj_new_int(self->address),
        mp_obj_new_bytes(data, len),
    };
    mp_call_function_n_kw(mp_load_attr(self->i2c, MP_QSTR_writeto), 2, 0, args);
}

static mp_obj_t i2cbus_send(size_t n_args, const mp_obj_t *args) {
    i2cbus_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    mp_obj_t command = (n_args > 1) ? args[1] : mp_const_none;
    mp_obj_t data = (n_args > 2) ? args[2] : mp_const_none;

    vstr_reset(&self->vstr);
    vstr_add_byte(&self->vstr, CO_CMD);

    if (command != mp_const_none) {
        if (mp_obj_is_small_int(command) || mp_obj_is_int(command)) {
            vstr_add_byte(&self->vstr, mp_obj_get_int(command));
        } else {
            mp_buffer_info_t cmdinfo;
            mp_get_buffer_raise(command, &cmdinfo, MP_BUFFER_READ);
            vstr_add_strn(&self->vstr, cmdinfo.buf, cmdinfo.len);
        }
    }

    if (data != mp_const_none && data != mp_const_empty_bytes) {
        mp_buffer_info_t datainfo;
        mp_get_buffer_raise(data, &datainfo, MP_BUFFER_READ);
        if (datainfo.len > 0) {
            vstr_add_strn(&self->vstr, datainfo.buf, datainfo.len);
        }
    }

    i2cbus_writeto(self, (const uint8_t *)self->vstr.buf, self->vstr.len);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(i2cbus_send_obj, 1, 3, i2cbus_send);

static mp_obj_t i2cbus_send_data(mp_obj_t self_in, mp_obj_t data_in) {
    i2cbus_obj_t *self = MP_OBJ_TO_PTR(self_in);
    mp_buffer_info_t datainfo;
    mp_get_buffer_raise(data_in, &datainfo, MP_BUFFER_READ);

    vstr_reset(&self->vstr);
    vstr_add_byte(&self->vstr, CO_DATA);
    if (datainfo.len > 0) {
        vstr_add_strn(&self->vstr, datainfo.buf, datainfo.len);
    }
    i2cbus_writeto(self, (const uint8_t *)self->vstr.buf, self->vstr.len);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(i2cbus_send_data_obj, i2cbus_send_data);

static const mp_rom_map_elem_t i2cbus_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_send), MP_ROM_PTR(&i2cbus_send_obj) },
    { MP_ROM_QSTR(MP_QSTR_send_data), MP_ROM_PTR(&i2cbus_send_data_obj) },
};
static MP_DEFINE_CONST_DICT(i2cbus_locals_dict, i2cbus_locals_dict_table);

static MP_DEFINE_CONST_OBJ_TYPE(
    i2cbus_type,
    MP_QSTR_I2CBus,
    MP_TYPE_FLAG_NONE,
    make_new, i2cbus_make,
    locals_dict, &i2cbus_locals_dict
);

static const mp_rom_map_elem_t i2cbus_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_i2cbus) },
    { MP_ROM_QSTR(MP_QSTR_I2CBus), MP_ROM_PTR(&i2cbus_type) },
};
static MP_DEFINE_CONST_DICT(i2cbus_module_globals, i2cbus_module_globals_table);

const mp_obj_module_t i2cbus_user_cmodule = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&i2cbus_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_i2cbus, i2cbus_user_cmodule);
