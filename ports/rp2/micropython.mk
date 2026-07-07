# rp2 port — Protomatter rgbmatrix + PIO i80bus backends.

CFLAGS_USERMOD += -I$(DISPLAYIF_MOD_DIR)/ports/rp2

SRC_USERMOD_C += \
    $(DISPLAYIF_MOD_DIR)/ports/rp2/rgbmatrix_pm.c \
    $(DISPLAYIF_MOD_DIR)/ports/rp2/mod_i80bus.c \
    $(DISPLAYIF_MOD_DIR)/ports/common/notimpl/mod_rgbframebuffer.c \
    $(DISPLAYIF_MOD_DIR)/ports/common/notimpl/mod_mipidsi.c \
    $(DISPLAYIF_MOD_DIR)/ports/common/notimpl/mod_picodvi.c
