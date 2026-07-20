// SPDX-License-Identifier: MIT
// Intel 8080 parallel display bus for MIMXRT1062 (NXP FlexIO MCULCD, 8-bit).
// Lifecycle: idempotent deinit/__del__/ctor + soft-reset teardown (see soft_reset.h).

#include <string.h>
#include <stdlib.h>

#include "py/runtime.h"
#include "py/obj.h"
#include "py/binary.h"
#include "displayif/mp_helpers.h"
#include "displayif/soft_reset.h"
#include "pin.h"

#if defined(MIMXRT1062) || defined(CPU_MIMXRT1062) || defined(__IMXRT1062__)

#include "fsl_clock.h"
#include "fsl_flexio_mculcd.h"
#include "fsl_iomuxc.h"

#if defined(CLOCK_CONFIG_H)
#include CLOCK_CONFIG_H
#endif

#ifndef BOARD_BOOTCLOCKRUN_FLEXIO2_CLK_ROOT
#define BOARD_BOOTCLOCKRUN_FLEXIO2_CLK_ROOT 30000000UL
#endif

#ifndef FLEXIO_MCULCD_DATA_BUS_WIDTH
#define FLEXIO_MCULCD_DATA_BUS_WIDTH 8UL
#endif

#define I80BUS_DATA_WIDTH 8
#define I80BUS_FLEXIO2_ALT PIN_AF_MODE_ALT4
#define I80BUS_BULK_THRESHOLD 64

typedef struct _i80bus_obj_t {
    mp_obj_base_t base;
    FLEXIO_MCULCD_Type lcd;
    mp_obj_t dc_pin;
    mp_obj_t cs_pin;
    bool has_cs;
    bool initialized;
    bool deinited;
} i80bus_obj_t;

static const mp_obj_type_t i80bus_type;
static i80bus_obj_t *s_callback_bus;
static bool s_flexio_in_use;
static FLEXIO_MCULCD_Type s_shadow_lcd;
static bool s_soft_reset_registered;

static void i80bus_host_teardown(void) {
    if (!s_flexio_in_use) {
        return;
    }
    FLEXIO_MCULCD_Deinit(&s_shadow_lcd);
    FLEXIO_Deinit(FLEXIO2);
    s_flexio_in_use = false;
    s_callback_bus = NULL;
}

static void i80bus_ensure_soft_reset_registered(void) {
    if (!s_soft_reset_registered) {
        displayif_register_soft_reset(i80bus_host_teardown);
        s_soft_reset_registered = true;
    }
}

static void i80bus_deinit_internal(i80bus_obj_t *self) {
    if (self != NULL && self->deinited) {
        return;
    }
    i80bus_host_teardown();
    if (self != NULL) {
        self->initialized = false;
        self->deinited = true;
    }
}

static void i80bus_raise_status(status_t status) {
    if (status == kStatus_Success) {
        return;
    }
    mp_raise_msg_varg(&mp_type_OSError, MP_ERROR_TEXT("NXP SDK status %d"), (int)status);
}

static int i80bus_flexio2_index(const machine_pin_obj_t *pin) {
    const char *name = qstr_str(pin->name);
    if (strncmp(name, "GPIO_B0_", 8) == 0) {
        int n = atoi(name + 8);
        if (n >= 0 && n <= 15) {
            return n;
        }
    }
    if (strncmp(name, "GPIO_B1_", 8) == 0) {
        int n = atoi(name + 8);
        if (n >= 0 && n <= 15) {
            return 16 + n;
        }
    }
    return -1;
}

static void i80bus_mux_flexio2(const machine_pin_obj_t *pin) {
    uint32_t pad_config = pin_generate_config(PIN_PULL_DISABLED, PIN_MODE_ALT, PIN_DRIVE_6, pin->configRegister);
    IOMUXC_SetPinMux(pin->muxRegister, I80BUS_FLEXIO2_ALT, 0, 0, pin->configRegister, 0);
    IOMUXC_SetPinConfig(pin->muxRegister, I80BUS_FLEXIO2_ALT, 0, 0, pin->configRegister, pad_config);
}

