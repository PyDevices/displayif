// SPDX-License-Identifier: MIT
// Stub picodvi module for ports without DVI output (or before HSTX/PIO backend lands).

#include "py/runtime.h"

static mp_obj_t picodvi_unsupported_make(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    (void)type;
    (void)n_args;
    (void)n_kw;
    (void)args;
    #if defined(DISPLAYIF_STUB_PICODVI_MSG)
    mp_raise_msg(&mp_type_NotImplementedError, MP_ERROR_TEXT(DISPLAYIF_STUB_PICODVI_MSG));
    #else
    mp_raise_msg(&mp_type_NotImplementedError, MP_ERROR_TEXT("picodvi not supported on this port"));
    #endif
}

static MP_DEFINE_CONST_OBJ_TYPE(
    picodvi_framebuffer_type,
    MP_QSTR_Framebuffer,
    MP_TYPE_FLAG_NONE,
    make_new, picodvi_unsupported_make
);

static const mp_rom_map_elem_t picodvi_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_picodvi) },
    { MP_ROM_QSTR(MP_QSTR_Framebuffer), MP_ROM_PTR(&picodvi_framebuffer_type) },
};
static MP_DEFINE_CONST_DICT(picodvi_module_globals, picodvi_module_globals_table);

const mp_obj_module_t picodvi_user_cmodule = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&picodvi_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_picodvi, picodvi_user_cmodule);
