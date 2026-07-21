// SPDX-License-Identifier: MIT
// Dot-clock RGB framebuffer for esp32 port (ESP-IDF esp_lcd RGB panel).
//
// Python: displayif.DotClockFramebuffer (module name "displayif").
// CircuitPython equivalent: dotclockframebuffer.DotClockFramebuffer — continuous
// DMA scanout + bounce into a single live panel FB (auto_refresh=True), matching
// CP Qualia ``FramebufferDisplay(..., auto_refresh=True)``: blit/fill are visible
// without an explicit refresh()/show(). (An earlier tear-free double-FB path left
// LVGL painting a back buffer the panel never scanned.)
//
// Buffer typecode 'B', native blit/fill_rect. Reference: Qualia S3 + TL040HDS20.
// Lifecycle: idempotent deinit/__del__/ctor + soft-reset teardown
// (see include/displayif/soft_reset.h, IDEMPOTENT_LIFECYCLE.md,
// SOFT_RESET_AND_BRINGUP.md).

#include <string.h>
#include <stdlib.h>

#include "py/runtime.h"
#include "py/obj.h"
#include "py/binary.h"
#include "py/mpprint.h"
#include "displayif/mp_helpers.h"
#include "displayif/soft_reset.h"

#define DCFB_MAX_DATA_PINS 18
// Single live panel FB — matches CP Qualia auto_refresh=True (paint == scanout).
#define DCFB_NUM_FBS 1
#define DCFB_PRESENT_TIMEOUT_US 100000

#if defined(ESP_PLATFORM)
#include "esp_heap_caps.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "soc/soc_caps.h"
#if SOC_LCD_RGB_SUPPORTED
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_cache.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif
#endif

#include "displayif_esp32_pins.h"

typedef struct _dotclockframebuffer_obj_t {
    mp_obj_base_t base;
    uint16_t width;
    uint16_t height;
    uint16_t row_stride;
    uint8_t *buf;
    size_t buf_len;
    bool buf_owned; // true if we malloc'd buf; false if owned by esp_lcd panel
    int de_pin;
    int vsync_pin;
    int hsync_pin;
    int dclk_pin;
    int data_pins[DCFB_MAX_DATA_PINS];
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
    bool deinited;
#if defined(ESP_PLATFORM) && SOC_LCD_RGB_SUPPORTED
    esp_lcd_panel_handle_t panel;
    bool panel_ready;
    // Bounce DRAM refill memcpy's the PSRAM FB through the CPU cache. Explicit
    // esp_cache_msync on every blit contends on SPI0 with that ISR and causes
    // faint flicker (IDF draw_bitmap also skips FB msync when bb_size != 0).
    bool bounce_enabled;
    // Live panel FB(s). With DCFB_NUM_FBS==1, blit paints the scanned buffer.
    uint8_t *fbs[2];
    uint8_t draw_index;
    uint8_t num_fbs;
#endif
} dotclockframebuffer_obj_t;

#if defined(ESP_PLATFORM) && SOC_LCD_RGB_SUPPORTED
typedef struct {
    esp_lcd_panel_handle_t panel;
    bool panel_ready;
} dcfb_host_t;

static dcfb_host_t s_host;
#endif
static bool s_soft_reset_registered;

static const mp_obj_type_t dotclockframebuffer_type;

