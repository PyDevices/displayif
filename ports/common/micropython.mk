# MCU-shared sources — included only when building an MCU port (see root micropython.mk).

CFLAGS_USERMOD += -I$(DISPLAYIF_MOD_DIR)/include -I$(DISPLAYIF_MOD_DIR)/ports/common -Wno-unused-function

SRC_USERMOD_C += \
    $(DISPLAYIF_MOD_DIR)/ports/common/mp_helpers.c \
    $(DISPLAYIF_MOD_DIR)/ports/common/spi/mod_spibus.c \
    $(DISPLAYIF_MOD_DIR)/ports/common/i2c/mod_i2cbus.c \
    $(DISPLAYIF_MOD_DIR)/ports/common/rgbmatrix/mod_rgbmatrix.c
