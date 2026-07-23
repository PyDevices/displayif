# MCU-shared sources — included only when building an MCU port (see root micropython.mk).

CFLAGS_USERMOD += -I$(DISPLAYIF_MOD_DIR)/include -I$(DISPLAYIF_MOD_DIR)/ports/common -Wno-unused-function
CFLAGS_USERMOD += -DDISPLAYIF_WRAP_GC_SWEEP=1 -DDISPLAYIF_WRAP_MP_DEINIT=1
# Soft-reset: tear down before gc_sweep_all; mp_deinit is a second pass.
# Bare `ld` (mimxrt/samd) rejects -Wl,; gcc-as-linker (esp32/rp2/stm32) wants it.
ifneq ($(findstring /ports/mimxrt,$(CURDIR))$(findstring /ports/samd,$(CURDIR)),)
LDFLAGS_USERMOD += --wrap=gc_sweep_all --wrap=mp_deinit
else
LDFLAGS_USERMOD += -Wl,--wrap=gc_sweep_all -Wl,--wrap=mp_deinit
endif

SRC_USERMOD_C += \
    $(DISPLAYIF_MOD_DIR)/ports/common/mp_helpers.c \
    $(DISPLAYIF_MOD_DIR)/ports/common/soft_reset.c \
    $(DISPLAYIF_MOD_DIR)/ports/common/spi/mod_spibus.c \
    $(DISPLAYIF_MOD_DIR)/ports/common/i2c/mod_i2cbus.c \
    $(DISPLAYIF_MOD_DIR)/ports/common/rgbmatrix/mod_rgbmatrix.c

ifeq ($(DISPLAYIF_RGBMATRIX_USE_PROTOMATTER),1)
CFLAGS_USERMOD += -I$(DISPLAYIF_MOD_DIR)/ports/common/rgbmatrix/protomatter -DDISPLAYIF_RGBMATRIX_USE_PROTOMATTER=1
# CIRCUITPY is scoped inside protomatter_mp.c — do not set it CFLAGS-wide.
SRC_USERMOD_C += $(DISPLAYIF_MOD_DIR)/ports/common/rgbmatrix/protomatter_mp.c
endif
