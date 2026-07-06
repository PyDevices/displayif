/* MicroPython helpers for displayif native modules. */

#ifndef DISPLAYIF_MP_HELPERS_H
#define DISPLAYIF_MP_HELPERS_H

#include "py/runtime.h"
#include "py/obj.h"

#define DISPLAYIF_EMPTY_DICT MP_OBJ_FROM_PTR(&mp_const_empty_dict_obj)

void displayif_pin_set(mp_obj_t pin, int value);
int displayif_pin_get(mp_obj_t pin);
mp_obj_t displayif_machine_pin(mp_int_t pin_id, mp_int_t mode, mp_int_t value);
mp_obj_t displayif_machine_spi(mp_obj_t kwargs);
mp_obj_t displayif_obj_call_method0(mp_obj_t obj, qstr method);
mp_obj_t displayif_obj_call_method1(mp_obj_t obj, qstr method, mp_obj_t arg);
mp_obj_t displayif_obj_call_method_kw(mp_obj_t obj, qstr method, mp_obj_t kwargs);

#endif /* DISPLAYIF_MP_HELPERS_H */
