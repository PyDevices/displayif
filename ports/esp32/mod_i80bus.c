// SPDX-License-Identifier: MIT
// Intel 8080 parallel display bus for ESP32-S3 (pydisplay i80bus contract).

#include <string.h>

#include "py/runtime.h"
#include "py/obj.h"
#include "py/binary.h"
#include "displayif/mp_helpers.h"

#include "soc/soc_caps.h"

#if SOC_LCD_I80_SUPPORTED

#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_io_i80.h"

#define DC_CMD 0
#define DC_DATA 1

typedef struct _i80bus_obj_t {
    mp_obj_base_t base;
    esp_lcd_i80_bus_handle_t bus;
    esp_lcd_panel_io_handle_t io;
    bool has_cs;
    mp_obj_t buf1;
} i80bus_obj_t;

static const mp_obj_type_t i80bus_type;

static void i80bus_raise_esp_err(esp_err_t err) {
    if (err == ESP_OK) {
        return;
    }
    mp_raise_msg_varg(&mp_type_OSError, MP_ERROR_TEXT("ESP-IDF error %d (%s)"), err, esp_err_to_name(err));
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
        { MP_QSTR_dc, MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_INT, { .u_int = -1 } },
        { MP_QSTR_cs, MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_INT, { .u_int = -1 } },
        { MP_QSTR_wr, MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_INT, { .u_int = -1 } },
        { MP_QSTR_data, MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_OBJ, { .u_obj = MP_OBJ_NULL } },
        { MP_QSTR_freq, MP_ARG_KW_ONLY | MP_ARG_INT, { .u_int = 20000000 } },
    };
    mp_arg_val_t vals[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, args, MP_ARRAY_SIZE(allowed_args), allowed_args, vals);

    if (vals[ARG_dc].u_int < 0 || vals[ARG_wr].u_int < 0) {
        mp_raise_ValueError(MP_ERROR_TEXT("dc and wr pins must be specified"));
    }
    if (vals[ARG_freq].u_int <= 0) {
        mp_raise_ValueError(MP_ERROR_TEXT("freq must be positive"));
    }

    int data_pins[ESP_LCD_I80_BUS_WIDTH_MAX];
    size_t data_count = displayif_pin_seq_to_ints(vals[ARG_data].u_obj, data_pins, ESP_LCD_I80_BUS_WIDTH_MAX);
    if (data_count != 8 && data_count != 16) {
        mp_raise_ValueError(MP_ERROR_TEXT("data must be 8 or 16 pins"));
    }

    i80bus_obj_t *self = mp_obj_malloc(i80bus_obj_t, type);
    self->bus = NULL;
    self->io = NULL;
    self->has_cs = vals[ARG_cs].u_int >= 0;

    esp_lcd_i80_bus_config_t bus_config = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .dc_gpio_num = vals[ARG_dc].u_int,
        .wr_gpio_num = vals[ARG_wr].u_int,
        .bus_width = data_count,
        .max_transfer_bytes = 65536,
        .dma_burst_size = 64,
    };
    for (size_t i = 0; i < ESP_LCD_I80_BUS_WIDTH_MAX; i++) {
        bus_config.data_gpio_nums[i] = (i < data_count) ? data_pins[i] : -1;
    }

    i80bus_raise_esp_err(esp_lcd_new_i80_bus(&bus_config, &self->bus));

    esp_lcd_panel_io_i80_config_t io_config = {
        .cs_gpio_num = self->has_cs ? vals[ARG_cs].u_int : -1,
        .pclk_hz = vals[ARG_freq].u_int,
        .trans_queue_depth = 10,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .dc_levels = {
            .dc_idle_level = DC_CMD,
            .dc_cmd_level = DC_CMD,
            .dc_dummy_level = DC_CMD,
            .dc_data_level = DC_DATA,
        },
    };

    i80bus_raise_esp_err(esp_lcd_new_panel_io_i80(self->bus, &io_config, &self->io));
    self->buf1 = mp_obj_new_bytearray(1, (byte[]){0});
    return MP_OBJ_FROM_PTR(self);
}

static mp_obj_t i80bus_send(size_t n_args, const mp_obj_t *args) {
    i80bus_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    mp_obj_t command = (n_args > 1) ? args[1] : mp_const_none;
    mp_obj_t data = (n_args > 2) ? args[2] : mp_const_none;

    if (command != mp_const_none) {
        uint8_t cmd = mp_obj_get_int(command);
        i80bus_raise_esp_err(esp_lcd_panel_io_tx_param(self->io, cmd, NULL, 0));
    }

    if (data != mp_const_none && data != mp_const_empty_bytes) {
        mp_buffer_info_t bufinfo;
        mp_get_buffer_raise(data, &bufinfo, MP_BUFFER_READ);
        if (bufinfo.len > 0) {
            i80bus_raise_esp_err(esp_lcd_panel_io_tx_color(self->io, -1, bufinfo.buf, bufinfo.len));
        }
    }

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(i80bus_send_obj, 1, 3, i80bus_send);

static mp_obj_t i80bus_deinit(mp_obj_t self_in) {
    i80bus_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (self->io != NULL) {
        esp_lcd_panel_io_del(self->io);
        self->io = NULL;
    }
    if (self->bus != NULL) {
        esp_lcd_del_i80_bus(self->bus);
        self->bus = NULL;
    }
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

#endif /* SOC_LCD_I80_SUPPORTED */
