# ESP32 port — ESP-IDF LCD / RGB peripherals.

CFLAGS_USERMOD += -I$(DISPLAYIF_MOD_DIR)/ports/esp32 -Wno-unused-function

SRC_USERMOD_C += \
    $(DISPLAYIF_MOD_DIR)/ports/esp32/mod_rgbframebuffer.c
