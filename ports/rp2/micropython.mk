# rp2 port — Protomatter rgbmatrix + PIO i80bus + PicoDVI backends.

CFLAGS_USERMOD += -I$(DISPLAYIF_MOD_DIR)/ports/rp2
CFLAGS_USERMOD += -I$(DISPLAYIF_MOD_DIR)/ports/rp2/picodvi/libdvi

PICODVI_LIBDVI_SRC = \
    $(DISPLAYIF_MOD_DIR)/ports/rp2/picodvi/libdvi/dvi.c \
    $(DISPLAYIF_MOD_DIR)/ports/rp2/picodvi/libdvi/dvi_serialiser.c \
    $(DISPLAYIF_MOD_DIR)/ports/rp2/picodvi/libdvi/dvi_timing.c \
    $(DISPLAYIF_MOD_DIR)/ports/rp2/picodvi/libdvi/tmds_encode.c \
    $(DISPLAYIF_MOD_DIR)/ports/rp2/picodvi/libdvi/tmds_encode.S

SRC_USERMOD_C += \
    $(DISPLAYIF_MOD_DIR)/ports/rp2/rgbmatrix_pm.c \
    $(DISPLAYIF_MOD_DIR)/ports/rp2/mod_i80bus.c \
    $(DISPLAYIF_MOD_DIR)/ports/common/notimpl/mod_rgbframebuffer.c \
    $(DISPLAYIF_MOD_DIR)/ports/common/notimpl/mod_mipidsi.c

SRC_USERMOD_C := $(filter-out $(DISPLAYIF_MOD_DIR)/ports/common/notimpl/mod_picodvi.c,$(SRC_USERMOD_C))
SRC_USERMOD_C += $(DISPLAYIF_MOD_DIR)/ports/rp2/mod_picodvi.c

ifeq ($(PICODVI_RP2350),1)
SRC_USERMOD_C += $(DISPLAYIF_MOD_DIR)/ports/rp2/picodvi_rp2350.c
else
SRC_USERMOD_C += \
    $(DISPLAYIF_MOD_DIR)/ports/rp2/picodvi_rp2040.c \
    $(PICODVI_LIBDVI_SRC)
endif
