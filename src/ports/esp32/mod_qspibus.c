// SPDX-License-Identifier: MIT
// QSPI display bus for ESP32-S3 (esp_lcd panel IO SPI + quad_mode).
// API matches CircuitPython qspibus.QSPIBus; lifecycle matches displayif soft-reset.
//
// Adapted from CircuitPython ports/espressif/common-hal/qspibus/QSPIBus.c
// (MIT; Copyright (c) 2026 Przemyslaw Patrick Socha).

#include <string.h>

#include "py/runtime.h"
#include "py/obj.h"
#include "displayif/soft_reset.h"
#include "displayif_esp32_pins.h"

#include "sdkconfig.h"

#if defined(CONFIG_IDF_TARGET_ESP32S3)

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_lcd_io_spi.h"
#include "esp_lcd_panel_io.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define QSPI_OPCODE_WRITE_CMD (0x02U)
#define QSPI_OPCODE_WRITE_COLOR (0x32U)
#define LCD_CMD_RAMWR (0x2CU)
#define LCD_CMD_RAMWRC (0x3CU)
#define LCD_CMD_DISPOFF (0x28U)
#define LCD_CMD_SLPIN (0x10U)
#define QSPI_DMA_BUFFER_COUNT (2U)
#define QSPI_DMA_BUFFER_SIZE (16U * 1024U)
#define QSPI_COLOR_TIMEOUT_MS (1000U)

typedef struct _qspibus_obj_t {
    mp_obj_base_t base;
    esp_lcd_panel_io_handle_t io_handle;
    spi_host_device_t host_id;
    int8_t clock_pin;
    int8_t data0_pin;
    int8_t data1_pin;
    int8_t data2_pin;
    int8_t data3_pin;
    int8_t cs_pin;
    int8_t dcx_pin;
    int8_t reset_pin;
    uint32_t frequency;
    bool bus_initialized;
    bool in_transaction;
    bool has_pending_command;
    uint8_t pending_command;
    bool transfer_in_progress;
    uint8_t active_buffer;
    uint8_t inflight_transfers;
    size_t dma_buffer_size;
    uint8_t *dma_buffer[QSPI_DMA_BUFFER_COUNT];
    SemaphoreHandle_t transfer_done_sem;
    bool deinited;
} qspibus_obj_t;

static const mp_obj_type_t qspibus_type;

static qspibus_obj_t *s_active;
static bool s_soft_reset_registered;

static void qspibus_raise_esp_err(esp_err_t err) {
    if (err == ESP_OK) {
        return;
    }
    mp_raise_msg_varg(&mp_type_OSError, MP_ERROR_TEXT("ESP-IDF error %d (%s)"), err, esp_err_to_name(err));
}

static void qspibus_release_dma_buffers(qspibus_obj_t *self) {
    for (size_t i = 0; i < QSPI_DMA_BUFFER_COUNT; i++) {
        if (self->dma_buffer[i] != NULL) {
            heap_caps_free(self->dma_buffer[i]);
            self->dma_buffer[i] = NULL;
        }
    }
    self->dma_buffer_size = 0;
    self->active_buffer = 0;
    self->inflight_transfers = 0;
    self->transfer_in_progress = false;
}

