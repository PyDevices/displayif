# Desktop SDL2 usdl2 for MicroPython unix + windows (included from displayif/micropython.mk).
#
# Linux/unix: libsdl2-dev + pkg-config sdl2
# Windows (MinGW): SDL2 official MinGW development zip (not vendored).
#   export SDL2_DEV=~/SDL2-2.x.x   # unpacked zip root (x86_64-w64-mingw32/ inside)
#   See displayif/tools/sdl2_dev_env.sh and cmods/build_mp.sh.

USDL2_DIR := $(DISPLAYIF_MOD_DIR)/src/ports/desktop/usdl2

PORT_DIR_ABS := $(abspath $(CURDIR))
IS_UNIX_PORT := $(findstring /ports/unix,$(PORT_DIR_ABS))
IS_WINDOWS_PORT := $(findstring /ports/windows,$(PORT_DIR_ABS))

ifneq ($(IS_UNIX_PORT)$(IS_WINDOWS_PORT),)

ifeq ($(IS_WINDOWS_PORT),)
# Unix: system SDL2 via pkg-config.
SDL2_CFLAGS ?= $(shell pkg-config --cflags sdl2 2>/dev/null)
SDL2_LIBS ?= $(shell pkg-config --libs sdl2 2>/dev/null)
else
# Windows: static SDL2 for a self-contained .exe (no SDL2.dll beside the binary).
ifdef SDL2_DEV
  USDL2_SDL2_TRIPLET ?= x86_64-w64-mingw32
  ifneq ($(findstring i686-w64-mingw32,$(CROSS_COMPILE)),)
    USDL2_SDL2_TRIPLET := i686-w64-mingw32
  endif
  USDL2_SDL2_PREFIX := $(SDL2_DEV)/$(USDL2_SDL2_TRIPLET)
  # Console app: no SDL2main / WinMain / -mwindows (micropython already has main()).
  SDL2_CFLAGS := -I$(USDL2_SDL2_PREFIX)/include/SDL2
  SDL2_LIBS := -L$(USDL2_SDL2_PREFIX)/lib \
    $(USDL2_SDL2_PREFIX)/lib/libSDL2.a \
    -lm -ldinput8 -ldxguid -ldxerr8 -luser32 -lgdi32 -lwinmm -limm32 \
    -lole32 -loleaut32 -lshell32 -lsetupapi -lversion -luuid
else
  # Without SDL2_DEV, only a MinGW pkg-config is valid — never host Linux sdl2.pc
  # (Debian's /usr/include/SDL2/SDL_config.h indirection breaks cross-compiles).
  USDL2_MINGW_PKG_CONFIG :=
  ifneq ($(findstring x86_64-w64-mingw32,$(CROSS_COMPILE)),)
    ifneq ($(shell command -v x86_64-w64-mingw32-pkg-config 2>/dev/null),)
      USDL2_MINGW_PKG_CONFIG := x86_64-w64-mingw32-pkg-config
    endif
  else ifneq ($(findstring i686-w64-mingw32,$(CROSS_COMPILE)),)
    ifneq ($(shell command -v i686-w64-mingw32-pkg-config 2>/dev/null),)
      USDL2_MINGW_PKG_CONFIG := i686-w64-mingw32-pkg-config
    endif
  endif
  ifneq ($(USDL2_MINGW_PKG_CONFIG),)
    SDL2_CFLAGS ?= $(shell $(USDL2_MINGW_PKG_CONFIG) --cflags sdl2 2>/dev/null)
    SDL2_LIBS ?= $(shell $(USDL2_MINGW_PKG_CONFIG) --static --libs sdl2 2>/dev/null)
  else
    $(error displayif usdl2 windows port requires SDL2_DEV=path to unpacked SDL2 MinGW development ZIP — see displayif/tools/sdl2_dev_env.sh)
  endif
endif
endif

ifeq ($(SDL2_CFLAGS),)
$(error displayif usdl2 SDL2 not found — unix: install libsdl2-dev; windows: set SDL2_DEV to unpacked SDL2 MinGW dev zip (see displayif/tools/sdl2_dev_env.sh))
endif

CFLAGS_USERMOD += $(SDL2_CFLAGS) -I$(USDL2_DIR) -I$(DISPLAYIF_MOD_DIR)/src/include \
	-Wno-sign-compare -Wno-unused-parameter -Wno-shadow
CFLAGS_USERMOD += -DDISPLAYIF_WRAP_GC_SWEEP=1 -DDISPLAYIF_WRAP_MP_DEINIT=1

# Soft-reset: tear down SDL before gc_sweep_all; mp_deinit is a second pass.
# Desktop ports use gcc-as-linker (same as esp32/rp2).
LDFLAGS_USERMOD += -Wl,--wrap=gc_sweep_all -Wl,--wrap=mp_deinit

# MicroPython only appends LDFLAGS_USERMOD (see py/py.mk); do not use LIBS_USERMOD.
LDFLAGS_USERMOD += $(SDL2_LIBS)
ifeq ($(IS_WINDOWS_PORT),)
else
LDFLAGS_USERMOD += -static-libgcc
endif

SRC_USERMOD_C += \
	$(USDL2_DIR)/usdl2_mp.c \
	$(DISPLAYIF_MOD_DIR)/src/ports/common/soft_reset.c

endif
