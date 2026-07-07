# rp2 CircuitPython sources.
#
# picodvi (PIO/HSTX) is MicroPython-only for now; CP builds get SPI/bus stubs via
# ports/common/circuitpython.mk when sources land.

CFLAGS += -I$(DISPLAYIF_MOD_DIR)/ports/rp2
