# AGENTS.md — displayif

Native MicroPython display **interface** modules for pydisplay (`USER_C_MODULES`).
Portable code in `ports/common/`; SoC code under `ports/<mp-port>/`.

**Before editing lifecycle / soft-reset / a new board bring-up**, read:

1. [IDEMPOTENT_LIFECYCLE.md](IDEMPOTENT_LIFECYCLE.md) — required `deinit` / soft-reset contract (**implemented** for all real backends)
2. [SOFT_RESET_AND_BRINGUP.md](SOFT_RESET_AND_BRINGUP.md) — proven failure modes and methods from **ESP32-P4 `mipidsi`** and **Qualia S3 `displayif.DotClockFramebuffer`** (+ LVGL); applies to other ports/interfaces
3. [HANDOFF.md](HANDOFF.md) — module/port matrix and pydisplay board-config map

Those two interfaces are the reference bring-ups: keep their scanout / blit /
attr / soft-reset patterns when changing siblings.

## Hard rules

- **Fix root causes in displayif** (or the owning binding). Do not require hard reset, and do not special-case board_configs to avoid second init.
- **Do not patch `micropython/`** for soft-reset teardown when a usermod `--wrap` or registry hook will do (see soft-reset docs). Upstream clones under cmods must not be committed.
- **Every** accelerated backend that owns DMA/IRQ/PIO/SDK handles must register with `displayif_register_soft_reset()` and tear down from the same path used by `deinit` / `__del__` / idempotent ctors.
- Host teardowns run from `--wrap=gc_sweep_all` (before the heap is wiped). They must **not** `m_free` GC memory — only release non-GC host resources.

## Build / flash (agent workflow)

Public docs stay sibling-clone + stock `make` / `idf.py`. When working inside Brad’s cmods + mpftp setup for a connected MCU:

```bash
mpftp firmware build --port esp32 --board ESP32_GENERIC_P4 --variant C6_WIFI
mpftp firmware flash -d COM4
```

Prefer **mpftp** for board serial / firmware when that skill/session is available; do not assume `build_mp.sh` for MCU flash.

## Soft-reset smoke (minimum)

After changing any host teardown or wrap:

1. Construct the interface (or `import board_config` / example).
2. Soft-reset (`mpftp soft-reset` or `machine.soft_reset()`).
3. Construct / import again — must succeed **without** hard reset or Guru Meditation.
