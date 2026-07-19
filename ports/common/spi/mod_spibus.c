// SPDX-License-Identifier: MIT
// pydisplay-compatible SPI display bus (replaces viper spibus when displayif is built in).
// Lifecycle: idempotent deinit/__del__ (machine.SPI owns hardware teardown on soft reset).

#include <string.h>

#include "py/runtime.h"
#include "py/obj.h"
#include "py/mphal.h"
#include "displayif/mp_helpers.h"

#define DC_CMD 0
#define DC_DATA 1
#define CS_ACTIVE 0
#define CS_INACTIVE 1

typedef struct _spibus_obj_t {
    mp_obj_base_t base;
    mp_obj_t spi;
    mp_obj_t dc;
    mp_obj_t cs;
    mp_obj_t reset;
    mp_int_t baudrate;
    mp_int_t polarity;
    mp_int_t phase;
    mp_int_t bits;
    mp_int_t firstbit;
    mp_obj_t buf1;
    bool has_cs;
    bool has_reset;
    bool deinited;
} spibus_obj_t;

static const mp_obj_type_t spibus_type;

static void spibus_cs_set(spibus_obj_t *self, int level) {
    if (self->has_cs) {
        displayif_pin_set(self->cs, level);
    }
}

static void spibus_reinit_spi(spibus_obj_t *self) {
    mp_obj_t kwargs = mp_obj_new_dict(0);
    mp_obj_dict_store(kwargs, MP_OBJ_NEW_QSTR(MP_QSTR_baudrate), mp_obj_new_int(self->baudrate));
    mp_obj_dict_store(kwargs, MP_OBJ_NEW_QSTR(MP_QSTR_polarity), mp_obj_new_int(self->polarity));
    mp_obj_dict_store(kwargs, MP_OBJ_NEW_QSTR(MP_QSTR_phase), mp_obj_new_int(self->phase));
    mp_obj_dict_store(kwargs, MP_OBJ_NEW_QSTR(MP_QSTR_bits), mp_obj_new_int(self->bits));
    mp_obj_dict_store(kwargs, MP_OBJ_NEW_QSTR(MP_QSTR_firstbit), mp_obj_new_int(self->firstbit));
    displayif_obj_call_method_kw(self->spi, MP_QSTR_init, kwargs);
}

