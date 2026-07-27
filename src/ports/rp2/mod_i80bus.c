// SPDX-License-Identifier: MIT
// Intel 8080 parallel display bus for RP2040/RP2350 (PIO + DMA via pico-sdk).
// Lifecycle: idempotent deinit/__del__/ctor + soft-reset teardown (see soft_reset.h).

#include <string.h>

#include "py/runtime.h"
#include "py/obj.h"
#include "py/binary.h"
#include "py/mphal.h"
#include "displayif/mp_helpers.h"
#include "displayif/soft_reset.h"
#include "machine_pin.h"

#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/pio_instructions.h"

#define I80BUS_DATA_WIDTH 8
#define I80BUS_CYCLES_PER_BYTE 2
#define I80BUS_DMA_THRESHOLD 4

typedef struct _i80bus_obj_t {
    mp_obj_base_t base;
    PIO pio;
    int sm;
    uint prog_offset;
    int dma_channel;
    uint8_t dc_pin;
    uint8_t cs_pin;
    bool has_cs;
    uint8_t wr_pin;
    uint8_t data_base;
    uint32_t freq;
    bool has_reset;
    uint8_t reset_pin;
    bool deinited;
} i80bus_obj_t;

typedef struct {
    PIO pio;
    int sm;
    int dma_channel;
    bool live;
} i80bus_host_t;

static i80bus_host_t s_host = {
    .sm = -1,
    .dma_channel = -1,
};
static bool s_soft_reset_registered;

static const mp_obj_type_t i80bus_type;

static uint16_t i80bus_program_data[2];
static const struct pio_program i80bus_program = {
    .instructions = i80bus_program_data,
    .length = 2,
    .origin = -1,
};
static bool i80bus_program_ready;
static bool i80bus_offsets_ready;
#if NUM_PIOS > 0
static int i80bus_prog_offset[NUM_PIOS];
#endif

static void i80bus_host_teardown(void) {
    if (s_host.sm >= 0) {
        pio_sm_set_enabled(s_host.pio, s_host.sm, false);
        pio_sm_unclaim(s_host.pio, s_host.sm);
        s_host.sm = -1;
    }
    if (s_host.dma_channel >= 0) {
        dma_channel_abort(s_host.dma_channel);
        dma_channel_unclaim(s_host.dma_channel);
        s_host.dma_channel = -1;
    }
    s_host.live = false;
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
        self->sm = -1;
        self->dma_channel = -1;
        self->deinited = true;
    }
}

static void i80bus_prepare_program(void) {
    if (!i80bus_offsets_ready) {
        for (uint i = 0; i < NUM_PIOS; i++) {
            i80bus_prog_offset[i] = -1;
        }
        i80bus_offsets_ready = true;
    }
    if (i80bus_program_ready) {
        return;
    }
    i80bus_program_data[0] = (uint16_t)(pio_encode_pull(false, true) | pio_encode_sideset_opt(1, 0));
    i80bus_program_data[1] = (uint16_t)(pio_encode_out(pio_pins, 8) | pio_encode_sideset_opt(1, 1));
    i80bus_program_ready = true;
}

static bool i80bus_arg_is_set(mp_obj_t obj) {
    return obj != MP_OBJ_NULL && obj != mp_const_none;
}

static int i80bus_pin_num(mp_obj_t pin_or_int) {
    if (mp_obj_is_small_int(pin_or_int) || mp_obj_is_int(pin_or_int)) {
        return displayif_pin_id(pin_or_int);
    }
    const machine_pin_obj_t *pin = machine_pin_find(pin_or_int);
    return pin->id;
}

static void i80bus_gpio_out(uint8_t pin, bool value) {
    gpio_put(pin, value);
    gpio_set_dir(pin, GPIO_OUT);
}

static void i80bus_wait_tx_done(i80bus_obj_t *self) {
    while (!pio_sm_is_tx_fifo_empty(self->pio, self->sm)) {
        tight_loop_contents();
    }
}

static void i80bus_transfer(i80bus_obj_t *self, const uint8_t *data, size_t len) {
    if (len == 0) {
        return;
    }

    pio_sm_set_enabled(self->pio, self->sm, false);
    pio_sm_clear_fifos(self->pio, self->sm);
    pio_sm_set_enabled(self->pio, self->sm, true);

    if (len < I80BUS_DMA_THRESHOLD || self->dma_channel < 0) {
        for (size_t i = 0; i < len; i++) {
            pio_sm_put_blocking(self->pio, self->sm, data[i]);
        }
        i80bus_wait_tx_done(self);
        return;
    }

    dma_channel_config cfg = dma_channel_get_default_config(self->dma_channel);
    channel_config_set_read_increment(&cfg, true);
    channel_config_set_write_increment(&cfg, false);
    channel_config_set_dreq(&cfg, pio_get_dreq(self->pio, self->sm, true));
    dma_channel_configure(
        self->dma_channel,
        &cfg,
        &self->pio->txf[self->sm],
        data,
        len,
        true
    );
    dma_channel_wait_for_finish_blocking(self->dma_channel);
    i80bus_wait_tx_done(self);
}

