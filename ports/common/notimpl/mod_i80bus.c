// SPDX-License-Identifier: MIT
// Stub i80bus module for ports without accelerated I80 parallel bus.

#include "py/runtime.h"

static mp_obj_t i80bus_unsupported_make(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    (void)type;
    (void)n_args;
    (void)n_kw;
    (void)args;
    #if defined(DISPLAYIF_STUB_I80BUS_MSG)
    mp_raise_msg(&mp_type_NotImplementedError, MP_ERROR_TEXT(DISPLAYIF_STUB_I80BUS_MSG));
    #else
    mp_raise_msg(&mp_type_NotImplementedError, MP_ERROR_TEXT("i80bus not supported on this port"));
    #endif
}

static MP_DEFINE_CONST_OBJ_TYPE(
    i80bus_type,
    MP_QSTR_I80Bus,
    MP_TYPE_FLAG_NONE,
    make_new, i80bus_unsupported_make
);

static const mp_rom_map_elem_t i80bus_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_i80bus) },
    { MP_ROM_QSTR(MP_QSTR_I80Bus), MP_ROM_PTR(&i80bus_type) },
};
static MP_DEFINE_CONST_DICT(i80bus_module_globals, i80bus_module_globals_table);

const mp_obj_module_t i80bus_user_cmodule = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&i80bus_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_i80bus, i80bus_user_cmodule);
