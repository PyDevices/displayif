// SPDX-License-Identifier: MIT

#include "displayif/mp_helpers.h"

static mp_obj_t displayif_pin_type(void) {
    mp_obj_t machine_mod = mp_import_name(MP_QSTR_machine, DISPLAYIF_EMPTY_DICT, MP_OBJ_NULL);
    return mp_load_attr(machine_mod, MP_QSTR_Pin);
}

bool displayif_obj_is_pin(mp_obj_t obj) {
    return mp_obj_is_type(obj, displayif_pin_type());
}

mp_obj_t displayif_pin_resolve(mp_obj_t pin_or_int) {
    if (displayif_obj_is_pin(pin_or_int)) {
        return pin_or_int;
    }
    if (mp_obj_is_small_int(pin_or_int) || mp_obj_is_int(pin_or_int)) {
        mp_obj_t pin_cls = displayif_pin_type();
        mp_int_t pin_out = mp_obj_get_int(mp_load_attr(pin_cls, MP_QSTR_OUT));
        return displayif_machine_pin(mp_obj_get_int(pin_or_int), pin_out, 0);
    }
    if (mp_obj_is_str(pin_or_int)) {
        mp_obj_t pin_cls = displayif_pin_type();
        return mp_call_function_n_kw(pin_cls, 1, 0, &pin_or_int);
    }
    mp_raise_TypeError(MP_ERROR_TEXT("expected pin number or Pin"));
}

void displayif_pin_set(mp_obj_t pin, int value) {
    // machine.Pin exposes value as a method (pin.value(n)), not an attribute.
    displayif_obj_call_method1(pin, MP_QSTR_value, mp_obj_new_int(value));
}

int displayif_pin_get(mp_obj_t pin) {
    return mp_obj_get_int(displayif_obj_call_method0(pin, MP_QSTR_value));
}

int displayif_pin_id(mp_obj_t pin_or_int) {
    if (mp_obj_is_small_int(pin_or_int) || mp_obj_is_int(pin_or_int)) {
        return mp_obj_get_int(pin_or_int);
    }
    mp_raise_TypeError(MP_ERROR_TEXT("expected pin number"));
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
        mp_raise_TypeError(MP_ERROR_TEXT("expected a sequence of pins"));
    }
    if (len > max_out) {
        mp_raise_ValueError(MP_ERROR_TEXT("too many pins in sequence"));
    }
    for (size_t i = 0; i < len; i++) {
        out[i] = displayif_pin_id(items[i]);
    }
    return len;
}

size_t displayif_pin_seq_to_objs(mp_obj_t seq, mp_obj_t *out, size_t max_out) {
    size_t len;
    mp_obj_t *items;
    if (mp_obj_is_type(seq, &mp_type_tuple)) {
        mp_obj_tuple_get(seq, &len, &items);
    } else if (mp_obj_is_type(seq, &mp_type_list)) {
        mp_obj_list_get(seq, &len, &items);
    } else {
        mp_raise_TypeError(MP_ERROR_TEXT("expected a sequence of pins"));
    }
    if (len > max_out) {
        mp_raise_ValueError(MP_ERROR_TEXT("too many pins in sequence"));
    }
    for (size_t i = 0; i < len; i++) {
        out[i] = displayif_pin_resolve(items[i]);
    }
    return len;
}

// mp_call_function_n_kw expects: pos..., key0, val0, key1, val1, ...
// Expand a kwargs dict into that interleaved form.
static mp_obj_t displayif_call_pos_kwdict(mp_obj_t fun, size_t n_pos, const mp_obj_t *pos, mp_obj_t kwargs_in) {
    mp_map_t *map = mp_obj_dict_get_map(kwargs_in);
    size_t n_kw = map->used;
    size_t n_total = n_pos + 2 * n_kw;
    mp_obj_t *args = mp_local_alloc(n_total * sizeof(mp_obj_t));
    for (size_t i = 0; i < n_pos; i++) {
        args[i] = pos[i];
    }
    size_t j = n_pos;
    for (size_t i = 0; i < map->alloc; i++) {
        if (mp_map_slot_is_filled(map, i)) {
            args[j++] = map->table[i].key;
            args[j++] = map->table[i].value;
        }
    }
    mp_obj_t res = mp_call_function_n_kw(fun, n_pos, n_kw, args);
    mp_local_free(args);
    return res;
}

mp_obj_t displayif_machine_pin(mp_int_t pin_id, mp_int_t mode, mp_int_t value) {
    // Match pydisplay: Pin(id, mode, value=value)
    mp_obj_t pin_type = displayif_pin_type();
    mp_obj_t pos[2] = {
        mp_obj_new_int(pin_id),
        mp_obj_new_int(mode),
    };
    mp_obj_t kwargs = mp_obj_new_dict(0);
    mp_obj_dict_store(kwargs, MP_OBJ_NEW_QSTR(MP_QSTR_value), mp_obj_new_int(value));
    return displayif_call_pos_kwdict(pin_type, 2, pos, kwargs);
}

mp_obj_t displayif_machine_spi(mp_obj_t kwargs) {
    // Match pydisplay SPIBus: SPI(id, baudrate=..., sck=..., ...)
    mp_obj_t machine_mod = mp_import_name(MP_QSTR_machine, DISPLAYIF_EMPTY_DICT, MP_OBJ_NULL);
    mp_obj_t spi_type = mp_load_attr(machine_mod, MP_QSTR_SPI);
    mp_map_elem_t *elem = mp_map_lookup(mp_obj_dict_get_map(kwargs), MP_OBJ_NEW_QSTR(MP_QSTR_id), MP_MAP_LOOKUP);
    mp_obj_t id = (elem != NULL) ? elem->value : mp_obj_new_int(2);
    if (elem != NULL) {
        mp_obj_dict_delete(kwargs, MP_OBJ_NEW_QSTR(MP_QSTR_id));
    }
    return displayif_call_pos_kwdict(spi_type, 1, &id, kwargs);
}

mp_obj_t displayif_obj_call_method0(mp_obj_t obj, qstr method) {
    return mp_call_function_n_kw(mp_load_attr(obj, method), 0, 0, NULL);
}

mp_obj_t displayif_obj_call_method1(mp_obj_t obj, qstr method, mp_obj_t arg) {
    return mp_call_function_n_kw(mp_load_attr(obj, method), 1, 0, &arg);
}

mp_obj_t displayif_obj_call_method_kw(mp_obj_t obj, qstr method, mp_obj_t kwargs) {
    return displayif_call_pos_kwdict(mp_load_attr(obj, method), 0, NULL, kwargs);
}
