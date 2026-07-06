# Portable CircuitPython sources.

CFLAGS += -I$(DISPLAYIF_MOD_DIR)/include -I$(DISPLAYIF_MOD_DIR)/ports/common -DCIRCUITPY_DISPLAYIF=1

# Phase 1: SPI — enable when sources land.
# DISPLAYIF_SPI_SOURCES := \
#     $(DISPLAYIF_MOD_DIR)/ports/common/spi/spi_bus.c
# SRC_C += $(DISPLAYIF_SPI_SOURCES)
