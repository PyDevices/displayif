# mimxrt port — NXP LCD blocks + Protomatter rgbmatrix.

DISPLAYIF_NXP_SDK ?= $(TOP)/lib/nxp_driver/sdk

CFLAGS_USERMOD += -I$(DISPLAYIF_MOD_DIR)/ports/mimxrt

SRC_USERMOD_C += \
    $(DISPLAYIF_MOD_DIR)/ports/common/notimpl/mod_rgbframebuffer.c \
    $(DISPLAYIF_MOD_DIR)/ports/common/notimpl/mod_mipidsi.c \
    $(DISPLAYIF_MOD_DIR)/ports/common/notimpl/mod_i80bus.c

ifeq ($(MCU_SERIES),MIMXRT1062)
SRC_USERMOD_C := $(filter-out $(DISPLAYIF_MOD_DIR)/ports/common/notimpl/mod_rgbframebuffer.c,$(SRC_USERMOD_C))
SRC_USERMOD_C += \
    $(DISPLAYIF_MOD_DIR)/ports/mimxrt/mod_rgbframebuffer_elcdif.c \
    $(DISPLAYIF_MOD_DIR)/ports/mimxrt/mimxrt1060_lcd_pins.c
CFLAGS_USERMOD += -I$(DISPLAYIF_NXP_SDK)/drivers/elcdif
SRC_USERMOD_C += $(DISPLAYIF_NXP_SDK)/drivers/elcdif/fsl_elcdif.c
endif

ifeq ($(MCU_SERIES),MIMXRT1176)
CFLAGS += -Wno-maybe-uninitialized -Wno-error=double-promotion -Wno-error=float-conversion
SRC_USERMOD_C := $(filter-out $(DISPLAYIF_MOD_DIR)/ports/common/notimpl/mod_mipidsi.c,$(SRC_USERMOD_C))
SRC_USERMOD_C += \
    $(DISPLAYIF_MOD_DIR)/ports/mimxrt/mod_mipidsi.c \
    $(DISPLAYIF_MOD_DIR)/ports/mimxrt/mimxrt1176_dsi_display.c
CFLAGS_USERMOD += \
    -I$(DISPLAYIF_NXP_SDK)/drivers/mipi_dsi_split \
    -I$(DISPLAYIF_NXP_SDK)/drivers/lcdifv2 \
    -I$(DISPLAYIF_NXP_SDK)/devices/MIMXRT1176/drivers \
    -Wno-maybe-uninitialized
SRC_USERMOD_C += \
    $(DISPLAYIF_NXP_SDK)/drivers/mipi_dsi_split/fsl_mipi_dsi.c \
    $(DISPLAYIF_NXP_SDK)/drivers/lcdifv2/fsl_lcdifv2.c
endif

ifeq ($(MCU_SERIES),MIMXRT1062)
SRC_USERMOD_C := $(filter-out $(DISPLAYIF_MOD_DIR)/ports/common/notimpl/mod_i80bus.c,$(SRC_USERMOD_C))
SRC_USERMOD_C += \
    $(DISPLAYIF_MOD_DIR)/ports/mimxrt/mod_i80bus.c \
    $(DISPLAYIF_NXP_SDK)/drivers/flexio/mculcd/fsl_flexio_mculcd.c \
    $(DISPLAYIF_NXP_SDK)/drivers/flexio/fsl_flexio.c
CFLAGS_USERMOD += \
    -DFLEXIO_MCULCD_DATA_BUS_WIDTH=8 \
    -I$(DISPLAYIF_NXP_SDK)/drivers/flexio/mculcd \
    -I$(DISPLAYIF_NXP_SDK)/drivers/flexio
endif

ifeq ($(MCU_SERIES),MIMXRT1062)
SRC_USERMOD_C += $(DISPLAYIF_MOD_DIR)/ports/mimxrt/rgbmatrix_pm.c
endif
