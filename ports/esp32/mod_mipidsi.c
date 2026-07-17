// SPDX-License-Identifier: MIT
// MIPI DSI host for ESP32-P4 (FBDisplay / pydisplay mipidsi contract).

#include <string.h>
#include <stdlib.h>

#include "py/runtime.h"
#include "py/obj.h"
#include "py/binary.h"
#include "py/mphal.h"
#include "displayif/mp_helpers.h"

#include "sdkconfig.h"

#if SOC_MIPI_DSI_SUPPORTED

#include "esp_err.h"
#include "esp_log.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_ldo_regulator.h"
#include "esp_cache.h"
#include "esp_heap_caps.h"
#include "driver/gpio.h"

#define MIPIDSI_INIT_DELAY_FLAG 0x80

#define MIPIDSI_LDO_CHAN_DEFAULT 3
#define MIPIDSI_LDO_VOLTAGE_MV_DEFAULT 2500

typedef struct _mipidsi_bus_obj_t {
    mp_obj_base_t base;
    esp_lcd_dsi_bus_handle_t dsi_bus;
    esp_ldo_channel_handle_t ldo_phy;
    uint8_t num_lanes;
    uint32_t lane_bit_rate_mbps;
} mipidsi_bus_obj_t;

typedef struct _mipidsi_display_obj_t {
    mp_obj_base_t base;
    mipidsi_bus_obj_t *bus;
    esp_lcd_panel_io_handle_t io;
    esp_lcd_panel_handle_t panel;
    uint16_t width;
    uint16_t height;
    uint16_t row_stride;
    uint8_t *buf;
    size_t buf_len;
    int backlight_pin;
    bool backlight_on_high;
} mipidsi_display_obj_t;

static const mp_obj_type_t mipidsi_bus_type;
static const mp_obj_type_t mipidsi_display_type;

static void mipidsi_raise_esp_err(esp_err_t err) {
    if (err == ESP_OK) {
        return;
    }
    mp_raise_msg_varg(&mp_type_OSError, MP_ERROR_TEXT("ESP-IDF error %d (%s)"), err, esp_err_to_name(err));
}

static void mipidsi_enable_phy_power(mipidsi_bus_obj_t *bus, int ldo_chan, int ldo_voltage_mv) {
    if (bus->ldo_phy != NULL) {
        return;
    }
    esp_ldo_channel_config_t ldo_config = {
        .chan_id = ldo_chan,
        .voltage_mv = ldo_voltage_mv,
    };
    mipidsi_raise_esp_err(esp_ldo_acquire_channel(&ldo_config, &bus->ldo_phy));
}

static void mipidsi_send_init_sequence(esp_lcd_panel_io_handle_t io, const uint8_t *init_sequence, size_t init_len) {
    size_t i = 0;
    while (i < init_len) {
        if (i + 1 >= init_len) {
            mp_raise_ValueError(MP_ERROR_TEXT("truncated display init sequence"));
        }
        int command = init_sequence[i];
        int data_size = init_sequence[i + 1];
        bool delay = (data_size & MIPIDSI_INIT_DELAY_FLAG) != 0;
        data_size &= ~MIPIDSI_INIT_DELAY_FLAG;

        if (i + 2 + (size_t)data_size > init_len) {
            mp_raise_ValueError(MP_ERROR_TEXT("invalid display init sequence"));
        }

        const void *params = (data_size > 0) ? &init_sequence[i + 2] : NULL;
        mipidsi_raise_esp_err(esp_lcd_panel_io_tx_param(io, command, params, data_size));

        uint32_t delay_time_ms = 10;
        if (delay) {
            if (i + 2 + (size_t)data_size >= init_len) {
                mp_raise_ValueError(MP_ERROR_TEXT("init sequence delay byte missing"));
            }
            delay_time_ms = init_sequence[i + 2 + data_size];
            if (delay_time_ms == 255) {
                delay_time_ms = 500;
            }
            data_size += 1;
        }

        mp_hal_delay_ms(delay_time_ms);
        i += 2 + (size_t)data_size;
    }
}

static void mipidsi_gpio_reset(int reset_pin) {
    if (reset_pin < 0) {
        return;
    }
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << reset_pin,
        .mode = GPIO_MODE_OUTPUT,
    };
    mipidsi_raise_esp_err(gpio_config(&cfg));
    gpio_set_level(reset_pin, 0);
    mp_hal_delay_ms(10);
    gpio_set_level(reset_pin, 1);
    mp_hal_delay_ms(200);
}