static mp_obj_t spibus_make(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    enum {
        ARG_id,
        ARG_baudrate,
        ARG_polarity,
        ARG_phase,
        ARG_bits,
        ARG_lsb_first,
        ARG_sck,
        ARG_mosi,
        ARG_miso,
        ARG_dc,
        ARG_cs,
        ARG_reset,
    };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_id, MP_ARG_KW_ONLY | MP_ARG_INT, { .u_int = 2 } },
        { MP_QSTR_baudrate, MP_ARG_KW_ONLY | MP_ARG_INT, { .u_int = 24000000 } },
        { MP_QSTR_polarity, MP_ARG_KW_ONLY | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_phase, MP_ARG_KW_ONLY | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_bits, MP_ARG_KW_ONLY | MP_ARG_INT, { .u_int = 8 } },
        { MP_QSTR_lsb_first, MP_ARG_KW_ONLY | MP_ARG_BOOL, { .u_bool = false } },
        { MP_QSTR_sck, MP_ARG_KW_ONLY | MP_ARG_INT, { .u_int = -1 } },
        { MP_QSTR_mosi, MP_ARG_KW_ONLY | MP_ARG_INT, { .u_int = -1 } },
        { MP_QSTR_miso, MP_ARG_KW_ONLY | MP_ARG_INT, { .u_int = -1 } },
        { MP_QSTR_dc, MP_ARG_KW_ONLY | MP_ARG_INT, { .u_int = -1 } },
        { MP_QSTR_cs, MP_ARG_KW_ONLY | MP_ARG_INT, { .u_int = -1 } },
        { MP_QSTR_reset, MP_ARG_KW_ONLY | MP_ARG_INT, { .u_int = -1 } },
    };
    mp_arg_val_t vals[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, args, MP_ARRAY_SIZE(allowed_args), allowed_args, vals);

    if (vals[ARG_dc].u_int < 0) {
        mp_raise_ValueError(MP_ERROR_TEXT("DC pin must be specified"));
    }

    spibus_obj_t *self = mp_obj_malloc(spibus_obj_t, type);

    mp_obj_t machine_mod = mp_import_name(MP_QSTR_machine, DISPLAYIF_EMPTY_DICT, MP_OBJ_NULL);
    mp_obj_t pin_cls = mp_load_attr(machine_mod, MP_QSTR_Pin);
    mp_int_t pin_out = mp_obj_get_int(mp_load_attr(pin_cls, MP_QSTR_OUT));
    mp_int_t pin_in = mp_obj_get_int(mp_load_attr(pin_cls, MP_QSTR_IN));
    mp_obj_t spi_mod = mp_load_attr(machine_mod, MP_QSTR_SPI);
    mp_int_t spi_lsb = mp_obj_get_int(mp_load_attr(spi_mod, MP_QSTR_LSB));
    mp_int_t spi_msb = mp_obj_get_int(mp_load_attr(spi_mod, MP_QSTR_MSB));

    self->baudrate = vals[ARG_baudrate].u_int;
    self->polarity = vals[ARG_polarity].u_int;
    self->phase = vals[ARG_phase].u_int;
    self->bits = vals[ARG_bits].u_int;
    self->firstbit = vals[ARG_lsb_first].u_bool ? spi_lsb : spi_msb;

    mp_obj_t spi_kwargs = mp_obj_new_dict(0);
    mp_obj_dict_store(spi_kwargs, MP_OBJ_NEW_QSTR(MP_QSTR_id), mp_obj_new_int(vals[ARG_id].u_int));
    mp_obj_dict_store(spi_kwargs, MP_OBJ_NEW_QSTR(MP_QSTR_baudrate), mp_obj_new_int(self->baudrate));
    mp_obj_dict_store(spi_kwargs, MP_OBJ_NEW_QSTR(MP_QSTR_polarity), mp_obj_new_int(self->polarity));
    mp_obj_dict_store(spi_kwargs, MP_OBJ_NEW_QSTR(MP_QSTR_phase), mp_obj_new_int(self->phase));
    mp_obj_dict_store(spi_kwargs, MP_OBJ_NEW_QSTR(MP_QSTR_bits), mp_obj_new_int(self->bits));
    mp_obj_dict_store(spi_kwargs, MP_OBJ_NEW_QSTR(MP_QSTR_firstbit), mp_obj_new_int(self->firstbit));

    if (vals[ARG_sck].u_int >= 0 || vals[ARG_mosi].u_int >= 0 || vals[ARG_miso].u_int >= 0) {
        if (vals[ARG_sck].u_int >= 0) {
            mp_obj_dict_store(spi_kwargs, MP_OBJ_NEW_QSTR(MP_QSTR_sck),
                displayif_machine_pin(vals[ARG_sck].u_int, pin_out, 0));
        }
        if (vals[ARG_mosi].u_int >= 0) {
            mp_obj_dict_store(spi_kwargs, MP_OBJ_NEW_QSTR(MP_QSTR_mosi),
                displayif_machine_pin(vals[ARG_mosi].u_int, pin_out, 0));
        }
        if (vals[ARG_miso].u_int >= 0) {
            mp_obj_dict_store(spi_kwargs, MP_OBJ_NEW_QSTR(MP_QSTR_miso),
                displayif_machine_pin(vals[ARG_miso].u_int, pin_in, 0));
        }
    }

    self->spi = displayif_machine_spi(spi_kwargs);

    self->dc = displayif_machine_pin(vals[ARG_dc].u_int, pin_out, DC_DATA);

    if (vals[ARG_cs].u_int >= 0) {
        self->cs = displayif_machine_pin(vals[ARG_cs].u_int, pin_out, CS_INACTIVE);
        self->has_cs = true;
    } else {
        self->cs = mp_const_none;
        self->has_cs = false;
    }

    if (vals[ARG_reset].u_int >= 0) {
        self->reset = displayif_machine_pin(vals[ARG_reset].u_int, pin_out, 1);
        self->has_reset = true;
    } else {
        self->reset = mp_const_none;
        self->has_reset = false;
    }

    self->buf1 = mp_obj_new_bytearray(1, (byte[]){0});
    self->deinited = false;
    return MP_OBJ_FROM_PTR(self);
}

