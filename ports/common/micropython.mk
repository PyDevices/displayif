# Portable sources — built on every port.

CFLAGS_USERMOD += -I$(DISPLAYIF_MOD_DIR)/include -I$(DISPLAYIF_MOD_DIR)/ports/common -Wno-unused-function

SRC_USERMOD_C += \
    $(DISPLAYIF_MOD_DIR)/ports/common/spi/mod_spibus.c
