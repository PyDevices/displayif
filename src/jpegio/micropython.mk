# jpegio — CircuitPython-compatible JPEG decoder (TJpgDec R0.03, CP's config).
# Platform-neutral C: included from the root micropython.mk on every port.
#
# JPEGIO_VENDOR_TJPGD=1 (default) compiles the vendored tjpgd/tjpgd.c. Pass
# JPEGIO_VENDOR_TJPGD=0 when another usermod already links TJpgDec under the
# same tjpgdcnf.h (LVGL, once lvgl-bindings honours JD_FORMAT); jpegio then
# needs only a tjpgd.h on the include path. Nothing detects LVGL yet — that is
# Phase 2 of the org's docs/jpegio-vision.md.

JPEGIO_DIR := $(DISPLAYIF_MOD_DIR)/src/jpegio
JPEGIO_VENDOR_TJPGD ?= 1

CFLAGS_USERMOD += -I$(JPEGIO_DIR)/tjpgd

SRC_USERMOD_C += $(JPEGIO_DIR)/jpegio.c

ifeq ($(JPEGIO_VENDOR_TJPGD),1)
# Library source: no qstrs, so it stays out of the QSTR scan.
SRC_USERMOD_LIB_C += $(JPEGIO_DIR)/tjpgd/tjpgd.c
endif
