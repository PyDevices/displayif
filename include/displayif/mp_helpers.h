/* MicroPython helpers for displayif native modules. */

#ifndef DISPLAYIF_MP_HELPERS_H
#define DISPLAYIF_MP_HELPERS_H

#include "py/runtime.h"
#include "py/obj.h"

static inline void displayif_pin_set(mp_obj_t pin, int value) {
    mp_obj_t dest[3] = {
        pin,
        MP_OBJ_NEW_QSTR(MP_QSTR_value),
        mp_obj_new_int(value),
    };
    mp_obj_set_attr(pin, MP_OBJ_NEW_QSTR(MP_QSTR_value), dest[2]);
}

static inline int displayif_pin_get(mp_obj_t pin) {
    return mp_obj_get_int(mp_obj_get_attr(pin, MP_OBJ_NEW_QSTR(MP_QSTR_value)));
}

static inline mp_obj_t displayif_machine_pin(mp_int_t pin_id, mp_int_t mode, mp_int_t value) {
    mp_obj_t machine_mod = mp_import_name(MP_QSTR_machine, mp_const_empty_dict, MP_OBJ_NULL);
    mp_obj_t pin_type = mp_obj_get_attr(machine_mod, MP_OBJ_NEW_QSTR(MP_QSTR_Pin));
    mp_obj_t kwargs = mp_dict_new();
    mp_obj_dict_store(kwargs, MP_OBJ_NEW_QSTR(MP_QSTR_mode), mp_obj_new_int(mode));
    mp_obj_dict_store(kwargs, MP_OBJ_NEW_QSTR(MP_QSTR_value), mp_obj_new_int(value));
    return mp_call_function_n_kw(pin_type, 1, 1, (mp_obj_t[]) { mp_obj_new_int(pin_id), kwargs });
}

static inline mp_obj_t displayif_machine_spi(mp_obj_t kwargs) {
    mp_obj_t machine_mod = mp_import_name(MP_QSTR_machine, mp_const_empty_dict, MP_OBJ_NULL);
    mp_obj_t spi_type = mp_obj_get_attr(machine_mod, MP_OBJ_NEW_QSTR(MP_QSTR_SPI));
    mp_obj_t id = mp_obj_dict_get(kwargs, MP_OBJ_NEW_QSTR(MP_QSTR_id));
    if (id == MP_OBJ_NULL) {
        id = mp_obj_new_int(2);
    } else {
        mp_obj_dict_delete(kwargs, MP_OBJ_NEW_QSTR(MP_QSTR_id));
    }
    return mp_call_function_n_kw(spi_type, 1, 1, (mp_obj_t[]) { id, kwargs });
}

#endif /* DISPLAYIF_MP_HELPERS_H */
