# ESP32 port — ESP-IDF LCD / RGB peripherals.

CFLAGS_USERMOD += -I$(DISPLAYIF_MOD_DIR)/ports/esp32 -Wno-unused-function

SRC_USERMOD_C += \
    $(DISPLAYIF_MOD_DIR)/ports/esp32/mod_rgbframebuffer.c \
    $(DISPLAYIF_MOD_DIR)/ports/esp32/mod_mipidsi.c \
    $(DISPLAYIF_MOD_DIR)/ports/esp32/mod_i80bus.c

ifeq ($(IDF_TARGET),esp32s3)
DISPLAYIF_RGBMATRIX_USE_PROTOMATTER = 1
CFLAGS_USERMOD += -DESP_PLATFORM=1
SRC_USERMOD_C += $(DISPLAYIF_MOD_DIR)/ports/esp32/rgbmatrix_pm.c
endif
