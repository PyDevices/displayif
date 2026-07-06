# ESP32 port — ESP-IDF LCD / RGB peripherals.

CFLAGS_USERMOD += -I$(DISPLAYIF_MOD_DIR)/ports/esp32

# Phase 2 — enable when rgbframebuffer.c lands.
# SRC_USERMOD_C += \
#     $(DISPLAYIF_MOD_DIR)/ports/esp32/rgbframebuffer.c
