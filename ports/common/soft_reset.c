// SPDX-License-Identifier: MIT
// Soft-reset teardown dispatcher for displayif (see include/displayif/soft_reset.h).

#include <stddef.h>

#include "displayif/soft_reset.h"

#ifndef DISPLAYIF_SOFT_RESET_MAX
#define DISPLAYIF_SOFT_RESET_MAX 16
#endif

static displayif_soft_reset_fn_t s_fns[DISPLAYIF_SOFT_RESET_MAX];
static unsigned s_count;

void displayif_register_soft_reset(displayif_soft_reset_fn_t fn) {
    if (fn == NULL) {
        return;
    }
    for (unsigned i = 0; i < s_count; i++) {
        if (s_fns[i] == fn) {
            return;
        }
    }
    if (s_count < DISPLAYIF_SOFT_RESET_MAX) {
        s_fns[s_count++] = fn;
    }
}

void displayif_unregister_soft_reset(displayif_soft_reset_fn_t fn) {
    if (fn == NULL) {
        return;
    }
    for (unsigned i = 0; i < s_count; i++) {
        if (s_fns[i] == fn) {
            s_fns[i] = s_fns[s_count - 1];
            s_fns[s_count - 1] = NULL;
            s_count--;
            return;
        }
    }
}

void displayif_soft_reset_all(void) {
    /* Walk a snapshot so a teardown that unregisters itself is safe. */
    unsigned n = s_count;
    displayif_soft_reset_fn_t local[DISPLAYIF_SOFT_RESET_MAX];
    for (unsigned i = 0; i < n; i++) {
        local[i] = s_fns[i];
    }
    for (unsigned i = 0; i < n; i++) {
        if (local[i] != NULL) {
            local[i]();
        }
    }
}

#if defined(DISPLAYIF_WRAP_MP_DEINIT)
/* Linker --wrap=mp_deinit: run displayif teardown before MicroPython deinit.
 * On soft-reset paths this is after gc_sweep_all(), so teardowns must not
 * m_free GC memory — only release non-GC host resources (ESP-IDF, DMA, PIO). */
extern void __real_mp_deinit(void);

void __wrap_mp_deinit(void) {
    displayif_soft_reset_all();
    __real_mp_deinit();
}
#endif
