// SPDX-License-Identifier: MIT
// Dot-clock RGB framebuffer for esp32 port (ESP-IDF esp_lcd RGB panel).

#include <string.h>
#include <stdlib.h>

#include "py/runtime.h"
#include "py/obj.h"
#include "py/binary.h"
#include "displayif/mp_helpers.h"

#define RGBFB_MAX_DATA_PINS 18

#if defined(ESP_PLATFORM)
#include "esp_heap_caps.h"
#include "esp_err.h"
#include "soc/soc_caps.h"
#if SOC_LCD_RGB_SUPPORTED
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_cache.h"
#endif
#endif

#include "displayif_esp32_pins.h"

typedef struct _rgbframebuffer_obj_t {
    mp_obj_base_t base;
    uint16_t width;
    uint16_t height;
    uint16_t row_stride;
    uint8_t *buf;
    size_t buf_len;
    int de_pin;
    int vsync_pin;
    int hsync_pin;
    int dclk_pin;
    int data_pins[RGBFB_MAX_DATA_PINS];
    size_t data_pin_count;
    uint32_t frequency;
    uint16_t hsync_pulse_width;
    uint16_t hsync_front_porch;
    uint16_t hsync_back_porch;
    uint16_t vsync_pulse_width;
    uint16_t vsync_front_porch;
    uint16_t vsync_back_porch;
    bool hsync_idle_low;
    bool vsync_idle_low;
    bool de_idle_high;
    bool pclk_active_high;
    bool pclk_idle_high;
    int overscan_left;
    uint8_t bits_per_pixel;
    bool rgb666_layout;
#if defined(ESP_PLATFORM) && SOC_LCD_RGB_SUPPORTED
    esp_lcd_panel_handle_t panel;
    bool panel_ready;
#endif
} rgbframebuffer_obj_t;

static const mp_obj_type_t rgbframebuffer_type;

