// SPDX-License-Identifier: MIT
// Intel 8080 parallel display bus for SAMD51 (GPIO bit-bang via common backend).

#include <string.h>

#include "py/runtime.h"
#include "py/obj.h"
#include "py/binary.h"
#include "displayif/mp_helpers.h"
#include "displayif/i80bus_gpio.h"
#include "pin.h"

#if defined(MCU_SAMD51) || defined(__SAMD51__)

#include "sam.h"

#define I80BUS_DATA_WIDTH DISPLAYIF_I80BUS_GPIO_WIDTH
#define I80BUS_GPIO_GROUPS DISPLAYIF_I80BUS_GPIO_MAX_GROUPS

typedef struct _i80bus_obj_t {
    mp_obj_base_t base;
    displayif_i80bus_gpio_t gpio;
    mp_obj_t dc_pin;
    mp_obj_t cs_pin;
    bool has_cs;
} i80bus_obj_t;

static const mp_obj_type_t i80bus_type;

static volatile uint32_t *samd_gpio_outset[I80BUS_GPIO_GROUPS];
static volatile uint32_t *samd_gpio_outclr[I80BUS_GPIO_GROUPS];

static void samd_i80bus_bind_gpio_groups(void) {
    for (uint8_t i = 0; i < I80BUS_GPIO_GROUPS; i++) {
        samd_gpio_outset[i] = &PORT->Group[i].OUTSET.reg;
        samd_gpio_outclr[i] = &PORT->Group[i].OUTCLR.reg;
    }
}

static int samd_i80bus_pin_id(mp_obj_t pin_or_int) {
    if (mp_obj_is_small_int(pin_or_int) || mp_obj_is_int(pin_or_int)) {
        return mp_obj_get_int(pin_or_int);
    }
    const machine_pin_obj_t *pin = pin_find(pin_or_int);
    return pin->pin_id;
}

static size_t samd_i80bus_pin_seq_to_ints(mp_obj_t seq, int *out, size_t max_out) {
    size_t len;
    mp_obj_t *items;
    if (mp_obj_is_type(seq, &mp_type_tuple)) {
        mp_obj_tuple_get(seq, &len, &items);
    } else if (mp_obj_is_type(seq, &mp_type_list)) {
        mp_obj_list_get(seq, &len, &items);
    } else {
        mp_raise_TypeError(MP_ERROR_TEXT("expected a sequence of pins"));
    }
    if (len > max_out) {
        mp_raise_ValueError(MP_ERROR_TEXT("too many pins in sequence"));
    }
    for (size_t i = 0; i < len; i++) {
        out[i] = samd_i80bus_pin_id(items[i]);
    }
    return len;
}

static void samd_i80bus_hi_drive(int pin_id) {
    uint8_t port = (uint8_t)(pin_id / 32);
    uint8_t bit = (uint8_t)(pin_id & 31);
    PORT->Group[port].WRCONFIG.reg =
        (bit >= 16)
            ? PORT_WRCONFIG_WRPINCFG | PORT_WRCONFIG_DRVSTR |
                  PORT_WRCONFIG_HWSEL | (1u << (bit & 15))
            : PORT_WRCONFIG_WRPINCFG | PORT_WRCONFIG_DRVSTR | (1u << bit);
}

static void samd_i80bus_pin_output(int pin_id) {
    mp_obj_t pin = displayif_machine_pin(pin_id, 1, 0);
    (void)pin;
    samd_i80bus_hi_drive(pin_id);
}

