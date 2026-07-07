# rp2 port — Protomatter rgbmatrix backend (CMake is primary; this mirrors other ports).

CFLAGS_USERMOD += -I$(DISPLAYIF_MOD_DIR)/ports/rp2

SRC_USERMOD_C += $(DISPLAYIF_MOD_DIR)/ports/rp2/rgbmatrix_pm.c
