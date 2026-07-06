# Universal (port-agnostic) MicroPython sources.

CFLAGS_USERMOD += -I$(DISPLAYIF_MOD_DIR)/include -Wno-unused-function

# Phase 1: SPI display bus — enable when drivers/spi/mod_spi.c lands.
# SRC_USERMOD_C += \
#     $(DISPLAYIF_MOD_DIR)/drivers/spi/mod_spi.c \
#     $(DISPLAYIF_MOD_DIR)/drivers/spi/spi_bus.c
