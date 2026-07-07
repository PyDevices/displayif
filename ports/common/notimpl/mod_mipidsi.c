// SPDX-License-Identifier: MIT
// Stub mipidsi module for ports without MIPI DSI host hardware.

#include "py/runtime.h"

static mp_obj_t mipidsi_unsupported_make(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    (void)type;
    (void)n_args;
    (void)n_kw;
    (void)args;
    #if defined(DISPLAYIF_STUB_MIPIDSI_MSG)
    mp_raise_msg(&mp_type_NotImplementedError, MP_ERROR_TEXT(DISPLAYIF_STUB_MIPIDSI_MSG));
    #else
    mp_raise_msg(&mp_type_NotImplementedError, MP_ERROR_TEXT("mipidsi not supported on this port"));
    #endif
}

static MP_DEFINE_CONST_OBJ_TYPE(
    mipidsi_bus_type,
    MP_QSTR_Bus,
    MP_TYPE_FLAG_NONE,
    make_new, mipidsi_unsupported_make
);

static MP_DEFINE_CONST_OBJ_TYPE(
    mipidsi_display_type,
    MP_QSTR_Display,
    MP_TYPE_FLAG_NONE,
    make_new, mipidsi_unsupported_make
);

static const mp_rom_map_elem_t mipidsi_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_mipidsi) },
    { MP_ROM_QSTR(MP_QSTR_Bus), MP_ROM_PTR(&mipidsi_bus_type) },
    { MP_ROM_QSTR(MP_QSTR_Display), MP_ROM_PTR(&mipidsi_display_type) },
};
static MP_DEFINE_CONST_DICT(mipidsi_module_globals, mipidsi_module_globals_table);

const mp_obj_module_t mipidsi_user_cmodule = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&mipidsi_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_mipidsi, mipidsi_user_cmodule);
