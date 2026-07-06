# ESP32 port only (ports/esp32/) — ESP-IDF LCD / RGB peripherals.

CFLAGS_USERMOD += -I$(DISPLAYIF_MOD_DIR)/ports/esp32

# Phase 2 — enable when sources land.
# SRC_USERMOD_C += \
#     $(DISPLAYIF_MOD_DIR)/ports/esp32/rgb_panel.c \
#     $(DISPLAYIF_MOD_DIR)/ports/esp32/rgb666.c
