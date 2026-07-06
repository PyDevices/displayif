# CircuitPython port detection (Make-based CP ports).
#
# Folder names under ports/ follow MicroPython port names (esp32, mimxrt, …).
# CircuitPython uses different port directory names where needed (espressif → esp32).

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
