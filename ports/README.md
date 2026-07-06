# ports/

All interface code and **per-port build glue** live here. Folder names match **MicroPython** `ports/<name>/`.

| Directory | When included | MP port | CP port dir |
|-----------|---------------|---------|-------------|
| `common/` | always | all | all (when patched) |
| `esp32/` | esp32 builds | `esp32` | `espressif` |
| `mimxrt/` | mimxrt builds | `mimxrt` | `mimxrt` |

## Build files in every port tree

Each `ports/<name>/` directory has the same three filenames:

| File | Used by |
|------|---------|
| `micropython.mk` | Make-based MicroPython ports |
| `micropython.cmake` | CMake MicroPython ports (esp32, rp2, …) |
| `circuitpython.mk` | CircuitPython ports |

Root `micropython.mk` / `micropython.cmake` / `circuitpython.mk` detect the active port and `include` `ports/common/` plus the matching port tree.

## Rules

1. **`ports/common/`** — portable; only `machine.*` / CP bus APIs.
2. **`ports/<port>/`** — SoC SDK code. One tree per MP port name.
3. New portable interface → `ports/common/<name>/` + update `ports/common/micropython.mk`.
4. New port-specific interface → source under `ports/<port>/` + update that port's `micropython.mk`.