static bool qspibus_allocate_dma_buffers(qspibus_obj_t *self) {
    const size_t candidates[] = {
        QSPI_DMA_BUFFER_SIZE,
        QSPI_DMA_BUFFER_SIZE / 2,
        QSPI_DMA_BUFFER_SIZE / 4,
    };

    for (size_t c = 0; c < MP_ARRAY_SIZE(candidates); c++) {
        size_t size = candidates[c];
        bool ok = true;
        for (size_t i = 0; i < QSPI_DMA_BUFFER_COUNT; i++) {
            self->dma_buffer[i] = heap_caps_malloc(size, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
            if (self->dma_buffer[i] == NULL) {
                ok = false;
                break;
            }
        }
        if (ok) {
            self->dma_buffer_size = size;
            self->active_buffer = 0;
            self->inflight_transfers = 0;
            self->transfer_in_progress = false;
            return true;
        }
        qspibus_release_dma_buffers(self);
    }
    return false;
}

static void qspibus_reset_transfer_state(qspibus_obj_t *self) {
    self->inflight_transfers = 0;
    self->transfer_in_progress = false;
    if (self->transfer_done_sem != NULL) {
        while (xSemaphoreTake(self->transfer_done_sem, 0) == pdTRUE) {
        }
    }
}

static bool qspibus_wait_one_transfer_done(qspibus_obj_t *self, TickType_t timeout) {
    if (self->inflight_transfers == 0) {
        self->transfer_in_progress = false;
        return true;
    }
    if (xSemaphoreTake(self->transfer_done_sem, timeout) != pdTRUE) {
        return false;
    }
    self->inflight_transfers--;
    self->transfer_in_progress = (self->inflight_transfers > 0);
    return true;
}

static bool qspibus_wait_all_transfers_done(qspibus_obj_t *self, TickType_t timeout) {
    while (self->inflight_transfers > 0) {
        if (!qspibus_wait_one_transfer_done(self, timeout)) {
            return false;
        }
    }
    return true;
}

static void qspibus_check_active(qspibus_obj_t *self) {
    if (self->deinited || !self->bus_initialized) {
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("qspibus is deinited"));
    }
}

static void qspibus_send_command_bytes(qspibus_obj_t *self, uint8_t command, const uint8_t *data, size_t len) {
    qspibus_check_active(self);
    if (self->inflight_transfers >= QSPI_DMA_BUFFER_COUNT) {
        if (!qspibus_wait_one_transfer_done(self, pdMS_TO_TICKS(QSPI_COLOR_TIMEOUT_MS))) {
            qspibus_reset_transfer_state(self);
            mp_raise_OSError(MP_ETIMEDOUT);
        }
    }

    uint32_t packed_cmd = ((uint32_t)QSPI_OPCODE_WRITE_CMD << 24) | ((uint32_t)command << 8);
    esp_err_t err = esp_lcd_panel_io_tx_param(self->io_handle, packed_cmd, data, len);
    if (err != ESP_OK) {
        qspibus_reset_transfer_state(self);
        qspibus_raise_esp_err(err);
    }
}

static bool qspibus_is_color_payload_command(uint8_t command) {
    return command == LCD_CMD_RAMWR || command == LCD_CMD_RAMWRC;
}

static void qspibus_send_color_bytes(qspibus_obj_t *self, uint8_t command, const uint8_t *data, size_t len) {
    qspibus_check_active(self);

    if (len == 0) {
        qspibus_send_command_bytes(self, command, NULL, 0);
        return;
    }

    uint8_t chunk_command = command;
    const uint8_t *cursor = data;
    size_t remaining = len;

    if (self->inflight_transfers == 0 && self->transfer_done_sem != NULL) {
        while (xSemaphoreTake(self->transfer_done_sem, 0) == pdTRUE) {
        }
    }

    while (remaining > 0) {
        if (self->inflight_transfers >= QSPI_DMA_BUFFER_COUNT) {
            if (!qspibus_wait_one_transfer_done(self, pdMS_TO_TICKS(QSPI_COLOR_TIMEOUT_MS))) {
                qspibus_reset_transfer_state(self);
                mp_raise_OSError(MP_ETIMEDOUT);
            }
        }

        size_t chunk = remaining;
        if (chunk > self->dma_buffer_size) {
            chunk = self->dma_buffer_size;
        }

        uint8_t *buffer = self->dma_buffer[self->active_buffer];
        memcpy(buffer, cursor, chunk);

        uint32_t packed_cmd = ((uint32_t)QSPI_OPCODE_WRITE_COLOR << 24) | ((uint32_t)chunk_command << 8);
        esp_err_t err = esp_lcd_panel_io_tx_color(self->io_handle, packed_cmd, buffer, chunk);
        if (err != ESP_OK) {
            qspibus_reset_transfer_state(self);
            qspibus_raise_esp_err(err);
        }

        self->inflight_transfers++;
        self->transfer_in_progress = true;
        self->active_buffer = (uint8_t)((self->active_buffer + 1) % QSPI_DMA_BUFFER_COUNT);

        if (chunk_command == LCD_CMD_RAMWR) {
            chunk_command = LCD_CMD_RAMWRC;
        }

        cursor += chunk;
        remaining -= chunk;
    }
}

