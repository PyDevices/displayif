# CircuitPython build glue for displayif.
#
# Include from a CircuitPython port Makefile after setting DISPLAYIF_MOD_DIR:
#
#   DISPLAYIF_MOD_DIR := $(abspath ../../../displayif)
#   include $(DISPLAYIF_MOD_DIR)/circuitpython.mk
#
# Patched in by apply_cp_displayif_patches.sh (graphics/usdl2 pattern).

DISPLAYIF_MOD_DIR ?= $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))

include $(DISPLAYIF_MOD_DIR)/mk/detect_cp.mk
include $(DISPLAYIF_MOD_DIR)/mk/common_cp.mk

ifeq ($(DISPLAYIF_PORT_ESP32),1)
include $(DISPLAYIF_MOD_DIR)/mk/esp_idf_cp.mk
endif

ifeq ($(DISPLAYIF_PORT_IMXRT),1)
include $(DISPLAYIF_MOD_DIR)/mk/imxrt_cp.mk
endif