static mp_obj_t i80bus_make(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    enum {
        ARG_dc,
        ARG_cs,
        ARG_wr,
        ARG_data,
        ARG_freq,
    };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_dc, MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_OBJ, { .u_obj = MP_OBJ_NULL } },
        { MP_QSTR_cs, MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_OBJ, { .u_obj = MP_OBJ_NULL } },
        { MP_QSTR_wr, MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_OBJ, { .u_obj = MP_OBJ_NULL } },
        { MP_QSTR_data, MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_OBJ, { .u_obj = MP_OBJ_NULL } },
        { MP_QSTR_freq, MP_ARG_KW_ONLY | MP_ARG_INT, { .u_int = 20000000 } },
    };
    mp_arg_val_t vals[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, args, MP_ARRAY_SIZE(allowed_args), allowed_args, vals);

    if (vals[ARG_dc].u_obj == MP_OBJ_NULL || vals[ARG_wr].u_obj == MP_OBJ_NULL) {
        mp_raise_ValueError(MP_ERROR_TEXT("dc and wr pins must be specified"));
    }
    if (vals[ARG_freq].u_int <= 0) {
        mp_raise_ValueError(MP_ERROR_TEXT("freq must be positive"));
    }
    (void)vals[ARG_freq];

    int data_pins[I80BUS_DATA_WIDTH];
    size_t data_count = samd_i80bus_pin_seq_to_ints(vals[ARG_data].u_obj, data_pins, I80BUS_DATA_WIDTH);
    if (data_count != I80BUS_DATA_WIDTH) {
        mp_raise_ValueError(MP_ERROR_TEXT("data must be 8 pins"));
    }

    int dc_id = samd_i80bus_pin_id(vals[ARG_dc].u_obj);
    int wr_id = samd_i80bus_pin_id(vals[ARG_wr].u_obj);
    bool has_cs = vals[ARG_cs].u_obj != MP_OBJ_NULL;
    int cs_id = 0;
    if (has_cs) {
        cs_id = samd_i80bus_pin_id(vals[ARG_cs].u_obj);
    }

    samd_i80bus_bind_gpio_groups();
    samd_i80bus_pin_output(dc_id);
    samd_i80bus_pin_output(wr_id);
    if (has_cs) {
        samd_i80bus_pin_output(cs_id);
    }
    for (size_t i = 0; i < data_count; i++) {
        samd_i80bus_pin_output(data_pins[i]);
    }

    i80bus_obj_t *self = mp_obj_malloc(i80bus_obj_t, type);
    memset(&self->gpio, 0, sizeof(self->gpio));

    displayif_i80bus_gpio_init_wr(&self->gpio,
        samd_gpio_outset[wr_id / 32], samd_gpio_outclr[wr_id / 32],
        1u << (wr_id & 31));

    if (!displayif_i80bus_gpio_init_data(&self->gpio,
            samd_gpio_outset, samd_gpio_outclr, data_pins, data_count)) {
        mp_raise_ValueError(MP_ERROR_TEXT("invalid 8-bit data pin layout"));
    }

    self->dc_pin = displayif_pin_resolve(vals[ARG_dc].u_obj);
    self->has_cs = has_cs;
    if (has_cs) {
        self->cs_pin = displayif_pin_resolve(vals[ARG_cs].u_obj);
        displayif_pin_set(self->cs_pin, 1);
    } else {
        self->cs_pin = MP_OBJ_NULL;
    }
    displayif_pin_set(self->dc_pin, 0);
    return MP_OBJ_FROM_PTR(self);
}

static mp_obj_t i80bus_send(size_t n_args, const mp_obj_t *args) {
    i80bus_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    mp_obj_t command = (n_args > 1) ? args[1] : mp_const_none;
    mp_obj_t data = (n_args > 2) ? args[2] : mp_const_none;

    if (self->has_cs) {
        displayif_pin_set(self->cs_pin, 0);
    }

    if (command != mp_const_none) {
        uint8_t cmd = mp_obj_get_int(command);
        displayif_pin_set(self->dc_pin, 0);
        displayif_i80bus_gpio_write(&self->gpio, &cmd, 1);
    }

    if (data != mp_const_none && data != mp_const_empty_bytes) {
        mp_buffer_info_t bufinfo;
        mp_get_buffer_raise(data, &bufinfo, MP_BUFFER_READ);
        if (bufinfo.len > 0) {
            displayif_pin_set(self->dc_pin, 1);
            displayif_i80bus_gpio_write(&self->gpio, bufinfo.buf, bufinfo.len);
        }
    }

    if (self->has_cs) {
        displayif_pin_set(self->cs_pin, 1);
    }

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(i80bus_send_obj, 1, 3, i80bus_send);

static mp_obj_t i80bus_deinit(mp_obj_t self_in) {
    (void)self_in;
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(i80bus_deinit_obj, i80bus_deinit);

static const mp_rom_map_elem_t i80bus_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_send), MP_ROM_PTR(&i80bus_send_obj) },
    { MP_ROM_QSTR(MP_QSTR_deinit), MP_ROM_PTR(&i80bus_deinit_obj) },
};
static MP_DEFINE_CONST_DICT(i80bus_locals_dict, i80bus_locals_dict_table);

static MP_DEFINE_CONST_OBJ_TYPE(
    i80bus_type,
    MP_QSTR_I80Bus,
    MP_TYPE_FLAG_NONE,
    make_new, i80bus_make,
    locals_dict, &i80bus_locals_dict
);

static const mp_rom_map_elem_t i80bus_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_i80bus) },
    { MP_ROM_QSTR(MP_QSTR_I80Bus), MP_ROM_PTR(&i80bus_type) },
};
static MP_DEFINE_CONST_DICT(i80bus_module_globals, i80bus_module_globals_table);

const mp_obj_module_t i80bus_user_cmodule = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&i80bus_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_i80bus, i80bus_user_cmodule);

#endif /* SAMD51 */
