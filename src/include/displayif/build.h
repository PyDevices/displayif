// Which displayif this firmware was built from, exposed as __revision__ on
// every module this repo registers. The build passes DISPLAYIF_REVISION
// (git describe) as a compile definition; a build with no git says "unknown".
#pragma once
#include "py/obj.h"
#include "py/objstr.h"

extern const mp_obj_str_t displayif_revision_obj;

#define DISPLAYIF_REVISION_ENTRY \
    { MP_ROM_QSTR(MP_QSTR___revision__), MP_ROM_PTR(&displayif_revision_obj) }
