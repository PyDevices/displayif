# samd port — SAMD51 Protomatter rgbmatrix + API-parity stubs.

CFLAGS_USERMOD += -I$(DISPLAYIF_MOD_DIR)/ports/samd

SRC_USERMOD_C += \
    $(DISPLAYIF_MOD_DIR)/ports/common/notimpl/mod_rgbframebuffer.c \
    $(DISPLAYIF_MOD_DIR)/ports/common/notimpl/mod_mipidsi.c \
    $(DISPLAYIF_MOD_DIR)/ports/common/notimpl/mod_i80bus.c

ifeq ($(MCU_SERIES),SAMD51)
CFLAGS_USERMOD += -D__SAMD51__=1 -Wno-unused-function
SRC_USERMOD_C += \
    $(DISPLAYIF_MOD_DIR)/ports/samd/rgbmatrix_pm.c \
    $(DISPLAYIF_MOD_DIR)/ports/samd/samd_irq_hook.c
endif
