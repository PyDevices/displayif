# MicroPython user C module glue for displayif.
#
# Discovered via USER_C_MODULES pointing at the workspace directory that
# contains this repo (its parent), e.g. `make USER_C_MODULES=../../..`.
# Hardware interfaces (spibus, dotclockframebuffer, …) build only on MCU ports.
# Desktop SDL (`usdl2`) builds on unix and windows ports.

DISPLAYIF_MOD_DIR := $(USERMOD_DIR)

PORT_DIR_ABS := $(abspath $(CURDIR))
DISPLAYIF_PORT_ESP32 := $(findstring /ports/esp32,$(PORT_DIR_ABS))
DISPLAYIF_PORT_MIMXRT := $(findstring /ports/mimxrt,$(PORT_DIR_ABS))
DISPLAYIF_PORT_SAMD := $(findstring /ports/samd,$(PORT_DIR_ABS))
DISPLAYIF_PORT_RP2 := $(findstring /ports/rp2,$(PORT_DIR_ABS))
DISPLAYIF_PORT_STM32 := $(findstring /ports/stm32,$(PORT_DIR_ABS))
DISPLAYIF_PORT_UNIX := $(findstring /ports/unix,$(PORT_DIR_ABS))
DISPLAYIF_PORT_WINDOWS := $(findstring /ports/windows,$(PORT_DIR_ABS))

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

ifeq ($(DISPLAYIF_PORT_SAMD),)
DISPLAYIF_PORT_SAMD := 0
else
DISPLAYIF_PORT_SAMD := 1
endif

ifeq ($(DISPLAYIF_PORT_RP2),)
DISPLAYIF_PORT_RP2 := 0
else
DISPLAYIF_PORT_RP2 := 1
endif

ifeq ($(DISPLAYIF_PORT_STM32),)
DISPLAYIF_PORT_STM32 := 0
else
DISPLAYIF_PORT_STM32 := 1
endif

ifeq ($(DISPLAYIF_PORT_UNIX),)
DISPLAYIF_PORT_UNIX := 0
else
DISPLAYIF_PORT_UNIX := 1
endif

ifeq ($(DISPLAYIF_PORT_WINDOWS),)
DISPLAYIF_PORT_WINDOWS := 0
else
DISPLAYIF_PORT_WINDOWS := 1
endif

DISPLAYIF_IS_MCU := 0
ifeq ($(DISPLAYIF_PORT_ESP32),1)
DISPLAYIF_IS_MCU := 1
endif
ifeq ($(DISPLAYIF_PORT_MIMXRT),1)
DISPLAYIF_IS_MCU := 1
endif
ifeq ($(DISPLAYIF_PORT_SAMD),1)
DISPLAYIF_IS_MCU := 1
endif
ifeq ($(DISPLAYIF_PORT_RP2),1)
DISPLAYIF_IS_MCU := 1
endif
ifeq ($(DISPLAYIF_PORT_STM32),1)
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
ifeq ($(DISPLAYIF_PORT_SAMD),1)
ifeq ($(MCU_SERIES),SAMD51)
DISPLAYIF_RGBMATRIX_USE_PROTOMATTER = 1
endif
endif
ifeq ($(DISPLAYIF_PORT_RP2),1)
DISPLAYIF_RGBMATRIX_USE_PROTOMATTER = 1
endif
include $(DISPLAYIF_MOD_DIR)/src/ports/common/micropython.mk
endif

ifeq ($(DISPLAYIF_PORT_ESP32),1)
include $(DISPLAYIF_MOD_DIR)/src/ports/esp32/micropython.mk
endif

ifeq ($(DISPLAYIF_PORT_MIMXRT),1)
include $(DISPLAYIF_MOD_DIR)/src/ports/mimxrt/micropython.mk
endif

ifeq ($(DISPLAYIF_PORT_SAMD),1)
include $(DISPLAYIF_MOD_DIR)/src/ports/samd/micropython.mk
endif

ifeq ($(DISPLAYIF_PORT_RP2),1)
include $(DISPLAYIF_MOD_DIR)/src/ports/rp2/micropython.mk
endif

ifeq ($(DISPLAYIF_PORT_STM32),1)
include $(DISPLAYIF_MOD_DIR)/src/ports/stm32/micropython.mk
endif

# Desktop SDL bindings (`import usdl2`) for SDLDisplay / multimer.
ifneq ($(DISPLAYIF_PORT_UNIX)$(DISPLAYIF_PORT_WINDOWS),00)
include $(DISPLAYIF_MOD_DIR)/src/ports/desktop/usdl2/micropython.mk
endif
