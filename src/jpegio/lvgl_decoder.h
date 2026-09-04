// SPDX-License-Identifier: MIT
// jpegio's LVGL image decoder (lvgl_decoder.c). Compiled only when the
// lvgl-micropython sibling usermod is on the same build (JPEGIO_LVGL_DECODER).
#ifndef DISPLAYIF_JPEGIO_LVGL_DECODER_H
#define DISPLAYIF_JPEGIO_LVGL_DECODER_H

#include <stdbool.h>
#include <stddef.h>

// The name LVGL reports for it (lv_image_decoder_t.name).
#define JPEGIO_LVGL_DECODER_NAME "jpegio"

// Register jpegio's decoder with the running LVGL (lv_image_decoder_create).
// Idempotent: a second call finds the first registration and adds nothing.
// Returns true when the decoder is registered after the call, false when LVGL
// is not initialised (lv_init() first) or the registration allocation failed.
bool jpegio_lvgl_decoder_register(void);

// True when LVGL is initialised and jpegio's decoder is in its decoder list.
bool jpegio_lvgl_decoder_registered(void);

// Name of the i-th decoder in LVGL's list (the order LVGL consults them),
// NULL past the end or when LVGL is not initialised. Diagnostic for
// jpegio.lvgl_decoders(): the binding cannot walk the list from Python.
const char *jpegio_lvgl_decoder_name_at(size_t i);

#endif
