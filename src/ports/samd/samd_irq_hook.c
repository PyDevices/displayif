// SPDX-License-Identifier: MIT
// Install Protomatter TC3 handler into the SAMD vector table at runtime.
//
// MicroPython's samd_isr.c leaves TC3 as a null vector entry; copy the
// table to RAM, patch the TC3 slot, and repoint VTOR before enabling TC3.

#include "displayif/rgbmatrix_pm.h"

#if defined(DISPLAYIF_RGBMATRIX_USE_PROTOMATTER) && defined(MCU_SAMD51)

#include "sam.h"

typedef void (*displayif_isr_t)(void);

extern const displayif_isr_t isr_vector[];
extern void Default_Handler(void);
void displayif_pm_tc_irq_handler(void);

#if defined(MCU_SAMD51)
#define DISPLAYIF_SAMD_VECTOR_COUNT 137
#define DISPLAYIF_SAMD_TC3_VECTOR_INDEX 110
#endif

static displayif_isr_t displayif_samd_isr_ram[DISPLAYIF_SAMD_VECTOR_COUNT]
    __attribute__((aligned(1024)));
static bool displayif_samd_isr_installed;

void displayif_samd_pm_install_tc3_vector(void) {
    if (displayif_samd_isr_installed) {
        return;
    }

    for (size_t i = 0; i < DISPLAYIF_SAMD_VECTOR_COUNT; i++) {
        uintptr_t entry = (uintptr_t)isr_vector[i];
        displayif_samd_isr_ram[i] = (entry == 0) ? Default_Handler : (displayif_isr_t)entry;
    }
    displayif_samd_isr_ram[DISPLAYIF_SAMD_TC3_VECTOR_INDEX] = displayif_pm_tc_irq_handler;

    __DMB();
    SCB->VTOR = (uint32_t)displayif_samd_isr_ram;
    __DSB();

    displayif_samd_isr_installed = true;
}

#endif /* DISPLAYIF_RGBMATRIX_USE_PROTOMATTER && MCU_SAMD51 */