static void i80bus_set_cspin(bool set) {
    if (s_callback_bus == NULL || !s_callback_bus->has_cs) {
        return;
    }
    displayif_pin_set(s_callback_bus->cs_pin, set ? 1 : 0);
}

static void i80bus_set_rspin(bool set) {
    if (s_callback_bus == NULL) {
        return;
    }
    displayif_pin_set(s_callback_bus->dc_pin, set ? 1 : 0);
}

static uint8_t i80bus_pick_rd_pin(uint8_t data_start, uint8_t wr_index) {
    for (uint8_t i = 0; i < 32; i++) {
        if (i >= data_start && i < data_start + I80BUS_DATA_WIDTH) {
            continue;
        }
        if (i == wr_index) {
            continue;
        }
        return i;
    }
    return 31;
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

    if (s_flexio_in_use) {
        i80bus_host_teardown();
    }
    i80bus_ensure_soft_reset_registered();

    mp_obj_t data_pins[I80BUS_DATA_WIDTH];
    size_t data_count = displayif_pin_seq_to_objs(vals[ARG_data].u_obj, data_pins, I80BUS_DATA_WIDTH);
    if (data_count != I80BUS_DATA_WIDTH) {
        mp_raise_ValueError(MP_ERROR_TEXT("data must be 8 consecutive FlexIO2 pins"));
    }

    const machine_pin_obj_t *data_pin_objs[I80BUS_DATA_WIDTH];
    int flexio_indices[I80BUS_DATA_WIDTH];
    for (size_t i = 0; i < I80BUS_DATA_WIDTH; i++) {
        data_pin_objs[i] = pin_find(data_pins[i]);
        flexio_indices[i] = i80bus_flexio2_index(data_pin_objs[i]);
        if (flexio_indices[i] < 0) {
            mp_raise_ValueError(MP_ERROR_TEXT("data pins must map to FLEXIO2 on GPIO_B0/GPIO_B1"));
        }
    }
    for (size_t i = 1; i < I80BUS_DATA_WIDTH; i++) {
        if (flexio_indices[i] != flexio_indices[i - 1] + 1) {
            mp_raise_ValueError(MP_ERROR_TEXT("data pins must be consecutive FlexIO2 indices"));
        }
    }

    const machine_pin_obj_t *wr_pin_obj = pin_find(displayif_pin_resolve(vals[ARG_wr].u_obj));
    int wr_flexio = i80bus_flexio2_index(wr_pin_obj);
    if (wr_flexio < 0) {
        mp_raise_ValueError(MP_ERROR_TEXT("wr pin must map to FLEXIO2 on GPIO_B0/GPIO_B1"));
    }
    for (size_t i = 0; i < I80BUS_DATA_WIDTH; i++) {
        if (flexio_indices[i] == wr_flexio) {
            mp_raise_ValueError(MP_ERROR_TEXT("wr pin must not overlap data pins"));
        }
    }

    i80bus_obj_t *self = mp_obj_malloc(i80bus_obj_t, type);
    self->dc_pin = displayif_pin_resolve(vals[ARG_dc].u_obj);
    self->has_cs = vals[ARG_cs].u_obj != MP_OBJ_NULL;
    if (self->has_cs) {
        self->cs_pin = displayif_pin_resolve(vals[ARG_cs].u_obj);
        displayif_pin_set(self->cs_pin, 1);
    } else {
        self->cs_pin = mp_const_none;
    }
    displayif_pin_set(self->dc_pin, 0);
    self->initialized = false;
    self->deinited = false;

    for (size_t i = 0; i < I80BUS_DATA_WIDTH; i++) {
        i80bus_mux_flexio2(data_pin_objs[i]);
    }
    i80bus_mux_flexio2(wr_pin_obj);

    uint8_t data_start = (uint8_t)flexio_indices[0];
    uint8_t rd_flexio = i80bus_pick_rd_pin(data_start, (uint8_t)wr_flexio);

    self->lcd.flexioBase = FLEXIO2;
    self->lcd.busType = kFLEXIO_MCULCD_8080;
    self->lcd.dataPinStartIndex = data_start;
    self->lcd.ENWRPinIndex = (uint8_t)wr_flexio;
    self->lcd.RDPinIndex = rd_flexio;
    self->lcd.txShifterStartIndex = 0;
    self->lcd.txShifterEndIndex = 0;
    self->lcd.rxShifterStartIndex = 1;
    self->lcd.rxShifterEndIndex = 1;
    self->lcd.timerIndex = 0;
    self->lcd.setCSPin = i80bus_set_cspin;
    self->lcd.setRSPin = i80bus_set_rspin;
    self->lcd.setRDWRPin = NULL;

    flexio_mculcd_config_t config;
    FLEXIO_MCULCD_GetDefaultConfig(&config);
    config.baudRate_Bps = (uint32_t)vals[ARG_freq].u_int * I80BUS_DATA_WIDTH;

    s_callback_bus = self;
    uint32_t flexio_hz = BOARD_BOOTCLOCKRUN_FLEXIO2_CLK_ROOT;
    i80bus_raise_status(FLEXIO_MCULCD_Init(&self->lcd, &config, flexio_hz));

    s_shadow_lcd = self->lcd;
    self->initialized = true;
    s_flexio_in_use = true;
    return MP_OBJ_FROM_PTR(self);
}

