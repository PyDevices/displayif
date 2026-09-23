# Newcomer's guide to displayif

`displayif` supplies native display-interface modules for PyDevices
`displaydev` board configurations. It is source-integrated firmware code, not
a package: a board uses it only when its MicroPython firmware was built with
the required `USER_C_MODULES` backend.

Start from a board configuration in [pydevices](https://github.com/PyDevices/pydevices),
then identify the matching interface in [the port matrix](port-matrix.md).
For example, an ESP32-P4 DSI board needs `mipidsi`, an RGB framebuffer may
need `dotclockframebuffer`, and a desktop MicroPython application uses
`usdl2`.

## The mental model

```text
pydevices board_config
       |
       v
displaydev backend (BusDisplay, FBDisplay, or SDLDisplay)
       |
       v
displayif native module for the selected MicroPython port
       |
       v
panel bus, framebuffer, DSI, DVI, RGB matrix, or SDL2 window
```

Python does not import a wrapper package from this repository. Native modules
register directly in firmware, so a board config that raises
`NotImplementedError` normally indicates firmware missing its required
displayif backend.

## Choose the supported path

CircuitPython already provides its MCU display interfaces; use its stock
firmware and the CircuitPython board configs for those targets. The exception
is desktop `usdl2`, which displayif can add to CircuitPython unix through
`apply_cp_patches.sh`.

For MicroPython Make ports, `USER_C_MODULES` points at the workspace parent
containing this repository. For CMake ports such as ESP32 and RP2, it points
at this repository (or `micropython.cmake`). The root README gives the exact
commands and shows how to combine several CMake user modules.

## Repository map

| Path | Purpose |
|---|---|
| `src/ports/common/` | Shared buses, helpers, RGB matrix code, and lifecycle support. |
| `src/ports/<port>/` | SoC-specific backends for ESP32, RP2, SAMD, and MIMXRT; `stm32` builds the portable buses only. |
| `src/ports/desktop/usdl2/` | SDL2 desktop backend for MicroPython unix and Windows. |
| `src/jpegio/` | Platform-neutral JPEG decoder and optional LVGL decoder integration. |
| `src/include/displayif/` | Public native headers. |
| `micropython.mk`, `micropython.cmake` | Make/CMake user-module entrypoints. |
| `docs/port-matrix.md` | Module/port/board-config support matrix. |
| `docs/idempotent-lifecycle.md` | Required init, deinit, and soft-reset contract. |
| `docs/soft-reset-and-bring-up.md` | Evidence-based bring-up and debugging procedure. |
| `tests/`, `tools/` | API checks and hardware/desktop smoke tools. |

## The important invariant

Every accelerated interface must support safe, repeatable construction and
teardown. `deinit`, destructors, soft reset, and a second constructor call all
release the same non-GC resources. Do not work around failures with hard
resets or board-config special cases; follow
[the lifecycle contract](idempotent-lifecycle.md) and
[the bring-up guide](soft-reset-and-bring-up.md).

## Contributor boundary

This is native portability work. Read [AGENTS.md](../AGENTS.md) before
changing lifecycle, soft-reset, board bring-up, or QSTR definitions. A new
backend needs the appropriate port build, a real smoke test, then a soft reset
and successful second construction.

For a safe first contribution, improve a port note, add a focused API test, or
clarify a matrix entry. Keep generated firmware artifacts and upstream
MicroPython/CircuitPython checkout changes out of this repository.
