/* MicroPython helpers for displayif native modules. */

#ifndef DISPLAYIF_MP_HELPERS_H
#define DISPLAYIF_MP_HELPERS_H

#include "py/runtime.h"
#include "py/obj.h"

#define DISPLAYIF_EMPTY_DICT MP_OBJ_FROM_PTR(&mp_const_empty_dict_obj)

void displayif_pin_set(mp_obj_t pin, int value);
int displayif_pin_get(mp_obj_t pin);
bool displayif_obj_is_pin(mp_obj_t obj);
mp_obj_t displayif_pin_resolve(mp_obj_t pin_or_int);
int displayif_pin_id(mp_obj_t pin_or_int);
size_t displayif_pin_tuple_to_ints(mp_obj_t tuple, int *out, size_t max_out);
size_t displayif_pin_seq_to_ints(mp_obj_t seq, int *out, size_t max_out);
size_t displayif_pin_seq_to_objs(mp_obj_t seq, mp_obj_t *out, size_t max_out);
mp_obj_t displayif_machine_pin(mp_int_t pin_id, mp_int_t mode, mp_int_t value);
mp_obj_t displayif_machine_spi(mp_obj_t kwargs);
mp_obj_t displayif_obj_call_method0(mp_obj_t obj, qstr method);
mp_obj_t displayif_obj_call_method1(mp_obj_t obj, qstr method, mp_obj_t arg);
mp_obj_t displayif_obj_call_method_kw(mp_obj_t obj, qstr method, mp_obj_t kwargs);

#endif /* DISPLAYIF_MP_HELPERS_H */
