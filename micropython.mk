# MicroPython user C module glue for displayif.
#
# Discovered when USER_C_MODULES points at the cmods workspace root.
# Includes ports/common/ always, then ports/<port>/ when the active MP port matches.

DISPLAYIF_MOD_DIR := $(USERMOD_DIR)

PORT_DIR_ABS := $(abspath $(CURDIR))
DISPLAYIF_PORT_ESP32 := $(findstring /ports/esp32,$(PORT_DIR_ABS))
DISPLAYIF_PORT_MIMXRT := $(findstring /ports/mimxrt,$(PORT_DIR_ABS))

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

include $(DISPLAYIF_MOD_DIR)/ports/common/micropython.mk

ifeq ($(DISPLAYIF_PORT_ESP32),1)
include $(DISPLAYIF_MOD_DIR)/ports/esp32/micropython.mk
endif

ifeq ($(DISPLAYIF_PORT_MIMXRT),1)
include $(DISPLAYIF_MOD_DIR)/ports/mimxrt/micropython.mk
endif
