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

void displayif_port_pre_gc_sweep(void) {
    for (machine_timer_obj_t *t = MP_STATE_PORT(machine_timer_obj_head); t != NULL; t = t->next) {
        machine_timer_stop(t);
        /* Drop the callback so a late ISR cannot schedule into freed heap. */
        t->handler = NULL;
        t->handler_ctx = NULL;
    }
}
