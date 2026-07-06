# Shared port detection for Make-based MicroPython builds.

PORT_DIR_ABS := $(abspath $(CURDIR))

DISPLAYIF_PORT_ESP32 := $(findstring /ports/esp32,$(PORT_DIR_ABS))
DISPLAYIF_PORT_IMXRT := $(findstring /ports/mimxrt,$(PORT_DIR_ABS))
DISPLAYIF_PORT_RP2 := $(findstring /ports/rp2,$(PORT_DIR_ABS))
DISPLAYIF_PORT_UNIX := $(findstring /ports/unix,$(PORT_DIR_ABS))

# Normalize to 0/1 for ifeq
ifeq ($(DISPLAYIF_PORT_ESP32),)
DISPLAYIF_PORT_ESP32 := 0
else
DISPLAYIF_PORT_ESP32 := 1
endif

ifeq ($(DISPLAYIF_PORT_IMXRT),)
DISPLAYIF_PORT_IMXRT := 0
else
DISPLAYIF_PORT_IMXRT := 1
endif