#if defined(ESP_PLATFORM)
static uint8_t *dotclockframebuffer_alloc(size_t nbytes) {
    uint8_t *ptr = heap_caps_malloc(nbytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (ptr == NULL) {
        mp_printf(&mp_plat_print,
            "displayif: SPIRAM alloc failed (%u bytes); trying internal heap. "
            "Enable CONFIG_SPIRAM and size PSRAM in sdkconfig for large RGB panels.\n",
            (unsigned)nbytes);
        ptr = heap_caps_malloc(nbytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    return ptr;
}

static void dotclockframebuffer_free(uint8_t *ptr) {
    if (ptr != NULL) {
        heap_caps_free(ptr);
    }
}
#else
static uint8_t *dotclockframebuffer_alloc(size_t nbytes) {
    return malloc(nbytes);
}

static void dotclockframebuffer_free(uint8_t *ptr) {
    free(ptr);
}
#endif

#if defined(ESP_PLATFORM) && SOC_LCD_RGB_SUPPORTED
static void dcfb_host_teardown(void) {
    if (s_host.panel_ready && s_host.panel != NULL) {
        esp_lcd_panel_del(s_host.panel);
        s_host.panel = NULL;
        s_host.panel_ready = false;
    }
}

static void dcfb_ensure_soft_reset_registered(void) {
    if (!s_soft_reset_registered) {
        displayif_register_soft_reset(dcfb_host_teardown);
        s_soft_reset_registered = true;
    }
}

static void dotclockframebuffer_raise_esp_err(esp_err_t err) {
    if (err == ESP_OK) {
        return;
    }
    mp_raise_msg_varg(&mp_type_OSError, MP_ERROR_TEXT("ESP-IDF error %d (%s)"), err, esp_err_to_name(err));
}

// Match CircuitPython DotClockFramebuffer: RGB565 wire order is B0..B4, G0..G5, R0..R4.
static void dotclockframebuffer_fill_data_pins(dotclockframebuffer_obj_t *self, mp_obj_t red, mp_obj_t green, mp_obj_t blue, mp_obj_t data) {
    int pins[DCFB_MAX_DATA_PINS];
    size_t count = 0;

    if (data != MP_OBJ_NULL) {
        count = displayif_esp32_pin_seq_to_gpios(data, pins, DCFB_MAX_DATA_PINS);
    } else {
        size_t n_blue = displayif_esp32_pin_seq_to_gpios(blue, pins, DCFB_MAX_DATA_PINS);
        size_t n_green = displayif_esp32_pin_seq_to_gpios(green, pins + n_blue, DCFB_MAX_DATA_PINS - n_blue);
        size_t n_red = displayif_esp32_pin_seq_to_gpios(red, pins + n_blue + n_green,
            DCFB_MAX_DATA_PINS - n_blue - n_green);
        count = n_blue + n_green + n_red;
        if (!(n_red == 5 && n_green == 6 && n_blue == 5)) {
            mp_raise_ValueError(MP_ERROR_TEXT("red/green/blue must be 5/6/5 pins (RGB565)"));
        }
    }

    if (count == 0 || count > DCFB_MAX_DATA_PINS) {
        mp_raise_ValueError(MP_ERROR_TEXT("invalid RGB data pin layout"));
    }

    self->data_pin_count = count;
    for (size_t i = 0; i < DCFB_MAX_DATA_PINS; i++) {
        self->data_pins[i] = (i < count) ? pins[i] : -1;
    }
}

#define DCFB_CACHE_LINE 128

// Incremented in bounce EOF ISR when a whole FB has been sent to LCD DMA.
// Used by refresh() to wait until the newly promoted front is safe to leave.
static volatile uint32_t s_dcfb_frame_count;

static bool IRAM_ATTR dcfb_on_frame_buf_complete(esp_lcd_panel_handle_t panel,
    const esp_lcd_rgb_panel_event_data_t *edata, void *user_ctx) {
    (void)panel;
    (void)edata;
    (void)user_ctx;
    s_dcfb_frame_count++;
    return false;
}

// Sync dirty rows for DMA that reads PSRAM behind the cache (no bounce).
// With bounce buffers, refill uses CPU memcpy through DCache — skip msync.
static void dotclockframebuffer_msync_rows(dotclockframebuffer_obj_t *self, int y, int h) {
    if (self->buf == NULL || h <= 0 || self->bounce_enabled) {
        return;
    }
    size_t start = (size_t)y * self->row_stride;
    size_t end = start + (size_t)h * self->row_stride;
    size_t align_mask = (size_t)DCFB_CACHE_LINE - 1;
    size_t synced = start & ~align_mask;
    size_t sync_end = (end + align_mask) & ~align_mask;
    if (sync_end > self->buf_len) {
        sync_end = (self->buf_len + align_mask) & ~align_mask;
    }
    if (sync_end > synced) {
        dotclockframebuffer_raise_esp_err(
            esp_cache_msync(self->buf + synced, sync_end - synced, ESP_CACHE_MSYNC_FLAG_DIR_C2M));
    }
}

// Continuous RGB scanout of the panel's own PSRAM FBs (Qualia path + tear-free present).
static void dotclockframebuffer_init_panel(dotclockframebuffer_obj_t *self) {
    if (self->panel_ready) {
        return;
    }

    // Round h_res up to a multiple of 16 (same as CircuitPython DotClockFramebuffer).
    uint32_t h_res = (uint32_t)((self->width + self->overscan_left + 15) / 16 * 16);

    esp_lcd_rgb_panel_config_t panel_config = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .data_width = 16,
        .bits_per_pixel = 16,
        // One live panel FB (CP Qualia auto_refresh=True model).
        .num_fbs = DCFB_NUM_FBS,
        // PSRAM cannot sustain 16bpp@720x720 DPI alone — bounce through DRAM
        // (ESP-IDF rgb_panel example / Qualia-class panels). Without this the
        // image walks/tears horizontally under load. When bounce is on, do not
        // esp_cache_msync the FB from blit (see bounce_enabled).
        .bounce_buffer_size_px = 20 * h_res,
        .dma_burst_size = 64,
        .hsync_gpio_num = self->hsync_pin,
        .vsync_gpio_num = self->vsync_pin,
        .de_gpio_num = self->de_pin,
        .pclk_gpio_num = self->dclk_pin,
        .disp_gpio_num = -1,
        .timings = {
            .pclk_hz = self->frequency,
            .h_res = h_res,
            .v_res = self->height,
            .hsync_pulse_width = self->hsync_pulse_width,
            .hsync_front_porch = self->hsync_front_porch,
            .hsync_back_porch = self->hsync_back_porch,
            .vsync_pulse_width = self->vsync_pulse_width,
            .vsync_front_porch = self->vsync_front_porch,
            .vsync_back_porch = self->vsync_back_porch,
            .flags = {
                .hsync_idle_low = self->hsync_idle_low,
                // Match CircuitPython DotClockFramebuffer (uses hsync idle for both).
                .vsync_idle_low = self->hsync_idle_low,
                .de_idle_high = self->de_idle_high,
                .pclk_active_neg = !self->pclk_active_high,
                .pclk_idle_high = self->pclk_idle_high,
            },
        },
        .flags = {
            // Continuous DMA scanout — required for TTL RGB panels (Qualia).
            .refresh_on_demand = 0,
            .fb_in_psram = 1,
        },
    };

    memcpy(panel_config.data_gpio_nums, self->data_pins, sizeof(int) * SOC_LCDCAM_RGB_DATA_WIDTH);

    esp_lcd_panel_handle_t panel = NULL;
    dotclockframebuffer_raise_esp_err(esp_lcd_new_rgb_panel(&panel_config, &panel));
    dotclockframebuffer_raise_esp_err(esp_lcd_panel_reset(panel));
    dotclockframebuffer_raise_esp_err(esp_lcd_panel_init(panel));

    if (self->overscan_left != 0) {
        dotclockframebuffer_raise_esp_err(esp_lcd_panel_set_gap(panel, self->overscan_left, 0));
    }

    esp_lcd_rgb_panel_event_callbacks_t cbs = {
        .on_frame_buf_complete = dcfb_on_frame_buf_complete,
    };
    dotclockframebuffer_raise_esp_err(esp_lcd_rgb_panel_register_event_callbacks(panel, &cbs, NULL));

    // Kick DMA once (CircuitPython does the same), then use the panel FBs.
    uint16_t kick = 0;
    dotclockframebuffer_raise_esp_err(esp_lcd_panel_draw_bitmap(panel, 0, 0, 1, 1, &kick));

    void *fb0 = NULL;
    dotclockframebuffer_raise_esp_err(esp_lcd_rgb_panel_get_frame_buffer(panel, 1, &fb0));
    if (fb0 == NULL) {
        esp_lcd_panel_del(panel);
        mp_raise_msg(&mp_type_MemoryError, MP_ERROR_TEXT("RGB panel frame buffer missing"));
    }

    // Drop any provisional malloc; panel owns the scanout buffer.
    if (self->buf_owned && self->buf != NULL) {
        dotclockframebuffer_free(self->buf);
    }
    self->fbs[0] = (uint8_t *)fb0;
    self->fbs[1] = NULL;
    self->num_fbs = DCFB_NUM_FBS;
    // Paint the live scanned FB (auto_refresh=True).
    self->draw_index = 0;
    self->buf = self->fbs[0];
    self->buf_owned = false;
    self->row_stride = (uint16_t)(2 * h_res);
    self->buf_len = (size_t)self->row_stride * self->height;
    self->bounce_enabled = (panel_config.bounce_buffer_size_px != 0);
    memset(self->fbs[0], 0, self->buf_len);
    // One-time writeback so the first bounce refill sees zeros.
    esp_cache_msync(self->fbs[0], self->buf_len, ESP_CACHE_MSYNC_FLAG_DIR_C2M);

    self->panel = panel;
    self->panel_ready = true;
    s_host.panel = panel;
    s_host.panel_ready = true;
}
#endif

static void dotclockframebuffer_deinit_internal(dotclockframebuffer_obj_t *self) {
    if (self->deinited) {
        return;
    }
#if defined(ESP_PLATFORM) && SOC_LCD_RGB_SUPPORTED
    if (s_host.panel_ready && s_host.panel != NULL) {
        esp_lcd_panel_del(s_host.panel);
        s_host.panel = NULL;
        s_host.panel_ready = false;
        self->panel = NULL;
        self->panel_ready = false;
        // Panel deletion frees its FB; do not heap_caps_free it.
        self->buf = NULL;
        self->buf_owned = false;
    }
#endif
    if (self->buf_owned && self->buf != NULL) {
        dotclockframebuffer_free(self->buf);
        self->buf = NULL;
        self->buf_owned = false;
    }
    self->deinited = true;
}

static mp_obj_t dotclockframebuffer_make(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
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

#if defined(ESP_PLATFORM) && SOC_LCD_RGB_SUPPORTED
    dcfb_host_teardown();
    dcfb_ensure_soft_reset_registered();
#endif

    dotclockframebuffer_obj_t *self = mp_obj_malloc(dotclockframebuffer_obj_t, type);
    self->width = vals[ARG_width].u_int;
    self->height = vals[ARG_height].u_int;
    self->row_stride = self->width * sizeof(uint16_t);
    self->buf_len = (size_t)self->row_stride * self->height;
    self->buf = NULL;
    self->buf_owned = false;

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
    self->deinited = false;
#if defined(ESP_PLATFORM) && SOC_LCD_RGB_SUPPORTED
    self->panel = NULL;
    self->panel_ready = false;
    self->bounce_enabled = false;
    self->fbs[0] = NULL;
    self->fbs[1] = NULL;
    self->draw_index = 0;
    self->num_fbs = 0;
#endif
    self->data_pin_count = 0;

#if defined(ESP_PLATFORM) && SOC_LCD_RGB_SUPPORTED
    dotclockframebuffer_fill_data_pins(self, vals[ARG_red].u_obj, vals[ARG_green].u_obj,
        vals[ARG_blue].u_obj, vals[ARG_data].u_obj);
    // Start continuous scanout immediately (CircuitPython DotClockFramebuffer model).
    dotclockframebuffer_init_panel(self);
#else
    // Host stub: software buffer only (no ESP-IDF RGB panel).
    self->buf = dotclockframebuffer_alloc(self->buf_len);
    if (self->buf == NULL) {
        mp_raise_msg(&mp_type_MemoryError, MP_ERROR_TEXT("RGB framebuffer allocation failed"));
    }
    self->buf_owned = true;
    memset(self->buf, 0, self->buf_len);
#endif

    return MP_OBJ_FROM_PTR(self);
}

static mp_obj_t dotclockframebuffer_refresh(mp_obj_t self_in) {
    dotclockframebuffer_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (self->deinited || self->buf == NULL) {
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("RGB framebuffer is deinited"));
    }
#if defined(ESP_PLATFORM) && SOC_LCD_RGB_SUPPORTED
    // auto_refresh=True / single live FB: optional cache writeback only.
    // Bounce mode already refills from PSRAM without per-blit msync.
    if (!self->bounce_enabled) {
        dotclockframebuffer_msync_rows(self, 0, self->height);
    }
    return mp_const_none;
#else
    mp_raise_msg(&mp_type_NotImplementedError, MP_ERROR_TEXT("ESP-IDF dotclock scanout not supported on this target"));
#endif
}
static MP_DEFINE_CONST_FUN_OBJ_1(dotclockframebuffer_refresh_obj, dotclockframebuffer_refresh);

/* blit(buf, x, y, w, h) — C memcpy into the scanout FB.
 * MicroPython has no memoryview.cast; FBDisplay's uint16 per-pixel path is
 * ~2s for 720x720. Native blit matches mipidsi / the CircuitPython Qualia fix. */
static mp_obj_t dotclockframebuffer_blit(size_t n_args, const mp_obj_t *args) {
    (void)n_args;
    dotclockframebuffer_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    if (self->deinited || self->buf == NULL) {
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("RGB framebuffer is deinited"));
    }
    mp_buffer_info_t srcinfo;
    mp_get_buffer_raise(args[1], &srcinfo, MP_BUFFER_READ);
    int x = mp_obj_get_int(args[2]);
    int y = mp_obj_get_int(args[3]);
    int w = mp_obj_get_int(args[4]);
    int h = mp_obj_get_int(args[5]);
    if (x < 0 || y < 0 || w <= 0 || h <= 0 || x + w > self->width || y + h > self->height) {
        mp_raise_ValueError(MP_ERROR_TEXT("blit rect out of range"));
    }
    size_t row_bytes = (size_t)w * sizeof(uint16_t);
    if (srcinfo.len < row_bytes * (size_t)h) {
        mp_raise_ValueError(MP_ERROR_TEXT("blit buffer too small"));
    }
    const uint8_t *src = srcinfo.buf;
    uint8_t *dst = self->buf + (size_t)y * self->row_stride + (size_t)x * sizeof(uint16_t);
    for (int row = 0; row < h; row++) {
        memcpy(dst, src, row_bytes);
        dst += self->row_stride;
        src += row_bytes;
    }
#if defined(ESP_PLATFORM) && SOC_LCD_RGB_SUPPORTED
    dotclockframebuffer_msync_rows(self, y, h);
#endif
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(dotclockframebuffer_blit_obj, 6, 6, dotclockframebuffer_blit);

/* fill_rect(x, y, w, h, color) — C RGB565 fill (MP has no bitmaptools). */
static mp_obj_t dotclockframebuffer_fill_rect(size_t n_args, const mp_obj_t *args) {
    (void)n_args;
    dotclockframebuffer_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    if (self->deinited || self->buf == NULL) {
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("RGB framebuffer is deinited"));
    }
    int x = mp_obj_get_int(args[1]);
    int y = mp_obj_get_int(args[2]);
    int w = mp_obj_get_int(args[3]);
    int h = mp_obj_get_int(args[4]);
    uint16_t color = (uint16_t)mp_obj_get_int(args[5]);
    if (x < 0 || y < 0 || w <= 0 || h <= 0 || x + w > self->width || y + h > self->height) {
        mp_raise_ValueError(MP_ERROR_TEXT("fill_rect out of range"));
    }
    for (int row = 0; row < h; row++) {
        uint16_t *dst = (uint16_t *)(self->buf + (size_t)(y + row) * self->row_stride + (size_t)x * sizeof(uint16_t));
        for (int col = 0; col < w; col++) {
            dst[col] = color;
        }
    }
#if defined(ESP_PLATFORM) && SOC_LCD_RGB_SUPPORTED
    dotclockframebuffer_msync_rows(self, y, h);
#endif
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(dotclockframebuffer_fill_rect_obj, 6, 6, dotclockframebuffer_fill_rect);

static mp_int_t dotclockframebuffer_get_buffer(mp_obj_t self_in, mp_buffer_info_t *bufinfo, mp_uint_t flags) {
    (void)flags;
    dotclockframebuffer_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (self->deinited || self->buf == NULL) {
        bufinfo->buf = NULL;
        bufinfo->len = 0;
        bufinfo->typecode = 'B';
        return 0;
    }
    bufinfo->buf = self->buf;
    bufinfo->len = self->buf_len;
    // Byte-addressable for displaysys.FBDisplay (MP has no memoryview.cast).
    bufinfo->typecode = 'B';
    return 0;
}

static mp_obj_t dotclockframebuffer_del(mp_obj_t self_in) {
    dotclockframebuffer_obj_t *self = MP_OBJ_TO_PTR(self_in);
    dotclockframebuffer_deinit_internal(self);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(dotclockframebuffer_del_obj, dotclockframebuffer_del);

static void dotclockframebuffer_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    dotclockframebuffer_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (dest[0] == MP_OBJ_NULL) {
        // Custom attr replaces locals_dict lookup — expose methods here (see mipidsi).
        if (attr == MP_QSTR_width) {
            dest[0] = mp_obj_new_int(self->width);
        } else if (attr == MP_QSTR_height) {
            dest[0] = mp_obj_new_int(self->height);
        } else if (attr == MP_QSTR_row_stride) {
            dest[0] = mp_obj_new_int(self->row_stride);
        } else if (attr == MP_QSTR_auto_refresh) {
            // True: paint the live scanout FB (CP Qualia FramebufferDisplay
            // auto_refresh=True). FBDisplay.show() is a no-op; blit/fill show up
            // on the next bounce/DMA frame.
            dest[0] = mp_const_true;
        } else if (attr == MP_QSTR_refresh) {
            dest[0] = MP_OBJ_FROM_PTR(&dotclockframebuffer_refresh_obj);
            dest[1] = self_in;
        } else if (attr == MP_QSTR_blit) {
            dest[0] = MP_OBJ_FROM_PTR(&dotclockframebuffer_blit_obj);
            dest[1] = self_in;
        } else if (attr == MP_QSTR_fill_rect) {
            dest[0] = MP_OBJ_FROM_PTR(&dotclockframebuffer_fill_rect_obj);
            dest[1] = self_in;
        } else if (attr == MP_QSTR_deinit) {
            dest[0] = MP_OBJ_FROM_PTR(&dotclockframebuffer_del_obj);
            dest[1] = self_in;
        } else if (attr == MP_QSTR___del__) {
            dest[0] = MP_OBJ_FROM_PTR(&dotclockframebuffer_del_obj);
            dest[1] = self_in;
        }
    }
}

static const mp_rom_map_elem_t dotclockframebuffer_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_refresh), MP_ROM_PTR(&dotclockframebuffer_refresh_obj) },
    { MP_ROM_QSTR(MP_QSTR_blit), MP_ROM_PTR(&dotclockframebuffer_blit_obj) },
    { MP_ROM_QSTR(MP_QSTR_fill_rect), MP_ROM_PTR(&dotclockframebuffer_fill_rect_obj) },
    { MP_ROM_QSTR(MP_QSTR_deinit), MP_ROM_PTR(&dotclockframebuffer_del_obj) },
    { MP_ROM_QSTR(MP_QSTR___del__), MP_ROM_PTR(&dotclockframebuffer_del_obj) },
};
static MP_DEFINE_CONST_DICT(dotclockframebuffer_locals_dict, dotclockframebuffer_locals_dict_table);

static MP_DEFINE_CONST_OBJ_TYPE(
    dotclockframebuffer_type,
    MP_QSTR_DotClockFramebuffer,
    MP_TYPE_FLAG_NONE,
    make_new, dotclockframebuffer_make,
    attr, dotclockframebuffer_attr,
    locals_dict, &dotclockframebuffer_locals_dict,
    buffer, dotclockframebuffer_get_buffer
);

static const mp_rom_map_elem_t dotclockframebuffer_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_displayif) },
    { MP_ROM_QSTR(MP_QSTR_DotClockFramebuffer), MP_ROM_PTR(&dotclockframebuffer_type) },
};
static MP_DEFINE_CONST_DICT(dotclockframebuffer_module_globals, dotclockframebuffer_module_globals_table);

const mp_obj_module_t dotclockframebuffer_user_cmodule = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&dotclockframebuffer_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_displayif, dotclockframebuffer_user_cmodule);