static void qspibus_panel_sleep_best_effort(qspibus_obj_t *self) {
    if (!self->bus_initialized || self->io_handle == NULL) {
        return;
    }

    if (!qspibus_wait_all_transfers_done(self, pdMS_TO_TICKS(QSPI_COLOR_TIMEOUT_MS))) {
        qspibus_reset_transfer_state(self);
    }

    if (self->has_pending_command) {
        uint32_t pending = ((uint32_t)QSPI_OPCODE_WRITE_CMD << 24) | ((uint32_t)self->pending_command << 8);
        (void)esp_lcd_panel_io_tx_param(self->io_handle, pending, NULL, 0);
        self->has_pending_command = false;
    }

    uint32_t disp_off = ((uint32_t)QSPI_OPCODE_WRITE_CMD << 24) | ((uint32_t)LCD_CMD_DISPOFF << 8);
    (void)esp_lcd_panel_io_tx_param(self->io_handle, disp_off, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(20));

    uint32_t sleep_in = ((uint32_t)QSPI_OPCODE_WRITE_CMD << 24) | ((uint32_t)LCD_CMD_SLPIN << 8);
    (void)esp_lcd_panel_io_tx_param(self->io_handle, sleep_in, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(120));
}

static bool IRAM_ATTR qspibus_on_color_trans_done(
    esp_lcd_panel_io_handle_t io_handle,
    esp_lcd_panel_io_event_data_t *event_data,
    void *user_ctx) {
    (void)io_handle;
    (void)event_data;

    qspibus_obj_t *self = (qspibus_obj_t *)user_ctx;
    if (self == NULL || self->transfer_done_sem == NULL) {
        return false;
    }
    BaseType_t x_higher_priority_task_woken = pdFALSE;
    xSemaphoreGiveFromISR(self->transfer_done_sem, &x_higher_priority_task_woken);
    return x_higher_priority_task_woken == pdTRUE;
}

static void qspibus_deinit_internal(qspibus_obj_t *self) {
    if (self == NULL || self->deinited) {
        return;
    }

    if (self->bus_initialized) {
        qspibus_panel_sleep_best_effort(self);
        self->in_transaction = false;

        if (self->io_handle != NULL) {
            esp_lcd_panel_io_del(self->io_handle);
            self->io_handle = NULL;
        }

        spi_bus_free(self->host_id);
        self->bus_initialized = false;
    }

    if (self->transfer_done_sem != NULL) {
        SemaphoreHandle_t sem = self->transfer_done_sem;
        self->transfer_done_sem = NULL;
        vSemaphoreDelete(sem);
    }

    qspibus_release_dma_buffers(self);

    self->has_pending_command = false;
    self->pending_command = 0;
    self->transfer_in_progress = false;
    self->inflight_transfers = 0;
    self->deinited = true;

    if (s_active == self) {
        s_active = NULL;
    }
}

static void qspibus_host_teardown(void) {
    qspibus_deinit_internal(s_active);
    s_active = NULL;
}

static void qspibus_ensure_soft_reset_registered(void) {
    if (!s_soft_reset_registered) {
        displayif_register_soft_reset(qspibus_host_teardown);
        s_soft_reset_registered = true;
    }
}

static int qspibus_optional_gpio(mp_obj_t pin_obj) {
    if (pin_obj == mp_const_none || pin_obj == MP_OBJ_NULL) {
        return -1;
    }
    return displayif_esp32_pin_gpio(pin_obj);
}

