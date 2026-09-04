# jpegio — CircuitPython-compatible JPEG decoder (TJpgDec R0.03, CP's config).
# Platform-neutral C: included from the root micropython.mk on every port.
#
# The vendored tjpgd/tjpgd.c is the firmware's only TJpgDec. LVGL's own copy
# is off by config (LV_USE_TJPGD 0 in lvgl-bindings' lv_conf.h on MicroPython),
# so when the lvgl-micropython sibling is on the same USER_C_MODULES scan path
# jpegio also compiles lvgl_decoder.c: an LVGL image decoder on this TJpgDec,
# registered through LVGL's public lv_image_decoder_create API (Phase 2 of the
# org's docs/jpegio-vision.md, decision D4: displayif self-detects). Without
# the sibling nothing here changes and the build stays LVGL-less.

JPEGIO_DIR := $(DISPLAYIF_MOD_DIR)/src/jpegio

CFLAGS_USERMOD += -I$(JPEGIO_DIR)/tjpgd

SRC_USERMOD_C += $(JPEGIO_DIR)/jpegio.c

# Library source: no qstrs, so it stays out of the QSTR scan.
SRC_USERMOD_LIB_C += $(JPEGIO_DIR)/tjpgd/tjpgd.c

# Sibling detection is exactly py.mk's usermod glob, one level under
# USER_C_MODULES (works with a relative USER_C_MODULES too). Override with
# JPEGIO_LVGL=0/1 on the make command line.
JPEGIO_LVGL ?= $(if $(wildcard $(USER_C_MODULES)/lvgl-micropython/micropython.mk),1,0)

ifeq ($(JPEGIO_LVGL),1)
# The bindings checkout, derived the way lvgl-micropython/micropython.mk does
# (BINDINGS_DIR ?= $(abspath $(LVMP_DIR)/../lvgl-bindings)) but under a
# displayif-private name: displayif's micropython.mk is included before
# lvgl-micropython's, so a `BINDINGS_DIR ?=` here would pre-empt theirs. A
# BINDINGS_DIR given on the make command line is honoured so both usermods
# compile against the same lv_conf.h (struct layouts depend on it).
JPEGIO_LVGL_BINDINGS_DIR ?= $(or $(BINDINGS_DIR),$(abspath $(USER_C_MODULES)/lvgl-micropython/../lvgl-bindings))
# -I<bindings> finds lv_conf.h (LVGL's lv_conf_internal.h picks it up through
# __has_include, the same route lvgl-micropython's -I$(BINDINGS_DIR) uses);
# -I<bindings>/lvgl finds lvgl.h and the src/... private headers.
CFLAGS_USERMOD += -DJPEGIO_LVGL_DECODER=1 -I$(JPEGIO_LVGL_BINDINGS_DIR) -I$(JPEGIO_LVGL_BINDINGS_DIR)/lvgl
# No qstrs of its own (the Python-visible names live in jpegio.c).
SRC_USERMOD_LIB_C += $(JPEGIO_DIR)/lvgl_decoder.c
# lvgl.h with LV_USE_FLOAT trips -Werror=double-promotion / float-conversion
# on ports that append those after CFLAGS_USERMOD (unix, webassembly): the
# same per-object suppression lvgl-micropython puts on LVGL's own objects.
$(eval $(BUILD)/$(patsubst $(USER_C_MODULES)/%,%,$(JPEGIO_DIR)/lvgl_decoder.o): CFLAGS += -Wno-double-promotion -Wno-float-conversion)
endif
