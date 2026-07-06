// SPDX-License-Identifier: MIT
// ESP32 Protomatter timer backend for rgbmatrix.

#include "displayif/rgbmatrix_pm.h"

#if defined(DISPLAYIF_RGBMATRIX_USE_PROTOMATTER)

#include "driver/gptimer.h"
#include "esp_idf_version.h"

bool displayif_rgbmatrix_pm_available(void) {
#if defined(CONFIG_IDF_TARGET_ESP32S3)
    return true;
#else
    return false;
#endif
}

void *displayif_rgbmatrix_pm_timer_alloc(void) {
#if defined(CONFIG_IDF_TARGET_ESP32S3)
    gptimer_config_t config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 40000000,
        .flags = {
            .intr_shared = true,
        },
    };
    gptimer_handle_t timer = NULL;
    if (gptimer_new_timer(&config, &timer) != ESP_OK) {
        return NULL;
    }
    return timer;
#else
    return NULL;
#endif
}

void displayif_rgbmatrix_pm_timer_enable(void *timer) {
    (void)timer;
}

void displayif_rgbmatrix_pm_timer_disable(void *timer) {
#if defined(CONFIG_IDF_TARGET_ESP32S3)
    if (timer != NULL) {
        gptimer_stop((gptimer_handle_t)timer);
    }
#endif
}

void displayif_rgbmatrix_pm_timer_free(void *timer) {
#if defined(CONFIG_IDF_TARGET_ESP32S3)
    if (timer != NULL) {
        gptimer_handle_t handle = (gptimer_handle_t)timer;
        gptimer_disable(handle);
        gptimer_del_timer(handle);
    }
#endif
}

void displayif_rgbmatrix_pm_set_active(void *core) {
    extern Protomatter_core *_PM_protoPtr;
    _PM_protoPtr = (Protomatter_core *)core;
}

void displayif_rgbmatrix_pm_bind_pin(uint8_t pm_pin, mp_obj_t pin_obj) {
    (void)pm_pin;
    (void)pin_obj;
}

#endif /* DISPLAYIF_RGBMATRIX_USE_PROTOMATTER */