static mp_obj_t qspibus_make(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args) {
    enum {
        ARG_clock,
        ARG_data0,
        ARG_data1,
        ARG_data2,
        ARG_data3,
        ARG_cs,
        ARG_dcx,
        ARG_reset,
        ARG_frequency,
    };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_clock, MP_ARG_KW_ONLY | MP_ARG_OBJ | MP_ARG_REQUIRED, { .u_obj = MP_OBJ_NULL } },
        { MP_QSTR_data0, MP_ARG_KW_ONLY | MP_ARG_OBJ | MP_ARG_REQUIRED, { .u_obj = MP_OBJ_NULL } },
        { MP_QSTR_data1, MP_ARG_KW_ONLY | MP_ARG_OBJ | MP_ARG_REQUIRED, { .u_obj = MP_OBJ_NULL } },
        { MP_QSTR_data2, MP_ARG_KW_ONLY | MP_ARG_OBJ | MP_ARG_REQUIRED, { .u_obj = MP_OBJ_NULL } },
        { MP_QSTR_data3, MP_ARG_KW_ONLY | MP_ARG_OBJ | MP_ARG_REQUIRED, { .u_obj = MP_OBJ_NULL } },
        { MP_QSTR_cs, MP_ARG_KW_ONLY | MP_ARG_OBJ | MP_ARG_REQUIRED, { .u_obj = MP_OBJ_NULL } },
        { MP_QSTR_dcx, MP_ARG_KW_ONLY | MP_ARG_OBJ, { .u_obj = mp_const_none } },
        { MP_QSTR_reset, MP_ARG_KW_ONLY | MP_ARG_OBJ, { .u_obj = mp_const_none } },
        { MP_QSTR_frequency, MP_ARG_KW_ONLY | MP_ARG_INT, { .u_int = 80000000 } },
    };

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, all_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    mp_int_t frequency = args[ARG_frequency].u_int;
    if (frequency < 1 || frequency > 80000000) {
        mp_raise_ValueError(MP_ERROR_TEXT("frequency must be 1..80000000"));
    }

    int clock_pin = displayif_esp32_pin_gpio(args[ARG_clock].u_obj);
    int data0_pin = displayif_esp32_pin_gpio(args[ARG_data0].u_obj);
    int data1_pin = displayif_esp32_pin_gpio(args[ARG_data1].u_obj);
    int data2_pin = displayif_esp32_pin_gpio(args[ARG_data2].u_obj);
    int data3_pin = displayif_esp32_pin_gpio(args[ARG_data3].u_obj);
    int cs_pin = displayif_esp32_pin_gpio(args[ARG_cs].u_obj);
    int dcx_pin = qspibus_optional_gpio(args[ARG_dcx].u_obj);
    int reset_pin = qspibus_optional_gpio(args[ARG_reset].u_obj);

    qspibus_ensure_soft_reset_registered();
    qspibus_host_teardown();

    qspibus_obj_t *self = mp_obj_malloc(qspibus_obj_t, type);
    memset(((uint8_t *)self) + sizeof(mp_obj_base_t), 0, sizeof(*self) - sizeof(mp_obj_base_t));
    self->host_id = SPI2_HOST;
    self->clock_pin = (int8_t)clock_pin;
    self->data0_pin = (int8_t)data0_pin;
    self->data1_pin = (int8_t)data1_pin;
    self->data2_pin = (int8_t)data2_pin;
    self->data3_pin = (int8_t)data3_pin;
    self->cs_pin = (int8_t)cs_pin;
    self->dcx_pin = (int8_t)dcx_pin;
    self->reset_pin = (int8_t)reset_pin;
    self->frequency = (uint32_t)frequency;
    self->deinited = false;

    self->transfer_done_sem = xSemaphoreCreateCounting(QSPI_DMA_BUFFER_COUNT, 0);
    if (self->transfer_done_sem == NULL) {
        self->deinited = true;
        mp_raise_msg(&mp_type_MemoryError, MP_ERROR_TEXT("ESP-IDF memory allocation failed"));
    }

    if (!qspibus_allocate_dma_buffers(self)) {
        qspibus_deinit_internal(self);
        mp_raise_msg(&mp_type_MemoryError, MP_ERROR_TEXT("Could not allocate DMA capable buffer"));
    }

    const spi_bus_config_t bus_config = {
        .sclk_io_num = self->clock_pin,
        .mosi_io_num = self->data0_pin,
        .miso_io_num = self->data1_pin,
        .quadwp_io_num = self->data2_pin,
        .quadhd_io_num = self->data3_pin,
        .max_transfer_sz = (int)self->dma_buffer_size,
        .flags = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_GPIO_PINS,
    };

    esp_err_t err = spi_bus_initialize(self->host_id, &bus_config, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        qspibus_deinit_internal(self);
        mp_raise_ValueError(MP_ERROR_TEXT("SPI bus in use"));
    }
    self->bus_initialized = true;

    const esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = self->cs_pin,
        .dc_gpio_num = -1,
        .spi_mode = 0,
        .pclk_hz = self->frequency,
        .trans_queue_depth = QSPI_DMA_BUFFER_COUNT,
        .on_color_trans_done = qspibus_on_color_trans_done,
        .user_ctx = self,
        .lcd_cmd_bits = 32,
        .lcd_param_bits = 8,
        .flags = {
            .quad_mode = 1,
        },
    };

    err = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)self->host_id, &io_config, &self->io_handle);
    if (err != ESP_OK) {
        qspibus_deinit_internal(self);
        qspibus_raise_esp_err(err);
    }

    if (self->dcx_pin >= 0) {
        gpio_set_direction((gpio_num_t)self->dcx_pin, GPIO_MODE_OUTPUT);
        gpio_set_level((gpio_num_t)self->dcx_pin, 1);
    }

    if (self->reset_pin >= 0) {
        gpio_set_direction((gpio_num_t)self->reset_pin, GPIO_MODE_OUTPUT);
        gpio_set_level((gpio_num_t)self->reset_pin, 0);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level((gpio_num_t)self->reset_pin, 1);
        vTaskDelay(pdMS_TO_TICKS(120));
    }

    s_active = self;
    return MP_OBJ_FROM_PTR(self);
}

