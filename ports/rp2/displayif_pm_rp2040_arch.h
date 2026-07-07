// SPDX-License-Identifier: MIT
// RP2040/RP2350 Protomatter arch hooks for displayif (included from rp2040.h).

#pragma once

#if defined(CIRCUITPY) && defined(DISPLAYIF_RGBMATRIX_USE_PROTOMATTER)

#include "py/gc.h"

#define _PM_allocate(x) m_malloc(x)
#define _PM_free(x) m_free(x)

#ifndef F_CPU
#if defined(PICO_RP2350)
#define F_CPU 150000000
#else
#define F_CPU 125000000
#endif
#endif

#endif /* CIRCUITPY && DISPLAYIF_RGBMATRIX_USE_PROTOMATTER */
