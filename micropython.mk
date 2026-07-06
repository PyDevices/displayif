# MicroPython user C module glue for displayif.
#
# Discovered when USER_C_MODULES points at the cmods workspace root.
# Hardware interfaces (spibus, rgbframebuffer, …) build only on MCU ports —
# not unix, windows, or other desktop targets.

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

DISPLAYIF_IS_MCU := 0
ifeq ($(DISPLAYIF_PORT_ESP32),1)
DISPLAYIF_IS_MCU := 1
endif
ifeq ($(DISPLAYIF_PORT_MIMXRT),1)
DISPLAYIF_IS_MCU := 1
endif

ifeq ($(DISPLAYIF_IS_MCU),1)
ifeq ($(DISPLAYIF_PORT_ESP32),1)
ifeq ($(IDF_TARGET),esp32s3)
DISPLAYIF_RGBMATRIX_USE_PROTOMATTER = 1
endif
endif
ifeq ($(DISPLAYIF_PORT_MIMXRT),1)
ifeq ($(MCU_SERIES),MIMXRT1062)
DISPLAYIF_RGBMATRIX_USE_PROTOMATTER = 1
endif
endif
include $(DISPLAYIF_MOD_DIR)/ports/common/micropython.mk
endif

ifeq ($(DISPLAYIF_PORT_ESP32),1)
include $(DISPLAYIF_MOD_DIR)/ports/esp32/micropython.mk
endif

ifeq ($(DISPLAYIF_PORT_MIMXRT),1)
include $(DISPLAYIF_MOD_DIR)/ports/mimxrt/micropython.mk
endif
