// SPDX-License-Identifier: MIT
// Compile Adafruit Protomatter with its CIRCUITPY arch paths on MicroPython
// without defining CIRCUITPY for the whole firmware (that breaks lv_bindings
// and any other code that treats CIRCUITPY as "we are CircuitPython").
//
// Also define DISPLAYIF_RGBMATRIX_USE_PROTOMATTER here so qstr preprocess
// (which may not get INTERFACE compile defs) skips CircuitPython Pin.h.
#define CIRCUITPY 1
#ifndef DISPLAYIF_RGBMATRIX_USE_PROTOMATTER
#define DISPLAYIF_RGBMATRIX_USE_PROTOMATTER 1
#endif
#include "core.c"
