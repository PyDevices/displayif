# samd port — SAMD51 Protomatter rgbmatrix + GPIO bit-bang i80bus.

CFLAGS_USERMOD += -I$(DISPLAYIF_MOD_DIR)/src/ports/samd
CFLAGS_USERMOD += -I$(DISPLAYIF_MOD_DIR)/src/include

SRC_USERMOD_C += \
    $(DISPLAYIF_MOD_DIR)/src/ports/common/notimpl/mod_dotclockframebuffer.c \
    $(DISPLAYIF_MOD_DIR)/src/ports/common/notimpl/mod_mipidsi.c \
    $(DISPLAYIF_MOD_DIR)/src/ports/common/notimpl/mod_qspibus.c \
    $(DISPLAYIF_MOD_DIR)/src/ports/common/i80bus/gpio_bitbang.c

ifeq ($(MCU_SERIES),SAMD51)
CFLAGS_USERMOD += -D__SAMD51__=1 -Wno-unused-function
SRC_USERMOD_C += \
    $(DISPLAYIF_MOD_DIR)/src/ports/samd/mod_i80bus.c \
    $(DISPLAYIF_MOD_DIR)/src/ports/samd/rgbmatrix_pm.c \
    $(DISPLAYIF_MOD_DIR)/src/ports/samd/samd_irq_hook.c
else
SRC_USERMOD_C += \
    $(DISPLAYIF_MOD_DIR)/src/ports/common/notimpl/mod_i80bus.c
endif
