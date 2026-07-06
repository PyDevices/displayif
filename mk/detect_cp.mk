# CircuitPython port detection (Make-based CP ports).

CP_PORT_DIR_ABS := $(abspath $(CURDIR))

DISPLAYIF_PORT_ESP32 := $(findstring /ports/espressif,$(CP_PORT_DIR_ABS))
DISPLAYIF_PORT_IMXRT := $(findstring /ports/mimxrt,$(CP_PORT_DIR_ABS))

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
