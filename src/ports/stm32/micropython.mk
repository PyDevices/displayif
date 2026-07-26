# stm32 port — portable spibus/i2cbus (+ rgbmatrix bitbang); no DotClock/MIPI/i80 yet.

CFLAGS_USERMOD += -I$(DISPLAYIF_MOD_DIR)/src/ports/stm32

SRC_USERMOD_C += \
    $(DISPLAYIF_MOD_DIR)/src/ports/common/notimpl/mod_dotclockframebuffer.c \
    $(DISPLAYIF_MOD_DIR)/src/ports/common/notimpl/mod_mipidsi.c \
    $(DISPLAYIF_MOD_DIR)/src/ports/common/notimpl/mod_i80bus.c \
    $(DISPLAYIF_MOD_DIR)/src/ports/common/notimpl/mod_qspibus.c
