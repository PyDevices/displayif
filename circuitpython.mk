# CircuitPython build glue for displayif.
#
# Include from a CircuitPython port Makefile after setting DISPLAYIF_MOD_DIR:
#
#   DISPLAYIF_MOD_DIR := $(abspath ../../../displayif)
#   include $(DISPLAYIF_MOD_DIR)/circuitpython.mk
#
# Patched in by apply_cp_displayif_patches.sh (graphics/usdl2 pattern).
# Folder names under ports/ follow MP port names; CP port dirs may differ
# (e.g. espressif → ports/esp32/).

DISPLAYIF_MOD_DIR ?= $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))

CP_PORT_DIR_ABS := $(abspath $(CURDIR))

DISPLAYIF_PORT_ESP32 := $(findstring /ports/espressif,$(CP_PORT_DIR_ABS))
DISPLAYIF_PORT_MIMXRT := $(findstring /ports/mimxrt,$(CP_PORT_DIR_ABS))

ifeq ($(DISPLAYIF_PORT_ESP32),)
DISPLAYIF_PORT_ESP32 := 0
else
DISPLAYIF_PORT_ESP32 := 1
endif

ifeq ($(DISPLAYIF_PORT_MIMXRT),)
DISPLAYIF_PORT_MIMXRT := 0
else
DISPLAYIF_PORT_MIMXRT := 1
endif

include $(DISPLAYIF_MOD_DIR)/ports/common/circuitpython.mk

ifeq ($(DISPLAYIF_PORT_ESP32),1)
include $(DISPLAYIF_MOD_DIR)/ports/esp32/circuitpython.mk
endif

ifeq ($(DISPLAYIF_PORT_MIMXRT),1)
include $(DISPLAYIF_MOD_DIR)/ports/mimxrt/circuitpython.mk
endif
