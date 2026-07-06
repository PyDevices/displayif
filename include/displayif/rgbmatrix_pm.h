/* Protomatter backend hooks for rgbmatrix (port-specific). */

#ifndef DISPLAYIF_RGBMATRIX_PM_H
#define DISPLAYIF_RGBMATRIX_PM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "py/obj.h"

bool displayif_rgbmatrix_pm_available(void);
void *displayif_rgbmatrix_pm_timer_alloc(void);
void displayif_rgbmatrix_pm_timer_enable(void *timer);
void displayif_rgbmatrix_pm_timer_disable(void *timer);
void displayif_rgbmatrix_pm_timer_free(void *timer);
void displayif_rgbmatrix_pm_set_active(void *core);
void displayif_rgbmatrix_pm_bind_pin(uint8_t pm_pin, mp_obj_t pin_obj);

#endif /* DISPLAYIF_RGBMATRIX_PM_H */
