# ports/

All interface code and **per-port build glue** live here. Folder names match **MicroPython** `ports/<name>/`.

| Directory | When included | MP port |
|-----------|---------------|---------|
| `common/` | MCU builds only | esp32, mimxrt, samd, rp2, … |
| `esp32/` | esp32 builds | `esp32` |
| `mimxrt/` | mimxrt builds | `mimxrt` |
| `samd/` | samd builds | `samd` |
| `rp2/` | rp2 builds | `rp2` |

Hardware-specific interfaces (`spibus`, `rgbframebuffer`, …) do **not** build on desktop MicroPython ports (`unix`, `windows`, …).

## Build files in every port tree

Each `ports/<name>/` directory has:

| File | Used by |
|------|---------|
| `micropython.mk` | Make-based MicroPython ports |
| `micropython.cmake` | CMake MicroPython ports (esp32, rp2, …) |

Root `micropython.mk` / `micropython.cmake` detect the active port, include `ports/common/` on **MCU** targets only, then include the matching port tree.

## Rules

1. **`ports/common/`** — shared across MCU ports; uses `machine.*` APIs. Includes `i80bus/gpio_bitbang.c` for GPIO bit-bang backends.
2. **`ports/<port>/`** — SoC SDK code. One tree per MP port name.
3. New portable interface → `ports/common/<name>/` + update `ports/common/micropython.mk`.
4. New port-specific interface → source under `ports/<port>/` + update that port's `micropython.mk`.
