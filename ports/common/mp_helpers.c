// SPDX-License-Identifier: MIT

#include "displayif/mp_helpers.h"

void displayif_pin_set(mp_obj_t pin, int value) {
    mp_store_attr(pin, MP_QSTR_value, mp_obj_new_int(value));
}

int displayif_pin_get(mp_obj_t pin) {
    return mp_obj_get_int(mp_load_attr(pin, MP_QSTR_value));
}

int displayif_pin_id(mp_obj_t pin_or_int) {
    if (mp_obj_is_small_int(pin_or_int) || mp_obj_is_int(pin_or_int)) {
        return mp_obj_get_int(pin_or_int);
    }
    return mp_obj_get_int(pin_or_int);
}

size_t displayif_pin_tuple_to_ints(mp_obj_t tuple, int *out, size_t max_out) {
    return displayif_pin_seq_to_ints(tuple, out, max_out);
}

size_t displayif_pin_seq_to_ints(mp_obj_t seq, int *out, size_t max_out) {
    size_t len;
    mp_obj_t *items;
    if (mp_obj_is_type(seq, &mp_type_tuple)) {
        mp_obj_tuple_get(seq, &len, &items);
    } else if (mp_obj_is_type(seq, &mp_type_list)) {
        mp_obj_list_get(seq, &len, &items);
    } else {
        mp_raise_TypeError(MP_ERROR_TEXT("expected a sequence of pin numbers"));
    }
    if (len > max_out) {
        mp_raise_ValueError(MP_ERROR_TEXT("too many pins in sequence"));
    }
    for (size_t i = 0; i < len; i++) {
        out[i] = displayif_pin_id(items[i]);
    }
    return len;
}

mp_obj_t displayif_machine_pin(mp_int_t pin_id, mp_int_t mode, mp_int_t value) {
    mp_obj_t machine_mod = mp_import_name(MP_QSTR_machine, DISPLAYIF_EMPTY_DICT, MP_OBJ_NULL);
    mp_obj_t pin_type = mp_load_attr(machine_mod, MP_QSTR_Pin);
    mp_obj_t kwargs = mp_obj_new_dict(0);
    mp_obj_dict_store(kwargs, MP_OBJ_NEW_QSTR(MP_QSTR_mode), mp_obj_new_int(mode));
    mp_obj_dict_store(kwargs, MP_OBJ_NEW_QSTR(MP_QSTR_value), mp_obj_new_int(value));
    return mp_call_function_n_kw(pin_type, 1, 1, (mp_obj_t[]) { mp_obj_new_int(pin_id), kwargs });
}

mp_obj_t displayif_machine_spi(mp_obj_t kwargs) {
    mp_obj_t machine_mod = mp_import_name(MP_QSTR_machine, DISPLAYIF_EMPTY_DICT, MP_OBJ_NULL);
    mp_obj_t spi_type = mp_load_attr(machine_mod, MP_QSTR_SPI);
    mp_obj_t id = mp_obj_dict_get(kwargs, MP_OBJ_NEW_QSTR(MP_QSTR_id));
    if (id == MP_OBJ_NULL) {
        id = mp_obj_new_int(2);
    } else {
        mp_obj_dict_delete(kwargs, MP_OBJ_NEW_QSTR(MP_QSTR_id));
    }
    return mp_call_function_n_kw(spi_type, 1, 1, (mp_obj_t[]) { id, kwargs });
}

mp_obj_t displayif_obj_call_method0(mp_obj_t obj, qstr method) {
    return mp_call_function_n_kw(mp_load_attr(obj, method), 0, 0, NULL);
}

mp_obj_t displayif_obj_call_method1(mp_obj_t obj, qstr method, mp_obj_t arg) {
    return mp_call_function_n_kw(mp_load_attr(obj, method), 1, 0, &arg);
}

mp_obj_t displayif_obj_call_method_kw(mp_obj_t obj, qstr method, mp_obj_t kwargs) {
    return mp_call_function_n_kw(mp_load_attr(obj, method), 0, 1, &kwargs);
}
