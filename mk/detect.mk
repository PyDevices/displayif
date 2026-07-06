# Shared port detection for Make-based MicroPython builds.

PORT_DIR_ABS := $(abspath $(CURDIR))

DISPLAYIF_PORT_ESP32 := $(findstring /ports/esp32,$(PORT_DIR_ABS))
DISPLAYIF_PORT_MIMXRT := $(findstring /ports/mimxrt,$(PORT_DIR_ABS))
DISPLAYIF_PORT_RP2 := $(findstring /ports/rp2,$(PORT_DIR_ABS))
DISPLAYIF_PORT_UNIX := $(findstring /ports/unix,$(PORT_DIR_ABS))

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
