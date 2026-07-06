# Universal CircuitPython sources.

CFLAGS += -I$(DISPLAYIF_MOD_DIR)/include -DCIRCUITPY_DISPLAYIF=1

# Phase 1: SPI — enable when CP bindings + drivers land.
# DISPLAYIF_SPI_SOURCES := \
#     $(DISPLAYIF_MOD_DIR)/drivers/spi/spi_bus.c
# SRC_C += $(DISPLAYIF_SPI_SOURCES)
