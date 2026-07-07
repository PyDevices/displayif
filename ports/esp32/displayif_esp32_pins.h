// SPDX-License-Identifier: MIT
// ESP32 GPIO number helpers for displayif esp32 modules.

#pragma once

#include "py/runtime.h"
#include "displayif/mp_helpers.h"
#include "machine_pin.h"

static inline int displayif_esp32_pin_gpio(mp_obj_t pin_or_int) {
    if (mp_obj_is_small_int(pin_or_int) || mp_obj_is_int(pin_or_int)) {
        return mp_obj_get_int(pin_or_int);
    }
    mp_obj_t pin = displayif_pin_resolve(pin_or_int);
    const machine_pin_obj_t *pin_ptr = pin;
    return (int)(pin_ptr - machine_pin_obj_table);
}

static inline size_t displayif_esp32_pin_seq_to_gpios(mp_obj_t seq, int *out, size_t max_out) {
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
        out[i] = displayif_esp32_pin_gpio(items[i]);
    }
    return len;
}
