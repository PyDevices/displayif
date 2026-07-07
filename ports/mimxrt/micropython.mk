# mimxrt port — NXP LCD blocks + Protomatter rgbmatrix.

CFLAGS_USERMOD += -I$(DISPLAYIF_MOD_DIR)/ports/mimxrt

SRC_USERMOD_C += \
    $(DISPLAYIF_MOD_DIR)/ports/common/notimpl/mod_rgbframebuffer.c \
    $(DISPLAYIF_MOD_DIR)/ports/common/notimpl/mod_mipidsi.c \
    $(DISPLAYIF_MOD_DIR)/ports/common/notimpl/mod_i80bus.c

ifeq ($(MCU_SERIES),MIMXRT1062)
SRC_USERMOD_C += $(DISPLAYIF_MOD_DIR)/ports/mimxrt/rgbmatrix_pm.c
endif