#if defined(ESP_PLATFORM)
static uint8_t *rgbframebuffer_alloc(size_t nbytes) {
    uint8_t *ptr = heap_caps_malloc(nbytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (ptr == NULL) {
        ptr = heap_caps_malloc(nbytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    return ptr;
}

static void rgbframebuffer_free(uint8_t *ptr) {
    if (ptr != NULL) {
        heap_caps_free(ptr);
    }
}
#else
static uint8_t *rgbframebuffer_alloc(size_t nbytes) {
    return malloc(nbytes);
}

static void rgbframebuffer_free(uint8_t *ptr) {
    free(ptr);
}
#endif

#if defined(ESP_PLATFORM) && SOC_LCD_RGB_SUPPORTED
static void rgbframebuffer_raise_esp_err(esp_err_t err) {
    if (err == ESP_OK) {
        return;
    }
    mp_raise_msg_varg(&mp_type_OSError, MP_ERROR_TEXT("ESP-IDF error %d (%s)"), err, esp_err_to_name(err));
}

static void rgbframebuffer_fill_data_pins(rgbframebuffer_obj_t *self, mp_obj_t red, mp_obj_t green, mp_obj_t blue, mp_obj_t data) {
    int pins[RGBFB_MAX_DATA_PINS];
    size_t count = 0;

    if (data != MP_OBJ_NULL) {
        count = displayif_esp32_pin_seq_to_gpios(data, pins, RGBFB_MAX_DATA_PINS);
    } else {
        size_t n_red = displayif_esp32_pin_seq_to_gpios(red, pins, RGBFB_MAX_DATA_PINS);
        size_t n_green = displayif_esp32_pin_seq_to_gpios(green, pins + n_red, RGBFB_MAX_DATA_PINS - n_red);
        size_t n_blue = displayif_esp32_pin_seq_to_gpios(blue, pins + n_red + n_green,
            RGBFB_MAX_DATA_PINS - n_red - n_green);
        count = n_red + n_green + n_blue;
    }

    if (count == 0 || count > RGBFB_MAX_DATA_PINS) {
        mp_raise_ValueError(MP_ERROR_TEXT("invalid RGB data pin layout"));
    }

    self->data_pin_count = count;
    for (size_t i = 0; i < RGBFB_MAX_DATA_PINS; i++) {
        self->data_pins[i] = (i < count) ? pins[i] : -1;
    }
}

static void rgbframebuffer_init_panel(rgbframebuffer_obj_t *self) {
    if (self->panel_ready) {
        return;
    }

    esp_lcd_rgb_panel_config_t panel_config = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .data_width = self->data_pin_count,
        .bits_per_pixel = self->bits_per_pixel,
        .num_fbs = 1,
        .dma_burst_size = 64,
        .hsync_gpio_num = self->hsync_pin,
        .vsync_gpio_num = self->vsync_pin,
        .de_gpio_num = self->de_pin,
        .pclk_gpio_num = self->dclk_pin,
        .disp_gpio_num = -1,
        .timings = {
            .pclk_hz = self->frequency,
            .h_res = self->width,
            .v_res = self->height,
            .hsync_pulse_width = self->hsync_pulse_width,
            .hsync_front_porch = self->hsync_front_porch,
            .hsync_back_porch = self->hsync_back_porch,
            .vsync_pulse_width = self->vsync_pulse_width,
            .vsync_front_porch = self->vsync_front_porch,
            .vsync_back_porch = self->vsync_back_porch,
            .flags = {
                .hsync_idle_low = self->hsync_idle_low,
                .vsync_idle_low = self->vsync_idle_low,
                .de_idle_high = self->de_idle_high,
                .pclk_active_neg = !self->pclk_active_high,
                .pclk_idle_high = self->pclk_idle_high,
            },
        },
        .flags = {
            .refresh_on_demand = 1,
            .fb_in_psram = 1,
        },
    };

    memcpy(panel_config.data_gpio_nums, self->data_pins, sizeof(self->data_pins));

    esp_lcd_panel_handle_t panel = NULL;
    rgbframebuffer_raise_esp_err(esp_lcd_new_rgb_panel(&panel_config, &panel));
    rgbframebuffer_raise_esp_err(esp_lcd_panel_reset(panel));
    rgbframebuffer_raise_esp_err(esp_lcd_panel_init(panel));

    if (self->overscan_left != 0) {
        rgbframebuffer_raise_esp_err(esp_lcd_panel_set_gap(panel, self->overscan_left, 0));
    }

    self->panel = panel;
    self->panel_ready = true;
}
#endif

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
    if (rgb666 && (vals[ARG_green].u_obj == MP_OBJ_NULL || vals[ARG_blue].u_obj == MP_OBJ_NULL)) {
        mp_raise_ValueError(MP_ERROR_TEXT("red/green/blue pin tuples required together"));
    }
    if (vals[ARG_width].u_int <= 0 || vals[ARG_height].u_int <= 0) {
        mp_raise_ValueError(MP_ERROR_TEXT("width and height must be positive"));
    }
    if (vals[ARG_frequency].u_int <= 0) {
        mp_raise_ValueError(MP_ERROR_TEXT("frequency must be positive"));
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

    self->de_pin = displayif_esp32_pin_gpio(vals[ARG_de].u_obj);
    self->vsync_pin = displayif_esp32_pin_gpio(vals[ARG_vsync].u_obj);
    self->hsync_pin = displayif_esp32_pin_gpio(vals[ARG_hsync].u_obj);
    self->dclk_pin = displayif_esp32_pin_gpio(vals[ARG_dclk].u_obj);
    self->frequency = vals[ARG_frequency].u_int;
    self->hsync_pulse_width = vals[ARG_hsync_pulse_width].u_int;
    self->hsync_front_porch = vals[ARG_hsync_front_porch].u_int;
    self->hsync_back_porch = vals[ARG_hsync_back_porch].u_int;
    self->vsync_pulse_width = vals[ARG_vsync_pulse_width].u_int;
    self->vsync_front_porch = vals[ARG_vsync_front_porch].u_int;
    self->vsync_back_porch = vals[ARG_vsync_back_porch].u_int;
    self->hsync_idle_low = vals[ARG_hsync_idle_low].u_bool;
    self->vsync_idle_low = vals[ARG_vsync_idle_low].u_bool;
    self->de_idle_high = vals[ARG_de_idle_high].u_bool;
    self->pclk_active_high = vals[ARG_pclk_active_high].u_bool;
    self->pclk_idle_high = vals[ARG_pclk_idle_high].u_bool;
    self->overscan_left = vals[ARG_overscan_left].u_int;
    self->rgb666_layout = rgb666;
    self->bits_per_pixel = 16;

#if defined(ESP_PLATFORM) && SOC_LCD_RGB_SUPPORTED
    self->panel = NULL;
    self->panel_ready = false;
    rgbframebuffer_fill_data_pins(self, vals[ARG_red].u_obj, vals[ARG_green].u_obj,
        vals[ARG_blue].u_obj, vals[ARG_data].u_obj);
    if (rgb666 && self->data_pin_count == 18) {
        self->bits_per_pixel = 18;
    }
#else
    self->data_pin_count = 0;
#endif

    return MP_OBJ_FROM_PTR(self);
}

static mp_obj_t rgbframebuffer_refresh(mp_obj_t self_in) {
    rgbframebuffer_obj_t *self = MP_OBJ_TO_PTR(self_in);
#if defined(ESP_PLATFORM) && SOC_LCD_RGB_SUPPORTED
    rgbframebuffer_init_panel(self);
    rgbframebuffer_raise_esp_err(esp_cache_msync(self->buf, self->buf_len, ESP_CACHE_MSYNC_FLAG_DIR_C2M));
    rgbframebuffer_raise_esp_err(esp_lcd_panel_draw_bitmap(self->panel, 0, 0, self->width, self->height, self->buf));
    rgbframebuffer_raise_esp_err(esp_lcd_rgb_panel_refresh(self->panel));
    return mp_const_none;
#else
    mp_raise_msg(&mp_type_NotImplementedError, MP_ERROR_TEXT("ESP-IDF dotclock scanout not supported on this target"));
#endif
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
#if defined(ESP_PLATFORM) && SOC_LCD_RGB_SUPPORTED
    if (self->panel_ready && self->panel != NULL) {
        esp_lcd_panel_del(self->panel);
        self->panel = NULL;
        self->panel_ready = false;
    }
#endif
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
