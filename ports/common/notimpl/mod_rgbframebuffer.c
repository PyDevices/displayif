// SPDX-License-Identifier: MIT
// Stub rgbframebuffer module for ports without accelerated RGB scanout.

#include "py/runtime.h"

static mp_obj_t rgbframebuffer_unsupported_make(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    (void)type;
    (void)n_args;
    (void)n_kw;
    (void)args;
    #if defined(DISPLAYIF_STUB_RGBFRAMEBUFFER_MSG)
    mp_raise_msg(&mp_type_NotImplementedError, MP_ERROR_TEXT(DISPLAYIF_STUB_RGBFRAMEBUFFER_MSG));
    #else
    mp_raise_msg(&mp_type_NotImplementedError, MP_ERROR_TEXT("rgbframebuffer not supported on this port"));
    #endif
}

static MP_DEFINE_CONST_OBJ_TYPE(
    rgbframebuffer_type,
    MP_QSTR_RGBFrameBuffer,
    MP_TYPE_FLAG_NONE,
    make_new, rgbframebuffer_unsupported_make
);

static const mp_rom_map_elem_t rgbframebuffer_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_rgbframebuffer) },
    { MP_ROM_QSTR(MP_QSTR_RGBFrameBuffer), MP_ROM_PTR(&rgbframebuffer_type) },
};
static MP_DEFINE_CONST_DICT(rgbframebuffer_module_globals, rgbframebuffer_module_globals_table);

const mp_obj_module_t rgbframebuffer_user_cmodule = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&rgbframebuffer_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_rgbframebuffer, rgbframebuffer_user_cmodule);