static mp_obj_t qspibus_reset(mp_obj_t self_in) {
    qspibus_obj_t *self = MP_OBJ_TO_PTR(self_in);
    qspibus_check_active(self);
    if (self->reset_pin < 0) {
        mp_raise_msg_varg(&mp_type_RuntimeError, MP_ERROR_TEXT("No %q pin"), MP_QSTR_reset);
    }
    gpio_set_level((gpio_num_t)self->reset_pin, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level((gpio_num_t)self->reset_pin, 1);
    vTaskDelay(pdMS_TO_TICKS(120));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(qspibus_reset_obj, qspibus_reset);

static bool qspibus_begin_transaction(qspibus_obj_t *self) {
    if (!self->bus_initialized || self->in_transaction || self->has_pending_command) {
        return false;
    }
    if (self->transfer_in_progress) {
        if (!qspibus_wait_all_transfers_done(self, pdMS_TO_TICKS(QSPI_COLOR_TIMEOUT_MS))) {
            qspibus_reset_transfer_state(self);
            return false;
        }
    }
    self->in_transaction = true;
    return true;
}

static void qspibus_send_bytes(qspibus_obj_t *self, bool is_command, const uint8_t *data, uint32_t data_length) {
    if (is_command) {
        for (uint32_t i = 0; i < data_length; i++) {
            if (self->has_pending_command) {
                qspibus_send_command_bytes(self, self->pending_command, NULL, 0);
            }
            self->pending_command = data[i];
            self->has_pending_command = true;
        }
        return;
    }

    if (!self->has_pending_command) {
        if (data_length == 0) {
            return;
        }
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("Internal error"));
    }

    if (data_length == 0) {
        qspibus_send_command_bytes(self, self->pending_command, NULL, 0);
        self->has_pending_command = false;
        return;
    }

    if (qspibus_is_color_payload_command(self->pending_command)) {
        qspibus_send_color_bytes(self, self->pending_command, data, data_length);
    } else {
        qspibus_send_command_bytes(self, self->pending_command, data, data_length);
    }
    self->has_pending_command = false;
}

static void qspibus_end_transaction(qspibus_obj_t *self) {
    if (!self->bus_initialized) {
        return;
    }
    if (self->has_pending_command) {
        qspibus_send_command_bytes(self, self->pending_command, NULL, 0);
        self->has_pending_command = false;
    }
    self->in_transaction = false;
}

static mp_obj_t qspibus_send(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_command, ARG_data };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_command, MP_ARG_INT | MP_ARG_REQUIRED, { .u_int = 0 } },
        { MP_QSTR_data, MP_ARG_OBJ | MP_ARG_REQUIRED, { .u_obj = MP_OBJ_NULL } },
    };

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    qspibus_obj_t *self = MP_OBJ_TO_PTR(pos_args[0]);
    qspibus_check_active(self);

    mp_int_t command_i = args[ARG_command].u_int;
    if (command_i < 0 || command_i > 255) {
        mp_raise_ValueError(MP_ERROR_TEXT("command must be 0..255"));
    }
    uint8_t command = (uint8_t)command_i;

    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(args[ARG_data].u_obj, &bufinfo, MP_BUFFER_READ);

    if (self->has_pending_command) {
        qspibus_send_command_bytes(self, self->pending_command, NULL, 0);
        self->has_pending_command = false;
        // Mirror CP write_data(NULL, 0) flush path for staged command.
    }

    while (!qspibus_begin_transaction(self)) {
        MICROPY_EVENT_POLL_HOOK;
    }
    qspibus_send_bytes(self, true, &command, 1);
    qspibus_send_bytes(self, false, (const uint8_t *)bufinfo.buf, (uint32_t)bufinfo.len);
    qspibus_end_transaction(self);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(qspibus_send_obj, 1, qspibus_send);

