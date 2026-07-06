# ports/

All interface code and **per-port build glue** live here. Folder names match **MicroPython** `ports/<name>/`.

| Directory | When included | MP port | CP port dir |
|-----------|---------------|---------|-------------|
| `common/` | MCU builds only | esp32, mimxrt, … | espressif, mimxrt, … |
| `esp32/` | esp32 builds | `esp32` | `espressif` |
| `mimxrt/` | mimxrt builds | `mimxrt` | `mimxrt` |

Hardware-specific interfaces (`spibus`, `rgbframebuffer`, …) do **not** build on desktop MicroPython ports (`unix`, `windows`, …).

## Build files in every port tree

Each `ports/<name>/` directory has the same three filenames:

| File | Used by |
|------|---------|
| `micropython.mk` | Make-based MicroPython ports |
| `micropython.cmake` | CMake MicroPython ports (esp32, rp2, …) |
| `circuitpython.mk` | CircuitPython ports |

Root `micropython.mk` / `micropython.cmake` / `circuitpython.mk` detect the active port, include `ports/common/` on **MCU** targets only, then include the matching port tree.

## Rules

1. **`ports/common/`** — shared across MCU ports; uses `machine.*` / CP bus APIs. Not built on unix/windows.
2. **`ports/<port>/`** — SoC SDK code. One tree per MP port name.
3. New portable interface → `ports/common/<name>/` + update `ports/common/micropython.mk`.
4. New port-specific interface → source under `ports/<port>/` + update that port's `micropython.mk`.
