// SPDX-License-Identifier: MIT
// Native SPIBus matching pydisplay/drivers/bus/spibus.py (keyword-only ctor).
// Lifecycle: idempotent deinit/__del__/ctor + soft-reset teardown (see soft_reset.h).

#include <string.h>

#include "py/runtime.h"
#include "py/obj.h"
#include "py/mphal.h"
#include "py/mpprint.h"
#include "displayif/mp_helpers.h"
#include "displayif/soft_reset.h"

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
    // Keep SPI pin objs so reinit can re-apply mux (ESP32-S3 drops pins on init() without them).
    mp_obj_t sck_pin;
    mp_obj_t mosi_pin;
    mp_obj_t miso_pin;
    mp_int_t baudrate;
    mp_int_t polarity;
    mp_int_t phase;
    mp_int_t bits;
    mp_int_t firstbit;
    mp_obj_t buf1;
    bool has_cs;
    bool has_reset;
    bool has_spi_pins;
    bool soft; // machine.SoftSPI (bitbang) — do not reinit every send
    bool deinited;
} spibus_obj_t;

static const mp_obj_type_t spibus_type;

// Active instance for soft-reset / idempotent ctor (pointer valid until gc_sweep).
static spibus_obj_t *s_active;
static bool s_soft_reset_registered;

static void spibus_deinit_internal(spibus_obj_t *self) {
    if (self == NULL || self->deinited) {
        return;
    }
    // Release ESP-IDF SPI host while the Python SPI object is still alive.
    displayif_obj_call_method0(self->spi, MP_QSTR_deinit);
    self->deinited = true;
    if (s_active == self) {
        s_active = NULL;
    }
}

static void spibus_host_teardown(void) {
    spibus_deinit_internal(s_active);
    s_active = NULL;
}

static void spibus_ensure_soft_reset_registered(void) {
    if (!s_soft_reset_registered) {
        displayif_register_soft_reset(spibus_host_teardown);
        s_soft_reset_registered = true;
    }
}

static void spibus_cs_set(spibus_obj_t *self, int level) {
    if (self->has_cs) {
        displayif_pin_set(self->cs, level);
    }
}

static void spibus_reinit_spi(spibus_obj_t *self) {
    // SoftSPI: reinit every transfer corrupts window cmds / pixel streams
    // (diagonal noise, misplaced fill_rect columns). Leave the bitbang bus alone.
    if (self->soft) {
        return;
    }
    // Restore SPI params before each transfer. On ESP32-S3, SPI.init without
    // sck/mosi clears the GPIO matrix — re-pass pins there. On rp2 (and most
    // other ports), machine.SPI.init rejects pin kwargs ("extra keyword
    // arguments given"), so only pass baud/mode fields.
    mp_obj_t kwargs = mp_obj_new_dict(0);
    mp_obj_dict_store(kwargs, MP_OBJ_NEW_QSTR(MP_QSTR_baudrate), mp_obj_new_int(self->baudrate));
    mp_obj_dict_store(kwargs, MP_OBJ_NEW_QSTR(MP_QSTR_polarity), mp_obj_new_int(self->polarity));
    mp_obj_dict_store(kwargs, MP_OBJ_NEW_QSTR(MP_QSTR_phase), mp_obj_new_int(self->phase));
    mp_obj_dict_store(kwargs, MP_OBJ_NEW_QSTR(MP_QSTR_bits), mp_obj_new_int(self->bits));
    mp_obj_dict_store(kwargs, MP_OBJ_NEW_QSTR(MP_QSTR_firstbit), mp_obj_new_int(self->firstbit));
#if defined(ESP_PLATFORM)
    if (self->has_spi_pins) {
        mp_obj_dict_store(kwargs, MP_OBJ_NEW_QSTR(MP_QSTR_sck), self->sck_pin);
        mp_obj_dict_store(kwargs, MP_OBJ_NEW_QSTR(MP_QSTR_mosi), self->mosi_pin);
        mp_obj_dict_store(kwargs, MP_OBJ_NEW_QSTR(MP_QSTR_miso), self->miso_pin);
    }
#endif
    displayif_obj_call_method_kw(self->spi, MP_QSTR_init, kwargs);
}