static mp_obj_t qspibus_write_command(mp_obj_t self_in, mp_obj_t command_obj) {
    qspibus_obj_t *self = MP_OBJ_TO_PTR(self_in);
    qspibus_check_active(self);
    if (self->in_transaction) {
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("Internal error"));
    }

    mp_int_t command_i = mp_obj_get_int(command_obj);
    if (command_i < 0 || command_i > 255) {
        mp_raise_ValueError(MP_ERROR_TEXT("command must be 0..255"));
    }

    if (self->has_pending_command) {
        qspibus_send_command_bytes(self, self->pending_command, NULL, 0);
    }
    self->pending_command = (uint8_t)command_i;
    self->has_pending_command = true;
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(qspibus_write_command_obj, qspibus_write_command);

static mp_obj_t qspibus_write_data(mp_obj_t self_in, mp_obj_t data_obj) {
    qspibus_obj_t *self = MP_OBJ_TO_PTR(self_in);
    qspibus_check_active(self);
    if (self->in_transaction) {
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("Internal error"));
    }

    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(data_obj, &bufinfo, MP_BUFFER_READ);

    if (bufinfo.len == 0) {
        if (self->has_pending_command) {
            qspibus_send_command_bytes(self, self->pending_command, NULL, 0);
            self->has_pending_command = false;
        }
        return mp_const_none;
    }
    if (!self->has_pending_command) {
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("Internal error"));
    }

    if (qspibus_is_color_payload_command(self->pending_command)) {
        qspibus_send_color_bytes(self, self->pending_command, (const uint8_t *)bufinfo.buf, bufinfo.len);
        if (!qspibus_wait_all_transfers_done(self, pdMS_TO_TICKS(QSPI_COLOR_TIMEOUT_MS))) {
            qspibus_reset_transfer_state(self);
            mp_raise_OSError(MP_ETIMEDOUT);
        }
    } else {
        qspibus_send_command_bytes(self, self->pending_command, (const uint8_t *)bufinfo.buf, bufinfo.len);
    }
    self->has_pending_command = false;
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(qspibus_write_data_obj, qspibus_write_data);

