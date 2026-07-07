# samd port — SAMD51 Protomatter rgbmatrix backend.

CFLAGS_USERMOD += -I$(DISPLAYIF_MOD_DIR)/ports/samd

ifeq ($(MCU_SERIES),SAMD51)
CFLAGS_USERMOD += -D__SAMD51__=1 -Wno-unused-function
SRC_USERMOD_C += \
    $(DISPLAYIF_MOD_DIR)/ports/samd/rgbmatrix_pm.c \
    $(DISPLAYIF_MOD_DIR)/ports/samd/samd_irq_hook.c
endif