static mp_obj_t i80bus_make(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    enum {
        ARG_data0,
        ARG_data_pins,
        ARG_command,
        ARG_chip_select,
        ARG_write,
        ARG_read,
        ARG_reset,
        ARG_frequency,
    };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_data0, MP_ARG_KW_ONLY | MP_ARG_OBJ, { .u_obj = mp_const_none } },
        { MP_QSTR_data_pins, MP_ARG_KW_ONLY | MP_ARG_OBJ, { .u_obj = mp_const_none } },
        { MP_QSTR_command, MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_OBJ, { .u_obj = MP_OBJ_NULL } },
        { MP_QSTR_chip_select, MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_OBJ, { .u_obj = MP_OBJ_NULL } },
        { MP_QSTR_write, MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_OBJ, { .u_obj = MP_OBJ_NULL } },
        { MP_QSTR_read, MP_ARG_KW_ONLY | MP_ARG_OBJ, { .u_obj = mp_const_none } },
        { MP_QSTR_reset, MP_ARG_KW_ONLY | MP_ARG_OBJ, { .u_obj = mp_const_none } },
        { MP_QSTR_frequency, MP_ARG_KW_ONLY | MP_ARG_INT, { .u_int = 30000000 } },
    };
    mp_arg_val_t vals[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, args, MP_ARRAY_SIZE(allowed_args), allowed_args, vals);

    if (!i80bus_arg_is_set(vals[ARG_command].u_obj) || !i80bus_arg_is_set(vals[ARG_write].u_obj)) {
        mp_raise_ValueError(MP_ERROR_TEXT("command and write pins must be specified"));
    }
    if (vals[ARG_frequency].u_int <= 0) {
        mp_raise_ValueError(MP_ERROR_TEXT("frequency must be positive"));
    }

    bool has_data0 = i80bus_arg_is_set(vals[ARG_data0].u_obj);
    bool has_data_pins = i80bus_arg_is_set(vals[ARG_data_pins].u_obj);
    if (has_data0 == has_data_pins) {
        mp_raise_ValueError(MP_ERROR_TEXT("Specify exactly one of data0 or data_pins"));
    }

    int data_pins[I80BUS_DATA_WIDTH];
    size_t data_count;
    if (has_data0) {
        /* CircuitPython ParallelBus(data0=): eight consecutive GPIOs from data0. */
        int base = i80bus_pin_num(vals[ARG_data0].u_obj);
        data_count = I80BUS_DATA_WIDTH;
        for (size_t i = 0; i < data_count; i++) {
            data_pins[i] = base + (int)i;
        }
    } else {
        data_count = displayif_pin_seq_to_ints(vals[ARG_data_pins].u_obj, data_pins, I80BUS_DATA_WIDTH);
        if (data_count != I80BUS_DATA_WIDTH) {
            mp_raise_ValueError(MP_ERROR_TEXT("data_pins must be 8 consecutive pins"));
        }
        for (size_t i = 1; i < data_count; i++) {
            if (data_pins[i] != data_pins[i - 1] + 1) {
                mp_raise_ValueError(MP_ERROR_TEXT("data_pins must be consecutive"));
            }
        }
    }

    /* read is accepted for CP ParallelBus signature parity (unused on write path). */
    (void)vals[ARG_read];

    uint8_t dc_pin = i80bus_pin_num(vals[ARG_command].u_obj);
    uint8_t wr_pin = i80bus_pin_num(vals[ARG_write].u_obj);
    uint8_t cs_pin = 0;
    bool has_cs = i80bus_arg_is_set(vals[ARG_chip_select].u_obj);
    if (has_cs) {
        cs_pin = i80bus_pin_num(vals[ARG_chip_select].u_obj);
    }

    uint8_t reset_pin = 0;
    bool has_reset = i80bus_arg_is_set(vals[ARG_reset].u_obj);
    if (has_reset) {
        reset_pin = i80bus_pin_num(vals[ARG_reset].u_obj);
        i80bus_gpio_out(reset_pin, true);
    }

    i80bus_host_teardown();
    i80bus_ensure_soft_reset_registered();
    i80bus_prepare_program();

    PIO pio = pio1;
    int sm = pio_claim_unused_sm(pio, true);
    if (sm < 0) {
        pio = pio0;
        sm = pio_claim_unused_sm(pio, true);
        if (sm < 0) {
            mp_raise_msg(&mp_type_OSError, MP_ERROR_TEXT("no PIO state machine available"));
        }
    }

    uint pio_idx = pio_get_index(pio);
    int prog_offset = i80bus_prog_offset[pio_idx];
    if (prog_offset < 0) {
        prog_offset = pio_add_program(pio, &i80bus_program);
        if (prog_offset < 0) {
            pio_sm_unclaim(pio, sm);
            mp_raise_msg(&mp_type_OSError, MP_ERROR_TEXT("failed to load PIO program"));
        }
        i80bus_prog_offset[pio_idx] = prog_offset;
    }

    int dma_channel = dma_claim_unused_channel(true);
    if (dma_channel < 0) {
        pio_sm_unclaim(pio, sm);
        mp_raise_msg(&mp_type_OSError, MP_ERROR_TEXT("no DMA channel available"));
    }

    uint8_t data_base = data_pins[0];
    for (size_t i = 0; i < data_count; i++) {
        pio_gpio_init(pio, data_pins[i]);
    }
    pio_gpio_init(pio, wr_pin);

    pio_sm_config config = pio_get_default_sm_config();
    sm_config_set_wrap(&config, prog_offset, prog_offset + i80bus_program.length - 1);
    sm_config_set_set_pins(&config, data_base, I80BUS_DATA_WIDTH);
    sm_config_set_out_pins(&config, data_base, I80BUS_DATA_WIDTH);
    sm_config_set_sideset_pins(&config, wr_pin);
    sm_config_set_sideset(&config, 1, true, false);
    sm_config_set_out_shift(&config, false, false, 0);
    sm_config_set_fifo_join(&config, PIO_FIFO_JOIN_TX);

    float div = (float)clock_get_hz(clk_sys) / ((float)vals[ARG_frequency].u_int * I80BUS_CYCLES_PER_BYTE);
    if (div < 1.0f) {
        div = 1.0f;
    }
    sm_config_set_clkdiv(&config, div);

    pio_sm_set_consecutive_pindirs(pio, sm, data_base, I80BUS_DATA_WIDTH, true);
    gpio_set_dir(wr_pin, GPIO_OUT);
    gpio_put(wr_pin, 0);

    pio_sm_init(pio, sm, prog_offset, &config);
    pio_sm_set_enabled(pio, sm, true);

    i80bus_gpio_out(dc_pin, false);
    if (has_cs) {
        i80bus_gpio_out(cs_pin, true);
    }

    i80bus_obj_t *self = mp_obj_malloc(i80bus_obj_t, type);
    self->pio = pio;
    self->sm = sm;
    self->prog_offset = prog_offset;
    self->dma_channel = dma_channel;
    self->dc_pin = dc_pin;
    self->cs_pin = cs_pin;
    self->has_cs = has_cs;
    self->wr_pin = wr_pin;
    self->data_base = data_base;
    self->freq = vals[ARG_frequency].u_int;
    self->has_reset = has_reset;
    self->reset_pin = reset_pin;
    self->deinited = false;
    s_host.pio = pio;
    s_host.sm = sm;
    s_host.dma_channel = dma_channel;
    s_host.live = true;
    return MP_OBJ_FROM_PTR(self);
}

