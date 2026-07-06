# i.MX RT-only MicroPython sources (mimxrt port).

CFLAGS_USERMOD += -I$(DISPLAYIF_MOD_DIR)/ports/imxrt

# Add imxrt-specific SRC_USERMOD_C when drivers exist.