static mp_obj_t qspibus_deinit(mp_obj_t self_in) {
    qspibus_obj_t *self = MP_OBJ_TO_PTR(self_in);
    qspibus_deinit_internal(self);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(qspibus_deinit_obj, qspibus_deinit);

static const mp_rom_map_elem_t qspibus_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_reset), MP_ROM_PTR(&qspibus_reset_obj) },
    { MP_ROM_QSTR(MP_QSTR_send), MP_ROM_PTR(&qspibus_send_obj) },
    { MP_ROM_QSTR(MP_QSTR_write_command), MP_ROM_PTR(&qspibus_write_command_obj) },
    { MP_ROM_QSTR(MP_QSTR_write_data), MP_ROM_PTR(&qspibus_write_data_obj) },
    { MP_ROM_QSTR(MP_QSTR_deinit), MP_ROM_PTR(&qspibus_deinit_obj) },
    { MP_ROM_QSTR(MP_QSTR___del__), MP_ROM_PTR(&qspibus_deinit_obj) },
};
static MP_DEFINE_CONST_DICT(qspibus_locals_dict, qspibus_locals_dict_table);

static MP_DEFINE_CONST_OBJ_TYPE(
    qspibus_type,
    MP_QSTR_QSPIBus,
    MP_TYPE_FLAG_NONE,
    make_new, qspibus_make,
    locals_dict, &qspibus_locals_dict
);

static const mp_rom_map_elem_t qspibus_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_qspibus) },
    { MP_ROM_QSTR(MP_QSTR_QSPIBus), MP_ROM_PTR(&qspibus_type) },
};
static MP_DEFINE_CONST_DICT(qspibus_module_globals, qspibus_module_globals_table);

const mp_obj_module_t qspibus_user_cmodule = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&qspibus_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_qspibus, qspibus_user_cmodule);

#else /* !CONFIG_IDF_TARGET_ESP32S3 */

static mp_obj_t qspibus_unsupported_make(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    (void)type;
    (void)n_args;
    (void)n_kw;
    (void)args;
    mp_raise_msg(&mp_type_NotImplementedError, MP_ERROR_TEXT("qspibus requires ESP32-S3 (esp_lcd SPI quad_mode)"));
}

static mp_obj_t qspibus_deinit(mp_obj_t self_in) {
    (void)self_in;
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(qspibus_deinit_obj, qspibus_deinit);

static const mp_rom_map_elem_t qspibus_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_deinit), MP_ROM_PTR(&qspibus_deinit_obj) },
    { MP_ROM_QSTR(MP_QSTR___del__), MP_ROM_PTR(&qspibus_deinit_obj) },
};
static MP_DEFINE_CONST_DICT(qspibus_locals_dict, qspibus_locals_dict_table);

static MP_DEFINE_CONST_OBJ_TYPE(
    qspibus_type,
    MP_QSTR_QSPIBus,
    MP_TYPE_FLAG_NONE,
    make_new, qspibus_unsupported_make,
    locals_dict, &qspibus_locals_dict
);

static const mp_rom_map_elem_t qspibus_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_qspibus) },
    { MP_ROM_QSTR(MP_QSTR_QSPIBus), MP_ROM_PTR(&qspibus_type) },
};
static MP_DEFINE_CONST_DICT(qspibus_module_globals, qspibus_module_globals_table);

const mp_obj_module_t qspibus_user_cmodule = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&qspibus_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_qspibus, qspibus_user_cmodule);

#endif /* CONFIG_IDF_TARGET_ESP32S3 */