static void mipidsi_backlight_set(int pin, bool on_high, bool on) {
    if (pin < 0) {
        return;
    }
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << pin,
        .mode = GPIO_MODE_OUTPUT,
    };
    mipidsi_raise_esp_err(gpio_config(&cfg));
    int level = on ? (on_high ? 1 : 0) : (on_high ? 0 : 1);
    gpio_set_level(pin, level);
}

#define MIPIDSI_CACHE_LINE 64

static size_t mipidsi_align_up(size_t nbytes) {
    return (nbytes + MIPIDSI_CACHE_LINE - 1) & ~(size_t)(MIPIDSI_CACHE_LINE - 1);
}

static uint8_t *mipidsi_alloc_framebuffer(size_t nbytes) {
    size_t alloc_size = mipidsi_align_up(nbytes);
    uint8_t *ptr = heap_caps_aligned_alloc(
        MIPIDSI_CACHE_LINE, alloc_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (ptr == NULL) {
        ptr = heap_caps_aligned_alloc(
            MIPIDSI_CACHE_LINE, alloc_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    return ptr;
}

static void mipidsi_free_framebuffer(uint8_t *ptr) {
    if (ptr != NULL) {
        heap_caps_free(ptr);
    }
}

static mp_obj_t mipidsi_bus_make(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    enum {
        ARG_frequency,
        ARG_num_lanes,
        ARG_ldo_chan,
        ARG_ldo_voltage_mv,
    };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_frequency, MP_ARG_REQUIRED | MP_ARG_KW_ONLY | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_num_lanes, MP_ARG_KW_ONLY | MP_ARG_INT, { .u_int = 2 } },
        { MP_QSTR_ldo_chan, MP_ARG_KW_ONLY | MP_ARG_INT, { .u_int = MIPIDSI_LDO_CHAN_DEFAULT } },
        { MP_QSTR_ldo_voltage_mv, MP_ARG_KW_ONLY | MP_ARG_INT, { .u_int = MIPIDSI_LDO_VOLTAGE_MV_DEFAULT } },
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
    self->dsi_bus = NULL;
    self->ldo_phy = NULL;
    self->num_lanes = vals[ARG_num_lanes].u_int;
    self->lane_bit_rate_mbps = (uint32_t)(vals[ARG_frequency].u_int / 1000000);
    if (self->lane_bit_rate_mbps == 0) {
        self->lane_bit_rate_mbps = 1;
    }

    mipidsi_enable_phy_power(self, vals[ARG_ldo_chan].u_int, vals[ARG_ldo_voltage_mv].u_int);

    esp_lcd_dsi_bus_config_t bus_config = {
        .bus_id = 0,
        .num_data_lanes = self->num_lanes,
        .lane_bit_rate_mbps = self->lane_bit_rate_mbps,
    };
    mipidsi_raise_esp_err(esp_lcd_new_dsi_bus(&bus_config, &self->dsi_bus));

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
    if (bus->dsi_bus == NULL) {
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

    mipidsi_gpio_reset(vals[ARG_reset_pin].u_int);

    esp_lcd_dbi_io_config_t dbi_config = {
        .virtual_channel = 0,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    esp_lcd_panel_io_handle_t io = NULL;
    mipidsi_raise_esp_err(esp_lcd_new_panel_io_dbi(bus->dsi_bus, &dbi_config, &io));

    mipidsi_send_init_sequence(io, init_bufinfo.buf, init_bufinfo.len);

    uint32_t dpi_clock_mhz = (uint32_t)(vals[ARG_pixel_clock_frequency].u_int / 1000000);
    if (dpi_clock_mhz == 0) {
        dpi_clock_mhz = 1;
    }

    esp_lcd_dpi_panel_config_t dpi_config = {
        .virtual_channel = 0,
        .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
        .dpi_clock_freq_mhz = dpi_clock_mhz,
        .in_color_format = LCD_COLOR_FMT_RGB565,
        .out_color_format = LCD_COLOR_FMT_RGB565,
        .video_timing = {
            .h_size = vals[ARG_width].u_int,
            .v_size = vals[ARG_height].u_int,
            .hsync_pulse_width = vals[ARG_hsync_pulse_width].u_int,
            .hsync_front_porch = vals[ARG_hsync_front_porch].u_int,
            .hsync_back_porch = vals[ARG_hsync_back_porch].u_int,
            .vsync_pulse_width = vals[ARG_vsync_pulse_width].u_int,
            .vsync_front_porch = vals[ARG_vsync_front_porch].u_int,
            .vsync_back_porch = vals[ARG_vsync_back_porch].u_int,
        },
    };

    esp_lcd_panel_handle_t panel = NULL;
    mipidsi_raise_esp_err(esp_lcd_new_panel_dpi(bus->dsi_bus, &dpi_config, &panel));
    mipidsi_raise_esp_err(esp_lcd_panel_init(panel));

    mipidsi_display_obj_t *self = mp_obj_malloc(mipidsi_display_obj_t, type);
    self->bus = bus;
    self->io = io;
    self->panel = panel;
    self->width = vals[ARG_width].u_int;
    self->height = vals[ARG_height].u_int;
    self->row_stride = self->width * sizeof(uint16_t);
    self->buf_len = (size_t)self->row_stride * self->height;
    self->buf = mipidsi_alloc_framebuffer(self->buf_len);
    if (self->buf == NULL) {
        mp_raise_msg(&mp_type_MemoryError, MP_ERROR_TEXT("MIPI DSI framebuffer allocation failed"));
    }
    memset(self->buf, 0, self->buf_len);
    self->backlight_pin = vals[ARG_backlight_pin].u_int;
    self->backlight_on_high = vals[ARG_backlight_on_high].u_bool;
    mipidsi_backlight_set(self->backlight_pin, self->backlight_on_high, true);

    return MP_OBJ_FROM_PTR(self);
}

static mp_obj_t mipidsi_display_refresh(mp_obj_t self_in) {
    mipidsi_display_obj_t *self = MP_OBJ_TO_PTR(self_in);
    /* Address and size must be cache-line aligned (see mipidsi_alloc_framebuffer). */
    mipidsi_raise_esp_err(esp_cache_msync(
        self->buf, mipidsi_align_up(self->buf_len), ESP_CACHE_MSYNC_FLAG_DIR_C2M));
    mipidsi_raise_esp_err(esp_lcd_panel_draw_bitmap(self->panel, 0, 0, self->width, self->height, self->buf));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mipidsi_display_refresh_obj, mipidsi_display_refresh);

static mp_int_t mipidsi_display_get_buffer(mp_obj_t self_in, mp_buffer_info_t *bufinfo, mp_uint_t flags) {
    (void)flags;
    mipidsi_display_obj_t *self = MP_OBJ_TO_PTR(self_in);
    bufinfo->buf = self->buf;
    bufinfo->len = self->buf_len;
    /* Byte-addressable for displaysys.FBDisplay (RGB565 packed). */
    bufinfo->typecode = 'B';
    return 0;
}

static mp_obj_t mipidsi_display_del(mp_obj_t self_in) {
    mipidsi_display_obj_t *self = MP_OBJ_TO_PTR(self_in);
    mipidsi_backlight_set(self->backlight_pin, self->backlight_on_high, false);
    if (self->panel != NULL) {
        esp_lcd_panel_del(self->panel);
        self->panel = NULL;
    }
    if (self->io != NULL) {
        esp_lcd_panel_io_del(self->io);
        self->io = NULL;
    }
    mipidsi_free_framebuffer(self->buf);
    self->buf = NULL;
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mipidsi_display_del_obj, mipidsi_display_del);

static void mipidsi_display_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    mipidsi_display_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (dest[0] == MP_OBJ_NULL) {
        /* Custom attr replaces locals_dict lookup on this port — expose methods here. */
        if (attr == MP_QSTR_width) {
            dest[0] = mp_obj_new_int(self->width);
        } else if (attr == MP_QSTR_height) {
            dest[0] = mp_obj_new_int(self->height);
        } else if (attr == MP_QSTR_row_stride) {
            dest[0] = mp_obj_new_int(self->row_stride);
        } else if (attr == MP_QSTR_refresh) {
            dest[0] = MP_OBJ_FROM_PTR(&mipidsi_display_refresh_obj);
            dest[1] = self_in;
        } else if (attr == MP_QSTR___del__) {
            dest[0] = MP_OBJ_FROM_PTR(&mipidsi_display_del_obj);
            dest[1] = self_in;
        }
    }
}

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

#else /* SOC_MIPI_DSI_SUPPORTED */

#include "py/runtime.h"

static mp_obj_t mipidsi_unsupported_make(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    (void)type;
    (void)n_args;
    (void)n_kw;
    (void)args;
    mp_raise_msg(&mp_type_NotImplementedError, MP_ERROR_TEXT("MIPI DSI not supported on this SoC"));
}

static MP_DEFINE_CONST_OBJ_TYPE(
    mipidsi_bus_type,
    MP_QSTR_Bus,
    MP_TYPE_FLAG_NONE,
    make_new, mipidsi_unsupported_make
);

static MP_DEFINE_CONST_OBJ_TYPE(
    mipidsi_display_type,
    MP_QSTR_Display,
    MP_TYPE_FLAG_NONE,
    make_new, mipidsi_unsupported_make
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

#endif /* SOC_MIPI_DSI_SUPPORTED */
