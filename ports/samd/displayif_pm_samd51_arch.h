// SPDX-License-Identifier: MIT
// SAMD51 Protomatter arch hooks for displayif (included from samd51.h).

#pragma once

#if (defined(__SAMD51__) || defined(SAM_D5X_E5X) || defined(MCU_SAMD51)) \
    && defined(CIRCUITPY) && defined(DISPLAYIF_RGBMATRIX_USE_PROTOMATTER)

#ifndef asm
#define asm __asm__ __volatile__
#endif

#include "sam.h"
#include "core.h"
#include "py/gc.h"
#include "mphalport.h"

#define _PM_allocate(x) m_malloc(x)
#define _PM_free(x) m_free(x)

#define F_CPU CPU_FREQ

#define _PM_portOutRegister(pin) (&PORT->Group[(pin) / 32].OUT.reg)
#define _PM_portSetRegister(pin) (&PORT->Group[(pin) / 32].OUTSET.reg)
#define _PM_portClearRegister(pin) (&PORT->Group[(pin) / 32].OUTCLR.reg)
#define _PM_portToggleRegister(pin) (&PORT->Group[(pin) / 32].OUTTGL.reg)

#define _PM_pinOutput(pin) mp_hal_pin_output(pin)
#define _PM_pinInput(pin) mp_hal_pin_input(pin)
#define _PM_pinHigh(pin) mp_hal_pin_high(pin)
#define _PM_pinLow(pin) mp_hal_pin_low(pin)
#define _PM_portBitMask(pin) (1u << ((pin) & 31))

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define _PM_byteOffset(pin) (((pin) & 31) / 8)
#define _PM_wordOffset(pin) (((pin) & 31) / 16)
#else
#define _PM_byteOffset(pin) (3 - (((pin) & 31) / 8))
#define _PM_wordOffset(pin) (1 - (((pin) & 31) / 16))
#endif

#define _PM_timerFreq 48000000
#define _PM_TIMER_DEFAULT ((void *)TC3)

extern Protomatter_core *displayif_pm_proto_ptr;

#define _PM_protoPtr displayif_pm_proto_ptr

static inline void _hi_drive(uint8_t pin) {
    PORT->Group[pin / 32].WRCONFIG.reg =
        (pin & 16)
            ? PORT_WRCONFIG_WRPINCFG | PORT_WRCONFIG_DRVSTR |
                  PORT_WRCONFIG_HWSEL | (1 << (pin & 15))
            : PORT_WRCONFIG_WRPINCFG | PORT_WRCONFIG_DRVSTR | (1 << (pin & 15));
}

void displayif_pm_tc_irq_handler(void);

#endif /* SAMD51 && CIRCUITPY && DISPLAYIF_RGBMATRIX_USE_PROTOMATTER */
