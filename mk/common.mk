# Portable sources — built on every port (ports/common/).

CFLAGS_USERMOD += -I$(DISPLAYIF_MOD_DIR)/include -I$(DISPLAYIF_MOD_DIR)/ports/common

# Phase 1: SPI display bus — enable when ports/common/spi/mod_spi.c lands.
# SRC_USERMOD_C += \
#     $(DISPLAYIF_MOD_DIR)/ports/common/spi/mod_spi.c \
#     $(DISPLAYIF_MOD_DIR)/ports/common/spi/spi_bus.c
