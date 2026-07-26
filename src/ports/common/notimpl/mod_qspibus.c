// SPDX-License-Identifier: MIT
// Stub qspibus module for ports without accelerated QSPI display bus.

#include "py/runtime.h"

static mp_obj_t qspibus_unsupported_make(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    (void)type;
    (void)n_args;
    (void)n_kw;
    (void)args;
    #if defined(DISPLAYIF_STUB_QSPIBUS_MSG)
    mp_raise_msg(&mp_type_NotImplementedError, MP_ERROR_TEXT(DISPLAYIF_STUB_QSPIBUS_MSG));
    #else
    mp_raise_msg(&mp_type_NotImplementedError, MP_ERROR_TEXT("qspibus not supported on this port"));
    #endif
}

static mp_obj_t qspibus_deinit(mp_obj_t self_in) {
    (void)self_in;
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(qspibus_deinit_obj, qspibus_deinit);

static const mp_rom_map_elem_t qspibus_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_deinit), MP_ROM_PTR(&qspibus_deinit_obj) },
    { MP_ROM_QSTR(MP_QSTR___del__), MP_ROM_PTR(&qspibus_deinit_obj) },
};
static MP_DEFINE_CONST_DICT(qspibus_locals_dict, qspibus_locals_dict_table);

static MP_DEFINE_CONST_OBJ_TYPE(
    qspibus_type,
    MP_QSTR_QSPIBus,
    MP_TYPE_FLAG_NONE,
    make_new, qspibus_unsupported_make,
    locals_dict, &qspibus_locals_dict
);

static const mp_rom_map_elem_t qspibus_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_qspibus) },
    { MP_ROM_QSTR(MP_QSTR_QSPIBus), MP_ROM_PTR(&qspibus_type) },
};
static MP_DEFINE_CONST_DICT(qspibus_module_globals, qspibus_module_globals_table);

const mp_obj_module_t qspibus_user_cmodule = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&qspibus_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_qspibus, qspibus_user_cmodule);
