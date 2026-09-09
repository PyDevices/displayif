// SPDX-License-Identifier: MIT
// ESP32 port hook: stop machine.Timer (esp_timer) before MicroPython gc_sweep_all.
//
// Soft-reset order is gc_sweep_all() then mp_deinit(). An armed virtual timer
// can fire into swept Python callbacks and corrupt the next session (Load
// access fault in LVGL bindings). Host DMA/IRQ teardown for every displayif
// interface runs from the common --wrap=gc_sweep_all; this strong symbol only
// adds the timer stop via displayif_port_pre_gc_sweep().

#include "py/mpstate.h"
#include "machine_timer.h"

#include "displayif/soft_reset.h"

// MicroPython v1.29.0 reworked ports/esp32/machine_timer: machine_timer_disable()
// became machine_timer_stop(), and the timer's Python callable moved from the
// `callback` member to `handler_ctx` (the `handler` function pointer stayed,
// with a bool return). What this hook does is unchanged - stop the timer, then
// drop the callable so a late ISR cannot schedule into freed heap.
#if MICROPY_VERSION_MAJOR > 1 || (MICROPY_VERSION_MAJOR == 1 && MICROPY_VERSION_MINOR >= 29)
#define DISPLAYIF_TIMER_STOP(t)         machine_timer_stop(t)
#define DISPLAYIF_TIMER_DROP_CB(t)      ((t)->handler_ctx = mp_const_none)
#else
#define DISPLAYIF_TIMER_STOP(t)         machine_timer_disable(t)
#define DISPLAYIF_TIMER_DROP_CB(t)      ((t)->callback = mp_const_none)
#endif

void displayif_port_pre_gc_sweep(void) {
    for (machine_timer_obj_t *t = MP_STATE_PORT(machine_timer_obj_head); t != NULL; t = t->next) {
        DISPLAYIF_TIMER_STOP(t);
        /* Drop the callback so a late ISR cannot schedule into freed heap. */
        t->handler = NULL;
        DISPLAYIF_TIMER_DROP_CB(t);
    }
}
