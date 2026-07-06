# MicroPython user C module glue for displayif.
#
# Discovered when USER_C_MODULES points at the cmods workspace root
# (displayif/micropython.mk).
#
# Port-specific sources are included from mk/*.mk fragments.

DISPLAYIF_MOD_DIR := $(USERMOD_DIR)

include $(DISPLAYIF_MOD_DIR)/mk/detect.mk
include $(DISPLAYIF_MOD_DIR)/mk/common.mk

ifeq ($(DISPLAYIF_PORT_ESP32),1)
include $(DISPLAYIF_MOD_DIR)/mk/esp_idf.mk
endif

ifeq ($(DISPLAYIF_PORT_IMXRT),1)
include $(DISPLAYIF_MOD_DIR)/mk/imxrt.mk
endif