static mp_obj_t spibus_reset(mp_obj_t self_in) {
    spibus_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (self->deinited) {
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("spibus is deinited"));
    }
    if (!self->has_reset) {
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("No reset pin defined"));
    }
    displayif_pin_set(self->reset, 0);
    mp_hal_delay_ms(10);
    displayif_pin_set(self->reset, 1);
    mp_hal_delay_ms(10);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(spibus_reset_obj, spibus_reset);

static mp_obj_t spibus_send(size_t n_args, const mp_obj_t *args) {
    spibus_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    if (self->deinited) {
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("spibus is deinited"));
    }
    mp_obj_t command = (n_args > 1) ? args[1] : mp_const_none;
    mp_obj_t data = (n_args > 2) ? args[2] : mp_const_none;

    spibus_reinit_spi(self);
    spibus_cs_set(self, CS_ACTIVE);

    if (command != mp_const_none) {
        uint8_t cmd = mp_obj_get_int(command);
        ((uint8_t *)MP_OBJ_TO_PTR(self->buf1))[0] = cmd;
        displayif_pin_set(self->dc, DC_CMD);
        displayif_obj_call_method1(self->spi, MP_QSTR_write, self->buf1);
    }

    if (data != mp_const_none && data != mp_const_empty_bytes) {
        mp_buffer_info_t bufinfo;
        mp_get_buffer_raise(data, &bufinfo, MP_BUFFER_READ);
        if (bufinfo.len > 0) {
            displayif_pin_set(self->dc, DC_DATA);
            displayif_obj_call_method1(self->spi, MP_QSTR_write, data);
        }
    }

    spibus_cs_set(self, CS_INACTIVE);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(spibus_send_obj, 1, 3, spibus_send);

static mp_obj_t spibus_deinit(mp_obj_t self_in) {
    spibus_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (self->deinited) {
        return mp_const_none;
    }
    displayif_obj_call_method0(self->spi, MP_QSTR_deinit);
    self->deinited = true;
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(spibus_deinit_obj, spibus_deinit);

static const mp_rom_map_elem_t spibus_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_reset), MP_ROM_PTR(&spibus_reset_obj) },
    { MP_ROM_QSTR(MP_QSTR_send), MP_ROM_PTR(&spibus_send_obj) },
    { MP_ROM_QSTR(MP_QSTR_deinit), MP_ROM_PTR(&spibus_deinit_obj) },
    { MP_ROM_QSTR(MP_QSTR___del__), MP_ROM_PTR(&spibus_deinit_obj) },
};
static MP_DEFINE_CONST_DICT(spibus_locals_dict, spibus_locals_dict_table);

static MP_DEFINE_CONST_OBJ_TYPE(
    spibus_type,
    MP_QSTR_SPIBus,
    MP_TYPE_FLAG_NONE,
    make_new, spibus_make,
    locals_dict, &spibus_locals_dict
);

static const mp_rom_map_elem_t spibus_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_spibus) },
    { MP_ROM_QSTR(MP_QSTR_SPIBus), MP_ROM_PTR(&spibus_type) },
    { MP_ROM_QSTR(MP_QSTR_DC_CMD), MP_ROM_INT(DC_CMD) },
    { MP_ROM_QSTR(MP_QSTR_DC_DATA), MP_ROM_INT(DC_DATA) },
    { MP_ROM_QSTR(MP_QSTR_CS_ACTIVE), MP_ROM_INT(CS_ACTIVE) },
    { MP_ROM_QSTR(MP_QSTR_CS_INACTIVE), MP_ROM_INT(CS_INACTIVE) },
};
static MP_DEFINE_CONST_DICT(spibus_module_globals, spibus_module_globals_table);

const mp_obj_module_t spibus_user_cmodule = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&spibus_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_spibus, spibus_user_cmodule);
