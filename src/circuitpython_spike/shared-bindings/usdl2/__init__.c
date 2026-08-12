// Copy to circuitpython/shared-bindings/usdl2/__init__.c
//
// CircuitPython module registration (jpegio pattern). Implementation and binding fun objs live in
// displayif/src/ports/desktop/usdl2/usdl2_mp.c; MicroPython registers there.

#include "py/obj.h"
#include "py/runtime.h"
#include "usdl2.h"

//| """Unix-only SDL2 subset for PyDevices displaydev (linked against libSDL2)."""

static const mp_rom_map_elem_t usdl2_module_globals_table[] = {
#include "usdl2_module_globals.inc"
};

static MP_DEFINE_CONST_DICT(usdl2_module_globals, usdl2_module_globals_table);

const mp_obj_module_t usdl2_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&usdl2_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_usdl2, usdl2_module);
