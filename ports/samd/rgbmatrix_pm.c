// SPDX-License-Identifier: MIT
// SAMD51 Protomatter timer and pin backend for rgbmatrix.

#include "displayif/rgbmatrix_pm.h"

#if defined(DISPLAYIF_RGBMATRIX_USE_PROTOMATTER) && defined(MCU_SAMD51)

#include "displayif_pm_samd51_arch.h"
#include "pin_af.h"
#include "core.h"

Protomatter_core *displayif_pm_proto_ptr = NULL;

bool displayif_rgbmatrix_pm_available(void) {
    return true;
}

void *displayif_rgbmatrix_pm_timer_alloc(void) {
    return (void *)TC3;
}

void displayif_rgbmatrix_pm_timer_enable(void *timer) {
    (void)timer;
}

void displayif_rgbmatrix_pm_timer_disable(void *timer) {
    if (timer != NULL) {
        Tc *tc = (Tc *)timer;
        tc->COUNT16.CTRLA.bit.ENABLE = 0;
        while (tc->COUNT16.SYNCBUSY.bit.STATUS) {
        }
        tc->COUNT16.INTENCLR.reg = TC_INTENCLR_OVF;
    }
}

void displayif_rgbmatrix_pm_timer_free(void *timer) {
    displayif_rgbmatrix_pm_timer_disable(timer);
}

void displayif_rgbmatrix_pm_set_active(void *core) {
    displayif_pm_proto_ptr = (Protomatter_core *)core;
}

uint8_t displayif_rgbmatrix_pm_pin_index(uint8_t pm_slot, mp_obj_t pin_obj) {
    (void)pm_slot;
    const machine_pin_obj_t *pin = pin_find(pin_obj);
    mp_hal_pin_output(pin->pin_id);
    return pin->pin_id;
}

void displayif_pm_tc_irq_handler(void) {
    if (displayif_pm_proto_ptr != NULL) {
        Tc *tc = (Tc *)displayif_pm_proto_ptr->timer;
        tc->COUNT16.INTFLAG.reg = TC_INTFLAG_OVF;
        _PM_row_handler(displayif_pm_proto_ptr);
    }
}

void TC3_Handler(void) {
    displayif_pm_tc_irq_handler();
}

#endif /* DISPLAYIF_RGBMATRIX_USE_PROTOMATTER && MCU_SAMD51 */