// Parse keyword-only args to match:
//   SPIBus(*, id=2, baudrate=24_000_000, polarity=0, phase=0, bits=8,
//           lsb_first=False, soft=False, sck=-1, mosi=-1, miso=-1,
//           dc=-1, cs=-1, reset=-1)
// Pin kwargs accept int, board name str ("D9"), or machine.Pin (mimxrt / samd).
// soft=True uses machine.SoftSPI and requires sck + mosi.
static mp_obj_t spibus_make(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    if (n_args != 0) {
        mp_raise_TypeError(MP_ERROR_TEXT("extra positional arguments given"));
    }

    mp_int_t id = 2;
    mp_int_t baudrate = 24000000;
    mp_int_t polarity = 0;
    mp_int_t phase = 0;
    mp_int_t bits = 8;
    bool lsb_first = false;
    bool soft = false;
    mp_obj_t sck = MP_OBJ_NEW_SMALL_INT(-1);
    mp_obj_t mosi = MP_OBJ_NEW_SMALL_INT(-1);
    mp_obj_t miso = MP_OBJ_NEW_SMALL_INT(-1);
    mp_obj_t dc = MP_OBJ_NEW_SMALL_INT(-1);
    mp_obj_t cs = MP_OBJ_NEW_SMALL_INT(-1);
    mp_obj_t reset = MP_OBJ_NEW_SMALL_INT(-1);

    for (size_t i = 0; i < n_kw; i++) {
        mp_obj_t key = args[2 * i];
        mp_obj_t val = args[2 * i + 1];
        if (!mp_obj_is_qstr(key)) {
            mp_raise_TypeError(MP_ERROR_TEXT("keywords must be strings"));
        }
        qstr q = MP_OBJ_QSTR_VALUE(key);
        if (q == MP_QSTR_id) {
            id = mp_obj_get_int(val);
        } else if (q == MP_QSTR_baudrate) {
            baudrate = mp_obj_get_int(val);
        } else if (q == MP_QSTR_polarity) {
            polarity = mp_obj_get_int(val);
        } else if (q == MP_QSTR_phase) {
            phase = mp_obj_get_int(val);
        } else if (q == MP_QSTR_bits) {
            bits = mp_obj_get_int(val);
        } else if (q == MP_QSTR_lsb_first) {
            lsb_first = mp_obj_is_true(val);
        } else if (q == MP_QSTR_soft) {
            soft = mp_obj_is_true(val);
        } else if (q == MP_QSTR_sck) {
            sck = val;
        } else if (q == MP_QSTR_mosi) {
            mosi = val;
        } else if (q == MP_QSTR_miso) {
            miso = val;
        } else if (q == MP_QSTR_dc) {
            dc = val;
        } else if (q == MP_QSTR_cs) {
            cs = val;
        } else if (q == MP_QSTR_reset) {
            reset = val;
        } else {
            mp_raise_TypeError(MP_ERROR_TEXT("extra keyword arguments given"));
        }
    }

    if (displayif_pin_spec_unset(dc)) {
        mp_raise_ValueError(MP_ERROR_TEXT("DC pin must be specified"));
    }
    if (soft && (displayif_pin_spec_unset(sck) || displayif_pin_spec_unset(mosi))) {
        mp_raise_ValueError(MP_ERROR_TEXT("soft SPI requires sck and mosi"));
    }

    // Tear down any prior SPIBus (soft-reset survivor or leaked instance) before recreate.
    spibus_ensure_soft_reset_registered();
    spibus_host_teardown();

    mp_printf(&mp_plat_print, "SPIBus loading...\n");

    spibus_obj_t *self = mp_obj_malloc(spibus_obj_t, type);

    mp_obj_t machine_mod = mp_import_name(MP_QSTR_machine, DISPLAYIF_EMPTY_DICT, MP_OBJ_NULL);
    mp_obj_t pin_cls = mp_load_attr(machine_mod, MP_QSTR_Pin);
    mp_int_t pin_out = mp_obj_get_int(mp_load_attr(pin_cls, MP_QSTR_OUT));
    mp_int_t pin_in = mp_obj_get_int(mp_load_attr(pin_cls, MP_QSTR_IN));
    // LSB/MSB from SoftSPI or SPI so soft path works when SoftSPI is present.
    mp_obj_t spi_cls = soft ? mp_load_attr(machine_mod, MP_QSTR_SoftSPI) : mp_load_attr(machine_mod, MP_QSTR_SPI);
    mp_int_t spi_lsb = mp_obj_get_int(mp_load_attr(spi_cls, MP_QSTR_LSB));
    mp_int_t spi_msb = mp_obj_get_int(mp_load_attr(spi_cls, MP_QSTR_MSB));

    self->baudrate = baudrate;
    self->polarity = polarity;
    self->phase = phase;
    self->bits = bits;
    self->firstbit = lsb_first ? spi_lsb : spi_msb;
    self->soft = soft;

    mp_obj_t spi_kwargs = mp_obj_new_dict(0);
    mp_obj_dict_store(spi_kwargs, MP_OBJ_NEW_QSTR(MP_QSTR_baudrate), mp_obj_new_int(self->baudrate));
    mp_obj_dict_store(spi_kwargs, MP_OBJ_NEW_QSTR(MP_QSTR_polarity), mp_obj_new_int(self->polarity));
    mp_obj_dict_store(spi_kwargs, MP_OBJ_NEW_QSTR(MP_QSTR_phase), mp_obj_new_int(self->phase));

    if (soft) {
        self->sck_pin = displayif_machine_pin_cfg(sck, pin_out, 0);
        self->mosi_pin = displayif_machine_pin_cfg(mosi, pin_out, 0);
        self->miso_pin = displayif_pin_spec_unset(miso) ? mp_const_none : displayif_machine_pin_cfg(miso, pin_in, 0);
        self->has_spi_pins = true;
        mp_obj_dict_store(spi_kwargs, MP_OBJ_NEW_QSTR(MP_QSTR_sck), self->sck_pin);
        mp_obj_dict_store(spi_kwargs, MP_OBJ_NEW_QSTR(MP_QSTR_mosi), self->mosi_pin);
        if (self->miso_pin != mp_const_none) {
            mp_obj_dict_store(spi_kwargs, MP_OBJ_NEW_QSTR(MP_QSTR_miso), self->miso_pin);
        }
        mp_obj_dict_store(spi_kwargs, MP_OBJ_NEW_QSTR(MP_QSTR_firstbit), mp_obj_new_int(self->firstbit));
        self->spi = displayif_machine_softspi(spi_kwargs);
    } else {
        mp_obj_dict_store(spi_kwargs, MP_OBJ_NEW_QSTR(MP_QSTR_bits), mp_obj_new_int(self->bits));
        mp_obj_dict_store(spi_kwargs, MP_OBJ_NEW_QSTR(MP_QSTR_firstbit), mp_obj_new_int(self->firstbit));

        self->has_spi_pins = !(displayif_pin_spec_unset(mosi) && displayif_pin_spec_unset(miso) && displayif_pin_spec_unset(sck));
        if (self->has_spi_pins) {
            self->sck_pin = displayif_machine_pin_cfg(sck, pin_out, 0);
            self->mosi_pin = displayif_machine_pin_cfg(mosi, pin_out, 0);
            self->miso_pin = displayif_pin_spec_unset(miso) ? mp_const_none : displayif_machine_pin_cfg(miso, pin_in, 0);
            mp_obj_dict_store(spi_kwargs, MP_OBJ_NEW_QSTR(MP_QSTR_sck), self->sck_pin);
            mp_obj_dict_store(spi_kwargs, MP_OBJ_NEW_QSTR(MP_QSTR_mosi), self->mosi_pin);
            mp_obj_dict_store(spi_kwargs, MP_OBJ_NEW_QSTR(MP_QSTR_miso), self->miso_pin);
        } else {
            self->sck_pin = mp_const_none;
            self->mosi_pin = mp_const_none;
            self->miso_pin = mp_const_none;
        }

        mp_obj_dict_store(spi_kwargs, MP_OBJ_NEW_QSTR(MP_QSTR_id), mp_obj_new_int(id));
        self->spi = displayif_machine_spi(spi_kwargs);
    }

    // DC/CS after SPI init (same comment as Python)
    self->dc = displayif_machine_pin_cfg(dc, pin_out, DC_DATA);

    if (!displayif_pin_spec_unset(cs)) {
        self->cs = displayif_machine_pin_cfg(cs, pin_out, CS_INACTIVE);
        self->has_cs = true;
    } else {
        self->cs = mp_const_none;
        self->has_cs = false;
    }

    if (!displayif_pin_spec_unset(reset)) {
        self->reset = displayif_machine_pin_cfg(reset, pin_out, 1);
        self->has_reset = true;
    } else {
        self->reset = mp_const_none;
        self->has_reset = false;
    }

    self->buf1 = mp_obj_new_bytearray(1, (byte[]){0});
    self->deinited = false;
    s_active = self;
    mp_printf(&mp_plat_print, soft ? "SPIBus loaded (SoftSPI)\n" : "SPIBus loaded\n");
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
        // Must write via buffer protocol — MP_OBJ_TO_PTR(bytearray) is not payload.
        uint8_t cmd = (uint8_t)mp_obj_get_int(command);
        mp_buffer_info_t bufinfo;
        mp_get_buffer_raise(self->buf1, &bufinfo, MP_BUFFER_WRITE);
        ((uint8_t *)bufinfo.buf)[0] = cmd;
        displayif_pin_set(self->dc, DC_CMD);
        displayif_obj_call_method1(self->spi, MP_QSTR_write, self->buf1);
    }

    // Match Python: `if data and len(data):`
    if (data != mp_const_none && data != mp_const_false && data != mp_const_empty_bytes) {
        mp_buffer_info_t bufinfo;
        if (mp_get_buffer(data, &bufinfo, MP_BUFFER_READ)) {
            if (bufinfo.len > 0) {
                displayif_pin_set(self->dc, DC_DATA);
                displayif_obj_call_method1(self->spi, MP_QSTR_write, data);
            }
        }
    }

    spibus_cs_set(self, CS_INACTIVE);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(spibus_send_obj, 1, 3, spibus_send);

static mp_obj_t spibus_deinit(mp_obj_t self_in) {
    spibus_deinit_internal(MP_OBJ_TO_PTR(self_in));
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