static mp_obj_t i80bus_send(size_t n_args, const mp_obj_t *args) {
    i80bus_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    if (self->deinited || self->sm < 0) {
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("i80bus is deinited"));
    }
    mp_obj_t command = (n_args > 1) ? args[1] : mp_const_none;
    mp_obj_t data = (n_args > 2) ? args[2] : mp_const_none;

    if (self->has_cs) {
        gpio_put(self->cs_pin, 0);
    }

    if (command != mp_const_none) {
        uint8_t cmd = mp_obj_get_int(command);
        gpio_put(self->dc_pin, 0);
        i80bus_transfer(self, &cmd, 1);
    }

    if (data != mp_const_none && data != mp_const_empty_bytes) {
        mp_buffer_info_t bufinfo;
        mp_get_buffer_raise(data, &bufinfo, MP_BUFFER_READ);
        if (bufinfo.len > 0) {
            gpio_put(self->dc_pin, 1);
            i80bus_transfer(self, bufinfo.buf, bufinfo.len);
        }
    }

    if (self->has_cs) {
        gpio_put(self->cs_pin, 1);
    }

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(i80bus_send_obj, 1, 3, i80bus_send);

static mp_obj_t i80bus_reset(mp_obj_t self_in) {
    i80bus_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (self->deinited) {
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("i80bus is deinited"));
    }
    if (!self->has_reset) {
        mp_raise_msg_varg(&mp_type_RuntimeError, MP_ERROR_TEXT("No %q pin"), MP_QSTR_reset);
    }
    gpio_put(self->reset_pin, 0);
    mp_hal_delay_us(4);
    gpio_put(self->reset_pin, 1);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(i80bus_reset_obj, i80bus_reset);

static mp_obj_t i80bus_deinit(mp_obj_t self_in) {
    i80bus_obj_t *self = MP_OBJ_TO_PTR(self_in);
    i80bus_deinit_internal(self);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(i80bus_deinit_obj, i80bus_deinit);

static const mp_rom_map_elem_t i80bus_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_reset), MP_ROM_PTR(&i80bus_reset_obj) },
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