static mp_obj_t i80bus_send(size_t n_args, const mp_obj_t *args) {
    i80bus_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    mp_obj_t command = (n_args > 1) ? args[1] : mp_const_none;
    mp_obj_t data = (n_args > 2) ? args[2] : mp_const_none;

    if (self->deinited || !self->initialized) {
        mp_raise_msg(&mp_type_OSError, MP_ERROR_TEXT("i80bus not initialized"));
    }

    s_callback_bus = self;

    if (self->has_cs) {
        FLEXIO_MCULCD_StartTransfer(&self->lcd);
    }

    if (command != mp_const_none) {
        uint8_t cmd = mp_obj_get_int(command);
        FLEXIO_MCULCD_WriteCommandBlocking(&self->lcd, cmd);
    }

    if (data != mp_const_none && data != mp_const_empty_bytes) {
        mp_buffer_info_t bufinfo;
        mp_get_buffer_raise(data, &bufinfo, MP_BUFFER_READ);
        if (bufinfo.len > 0) {
            if (bufinfo.len >= I80BUS_BULK_THRESHOLD) {
                flexio_mculcd_transfer_t xfer = {0};
                xfer.dataAddrOrSameValue = (uint32_t)(uintptr_t)bufinfo.buf;
                xfer.dataSize = bufinfo.len;
                xfer.mode = kFLEXIO_MCULCD_WriteArray;
                xfer.dataOnly = true;
                FLEXIO_MCULCD_TransferBlocking(&self->lcd, &xfer);
            } else {
                FLEXIO_MCULCD_WriteDataArrayBlocking(&self->lcd, bufinfo.buf, bufinfo.len);
            }
        }
    }

    if (self->has_cs) {
        FLEXIO_MCULCD_StopTransfer(&self->lcd);
    }

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(i80bus_send_obj, 1, 3, i80bus_send);

static mp_obj_t i80bus_deinit(mp_obj_t self_in) {
    i80bus_obj_t *self = MP_OBJ_TO_PTR(self_in);
    i80bus_deinit_internal(self);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(i80bus_deinit_obj, i80bus_deinit);

static const mp_rom_map_elem_t i80bus_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_send), MP_ROM_PTR(&i80bus_send_obj) },
    { MP_ROM_QSTR(MP_QSTR_deinit), MP_ROM_PTR(&i80bus_deinit_obj) },
    { MP_ROM_QSTR(MP_QSTR___del__), MP_ROM_PTR(&i80bus_deinit_obj) },
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

#else /* MIMXRT1062 */

#include "py/runtime.h"

static mp_obj_t i80bus_unsupported_make(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    (void)type;
    (void)n_args;
    (void)n_kw;
    (void)args;
    mp_raise_msg(&mp_type_NotImplementedError, MP_ERROR_TEXT("FlexIO i80bus not supported on this mimxrt SoC"));
}

static MP_DEFINE_CONST_OBJ_TYPE(
    i80bus_type,
    MP_QSTR_I80Bus,
    MP_TYPE_FLAG_NONE,
    make_new, i80bus_unsupported_make
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

#endif /* MIMXRT1062 */
