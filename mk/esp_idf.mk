# ESP-IDF-only MicroPython sources (ESP32-S3, ESP32-P4, …).

CFLAGS_USERMOD += -I$(DISPLAYIF_MOD_DIR)/ports/esp_idf

# Phase 2: RGB565 / RGB666 — enable when ports/esp_idf/*.c land.
# SRC_USERMOD_C += \
#     $(DISPLAYIF_MOD_DIR)/ports/esp_idf/rgb565_panel.c \
#     $(DISPLAYIF_MOD_DIR)/ports/esp_idf/rgb_framebuffer.c
