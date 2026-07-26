// SPDX-License-Identifier: MIT
// RP2040/RP2350 Protomatter timer and pin backend for rgbmatrix.

#include "displayif/rgbmatrix_pm.h"

#if defined(DISPLAYIF_RGBMATRIX_USE_PROTOMATTER)

#include "machine_pin.h"

// PWM slice used for Protomatter bitplane timing (avoid slice 0 used by Arduino defaults).
#define DISPLAYIF_RP2_PWM_SLICE 7

bool displayif_rgbmatrix_pm_available(void) {
    return true;
}

void *displayif_rgbmatrix_pm_timer_alloc(void) {
    return (void *)(uintptr_t)DISPLAYIF_RP2_PWM_SLICE;
}

void displayif_rgbmatrix_pm_timer_enable(void *timer) {
    (void)timer;
}

void displayif_rgbmatrix_pm_timer_disable(void *timer) {
    (void)timer;
}

void displayif_rgbmatrix_pm_timer_free(void *timer) {
    displayif_rgbmatrix_pm_timer_disable(timer);
}

void displayif_rgbmatrix_pm_set_active(void *core) {
    extern void *_PM_protoPtr;
    _PM_protoPtr = core;
}

uint8_t displayif_rgbmatrix_pm_pin_index(uint8_t pm_slot, mp_obj_t pin_obj) {
    (void)pm_slot;
    const machine_pin_obj_t *pin = machine_pin_find(pin_obj);
    return pin->id;
}

#endif /* DISPLAYIF_RGBMATRIX_USE_PROTOMATTER */
