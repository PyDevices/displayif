# mimxrt port — NXP LCD blocks.

CFLAGS_USERMOD += -I$(DISPLAYIF_MOD_DIR)/ports/mimxrt

ifeq ($(MCU_SERIES),MIMXRT1062)
SRC_USERMOD_C += $(DISPLAYIF_MOD_DIR)/ports/mimxrt/rgbmatrix_pm.c
endif
