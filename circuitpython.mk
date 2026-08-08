# CircuitPython build glue for displayif usdl2 (unix-only SDL2 subset).
#
# Include from circuitpython/ports/unix/Makefile after setting USDL2_MOD_DIR
# to this repo root (displayif/):
#
#   USDL2_MOD_DIR := $(abspath ../../../displayif)
#   include $(USDL2_MOD_DIR)/circuitpython.mk
#
# Requires libsdl2-dev (pkg-config sdl2). Only include on unix port.
#
# Prefer caller-set USDL2_MOD_DIR. Do not use a lazy MAKEFILE_LIST default for
# QSTR_DEFS (see pygraphics/circuitpython.mk).

ifndef USDL2_MOD_DIR
  USDL2_MOD_DIR := $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
endif

USDL2_DIR := $(USDL2_MOD_DIR)/src/ports/desktop/usdl2

SDL2_CFLAGS := $(shell pkg-config --cflags sdl2 2>/dev/null)
SDL2_LIBS := $(shell pkg-config --libs sdl2 2>/dev/null)

ifeq ($(SDL2_CFLAGS),)
$(error pkg-config sdl2 failed — install libsdl2-dev)
endif

CFLAGS += $(SDL2_CFLAGS) -I$(USDL2_DIR) -DCIRCUITPY_USDL2=1
LDFLAGS += $(SDL2_LIBS)

QSTR_DEFS += $(USDL2_DIR)/usdl2_qstrdefs.h

USDL2_SOURCES := $(USDL2_DIR)/usdl2_mp.c

USDL2_SUPPRESS_CFLAGS := -Wno-sign-compare -Wno-unused-parameter -Wno-shadow
$(foreach _usdl,$(USDL2_SOURCES),$(eval $(BUILD)/$(_usdl:.c=.o): CFLAGS += $(USDL2_SUPPRESS_CFLAGS)))

$(BUILD)/shared-bindings/usdl2/%.o: CFLAGS += $(USDL2_SUPPRESS_CFLAGS)

SRC_C += $(USDL2_SOURCES)
