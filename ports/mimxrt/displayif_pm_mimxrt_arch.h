// SPDX-License-Identifier: MIT
// i.MX RT106x Protomatter arch hooks for displayif (included from teensy4.h).

#pragma once

#if (defined(CPU_MIMXRT1062) || defined(CPU_MIMXRT1064) || defined(__IMXRT1062__)) \
    && defined(CIRCUITPY) && defined(DISPLAYIF_RGBMATRIX_USE_PROTOMATTER)

#include "fsl_gpio.h"
#include "fsl_pit.h"
#include "core.h"
#include "py/gc.h"

#include CPU_HEADER_H

#define _PM_allocate(x) m_malloc(x)
#define _PM_free(x) m_free(x)

typedef struct {
    GPIO_Type *gpio;
    uint8_t bit;
} displayif_pm_pin_entry_t;

extern displayif_pm_pin_entry_t displayif_pm_pin_table[64];
extern Protomatter_core *displayif_pm_proto_ptr;
extern pit_chnl_t displayif_pm_timer_channel;

void displayif_pm_pit_irq_handler(void);
void _PM_timerInit(Protomatter_core *core);
void _PM_timerStart(Protomatter_core *core, uint32_t period);
uint32_t _PM_timerGetCount(Protomatter_core *core);
uint32_t _PM_timerStop(Protomatter_core *core);

#define _PM_STRICT_32BIT_IO

#define _PM_SET_OFFSET 33
#define _PM_CLEAR_OFFSET 34
#define _PM_TOGGLE_OFFSET 35

#define _PM_portOutRegister(pin) ((void *)&displayif_pm_pin_table[(pin)].gpio->DR)
#define _PM_portSetRegister(pin) \
    ((volatile uint32_t *)&displayif_pm_pin_table[(pin)].gpio->DR + _PM_SET_OFFSET)
#define _PM_portClearRegister(pin) \
    ((volatile uint32_t *)&displayif_pm_pin_table[(pin)].gpio->DR + _PM_CLEAR_OFFSET)
#define _PM_portToggleRegister(pin) \
    ((volatile uint32_t *)&displayif_pm_pin_table[(pin)].gpio->DR + _PM_TOGGLE_OFFSET)

#define _PM_portBitMask(pin) (1U << displayif_pm_pin_table[(pin)].bit)

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define _PM_byteOffset(pin) (displayif_pm_pin_table[(pin)].bit / 8)
#define _PM_wordOffset(pin) (displayif_pm_pin_table[(pin)].bit / 16)
#else
#define _PM_byteOffset(pin) (3 - (displayif_pm_pin_table[(pin)].bit / 8))
#define _PM_wordOffset(pin) (1 - (displayif_pm_pin_table[(pin)].bit / 16))
#endif

#define _PM_protoPtr displayif_pm_proto_ptr

#define _PM_timerFreq 24000000
#define _PM_TIMER_DEFAULT ((void *)&displayif_pm_timer_channel)

static void _PM_pinOutput(uint8_t pin) {
    (void)pin;
}

static void _PM_pinInput(uint8_t pin) {
    (void)pin;
}

static void _PM_pinHigh(uint8_t pin) {
    GPIO_PortSet(displayif_pm_pin_table[pin].gpio, 1U << displayif_pm_pin_table[pin].bit);
}

static void _PM_pinLow(uint8_t pin) {
    GPIO_PortClear(displayif_pm_pin_table[pin].gpio, 1U << displayif_pm_pin_table[pin].bit);
}

#define _PM_clockHoldHigh                    \
    __asm__ volatile("nop; nop; nop; nop; nop; nop; nop;"); \
    __asm__ volatile("nop; nop; nop; nop; nop; nop; nop;");
#define _PM_clockHoldLow                                    \
    __asm__ volatile("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;"); \
    __asm__ volatile("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");

#define _PM_chunkSize 1

#endif /* CPU_MIMXRT1062 && CIRCUITPY && DISPLAYIF_RGBMATRIX_USE_PROTOMATTER */
