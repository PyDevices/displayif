// The one revision string, defined once. See include/displayif/build.h.
#include "displayif/build.h"

#ifndef DISPLAYIF_REVISION
#define DISPLAYIF_REVISION "unknown"
#endif
const MP_DEFINE_STR_OBJ(displayif_revision_obj, DISPLAYIF_REVISION);
