# ESP32 port — ESP-IDF LCD / RGB peripherals.

CFLAGS_USERMOD += -I$(DISPLAYIF_MOD_DIR)/ports/esp32 -Wno-unused-function
# Soft-reset: common wraps gc_sweep_all; soft_reset_gc_sweep.c = timer pre-hook.

SRC_USERMOD_C += \
    $(DISPLAYIF_MOD_DIR)/ports/esp32/mod_rgbframebuffer.c \
    $(DISPLAYIF_MOD_DIR)/ports/esp32/mod_mipidsi.c \
    $(DISPLAYIF_MOD_DIR)/ports/esp32/mod_i80bus.c \
    $(DISPLAYIF_MOD_DIR)/ports/esp32/soft_reset_gc_sweep.c

ifeq ($(IDF_TARGET),esp32s3)
DISPLAYIF_RGBMATRIX_USE_PROTOMATTER = 1
CFLAGS_USERMOD += -DESP_PLATFORM=1
SRC_USERMOD_C += $(DISPLAYIF_MOD_DIR)/ports/esp32/rgbmatrix_pm.c
endif
