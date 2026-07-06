// SPDX-License-Identifier: MIT
// mimxrt Protomatter timer and pin backend for rgbmatrix.

#include "displayif/rgbmatrix_pm.h"

#if defined(DISPLAYIF_RGBMATRIX_USE_PROTOMATTER) \
    && (defined(CPU_MIMXRT1062) || defined(CPU_MIMXRT1064) || defined(__IMXRT1062__))

#include "displayif_pm_mimxrt_arch.h"
#include "pin.h"
#include "core.h"
#include "fsl_pit.h"

displayif_pm_pin_entry_t displayif_pm_pin_table[64];
Protomatter_core *displayif_pm_proto_ptr = NULL;
pit_chnl_t displayif_pm_timer_channel = kPIT_Chnl_0;

bool displayif_rgbmatrix_pm_available(void) {
    return true;
}

void *displayif_rgbmatrix_pm_timer_alloc(void) {
    return &displayif_pm_timer_channel;
}

void displayif_rgbmatrix_pm_timer_enable(void *timer) {
    (void)timer;
}

void displayif_rgbmatrix_pm_timer_disable(void *timer) {
    if (timer != NULL) {
        pit_chnl_t channel = *(pit_chnl_t *)timer;
        PIT_StopTimer(PIT, channel);
        PIT_DisableInterrupts(PIT, channel, kPIT_TimerInterruptEnable);
    }
}

void displayif_rgbmatrix_pm_timer_free(void *timer) {
    displayif_rgbmatrix_pm_timer_disable(timer);
}

void displayif_rgbmatrix_pm_set_active(void *core) {
    displayif_pm_proto_ptr = (Protomatter_core *)core;
}

void displayif_rgbmatrix_pm_bind_pin(uint8_t pm_pin, mp_obj_t pin_obj) {
    if (pm_pin >= 64) {
        return;
    }
    const machine_pin_obj_t *pin = pin_find(pin_obj);
    displayif_pm_pin_table[pm_pin].gpio = pin->gpio;
    displayif_pm_pin_table[pm_pin].bit = pin->pin;
}

void _PM_timerInit(Protomatter_core *core) {
    pit_chnl_t channel = *(pit_chnl_t *)core->timer;
    pit_config_t cfg;
    PIT_GetDefaultConfig(&cfg);
    PIT_Init(PIT, &cfg);
    PIT_SetTimerPeriod(PIT, channel, 100000);
    NVIC_SetPriority(PIT_IRQn, 1);
    NVIC_EnableIRQ(PIT_IRQn);
}

void _PM_timerStart(Protomatter_core *core, uint32_t period) {
    pit_chnl_t channel = *(pit_chnl_t *)core->timer;
    PIT_StopTimer(PIT, channel);
    PIT_SetTimerPeriod(PIT, channel, period ? period : 1);
    PIT_ClearStatusFlags(PIT, channel, kPIT_TimerFlag);
    PIT_EnableInterrupts(PIT, channel, kPIT_TimerInterruptEnable);
    PIT_StartTimer(PIT, channel);
}

uint32_t _PM_timerGetCount(Protomatter_core *core) {
    pit_chnl_t channel = *(pit_chnl_t *)core->timer;
    uint32_t period = PIT->CHANNEL[channel].LDVAL + 1U;
    return period - PIT_GetCurrentTimerCount(PIT, channel);
}

uint32_t _PM_timerStop(Protomatter_core *core) {
    pit_chnl_t channel = *(pit_chnl_t *)core->timer;
    PIT_StopTimer(PIT, channel);
    PIT_DisableInterrupts(PIT, channel, kPIT_TimerInterruptEnable);
    return _PM_timerGetCount(core);
}

void displayif_pm_pit_irq_handler(void) {
    pit_chnl_t channel = displayif_pm_timer_channel;
    if (PIT_GetStatusFlags(PIT, channel) & kPIT_TimerFlag) {
        PIT_ClearStatusFlags(PIT, channel, kPIT_TimerFlag);
        if (displayif_pm_proto_ptr) {
            _PM_row_handler(displayif_pm_proto_ptr);
        }
    }
}

void PIT_IRQHandler(void) {
    displayif_pm_pit_irq_handler();
}

#endif /* DISPLAYIF_RGBMATRIX_USE_PROTOMATTER && CPU_MIMXRT1062 */
