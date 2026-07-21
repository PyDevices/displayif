# Idempotent init / deinit — displayif contract

**Audience:** Agents and humans changing host-owning displayif backends.  
**Repo:** [PyDevices/displayif](https://github.com/PyDevices/displayif) (also under `cmods/displayif`).  
**Companions:** [AGENTS.md](AGENTS.md), [HANDOFF.md](HANDOFF.md), [SOFT_RESET_AND_BRINGUP.md](SOFT_RESET_AND_BRINGUP.md), [README.md](README.md).  
**Related pydisplay:** `displaysys` expects idempotent `deinit()` — native modules must match.

**Status (2026-07):** Implemented for all real backends listed below. Soft-reset + reconstruct is the acceptance test. This doc is the **contract and inventory**, not a todo brief.

---

## Goal

Every displayif bus / framebuffer / panel constructor is **idempotent** with respect to host hardware state, and **complete, idempotent teardown** runs on:

1. Explicit `deinit()` (preferred public API)
2. `__del__` / finalizer (best-effort; must not crash if already deinited)
3. **MicroPython soft reset** (required — see why below)

A board may soft-reset (or mpftp connect / Run, which soft-resets) and then construct the same interface again **without hard reset** and without ESP-IDF / SDK “resource already taken” failures.

### Motivating failure (ESP32-P4 MIPI DSI) — fixed

Symptom after successful first `mipidsi.Bus` + `mipidsi.Display`, then soft reset + re-import `board_config`:

```text
E (…) intr_alloc: No free interrupt inputs for DSI_BRIDGE interrupt (flags 0xE)
E (…) lcd.dsi: esp_lcd_new_panel_dpi(…): allocate DSI Bridge interrupt failed
OSError: ESP-IDF error 261 (ESP_ERR_NOT_FOUND)
```

**Cause:** soft reset clears the Python heap; it does **not** release ESP-IDF DSI/DPI interrupt allocations. Without host teardown, a second ctor fails.

**Fix:** `ports/esp32/mod_mipidsi.c` mirrors bus/panel/LDO/FB handles in BSS, registers `mipidsi_host_teardown` with `displayif_register_soft_reset()`, and uses the same teardown from `deinit` / `__del__` / idempotent ctors (`esp_lcd_del_*`, LDO release, `heap_caps_free`). See [SOFT_RESET_AND_BRINGUP.md](SOFT_RESET_AND_BRINGUP.md).

The same class of bug applies to every accelerated bus that owns DMA, IRQs, PIO SMs, FlexIO, eLCDIF, HSTX, RGB panel handles, etc. — including **`displayif.DotClockFramebuffer`** on ESP32-S3 (Qualia).

---

## Terminology

| Term | Meaning |
|------|---------|
| **Idempotent** | Calling `Ctor()` / `deinit()` again is safe: tear down previous host resources then recreate (or no-op if already clean). Second `deinit()` is a no-op. |
| **Reentrant** | Safe if entered while already on the call stack. **Not** the goal here. |

Do **not** paper over missing teardown with “only init once” board_config special cases or “require hard reset” docs. Fix the native lifecycle.

---

## Why soft reset matters

Tools such as **mpftp** (and Thonny-style workflows) soft-reset into a clean heap **without** running `main.py`, then the user imports `board_config` or runs a script that constructs displays again.

| Layer | Soft reset behavior |
|-------|---------------------|
| MicroPython heap | Cleared — Python objects gone |
| `__del__` | **Unreliable** — must not be the only teardown path |
| ESP-IDF / NXP SDK / PIO / DMA | **Still live** unless C code tears them down |

Each module that owns host resources registers a **soft-reset teardown hook** that runs **before** the heap is wiped (`--wrap=gc_sweep_all` in `ports/common/soft_reset.c`).

---

## Modules and ports

Stubs under `ports/common/notimpl/` and ctor-raise stubs stay stubs (no hardware to free).

| Module | Python import | Real implementations | pydisplay backend |
|--------|---------------|----------------------|-------------------|
| `spibus` | `spibus` | `ports/common/spi/mod_spibus.c` | BusDisplay |
| `i2cbus` | `i2cbus` | `ports/common/i2c/mod_i2cbus.c` | BusDisplay |
| `i80bus` | `i80bus` | `ports/esp32/mod_i80bus.c`, `ports/rp2/mod_i80bus.c`, `ports/mimxrt/mod_i80bus.c`, `ports/samd/mod_i80bus.c` (+ `ports/common/i80bus/`) | BusDisplay |
| `displayif` | `displayif` | `ports/esp32/mod_dotclockframebuffer.c`, `ports/mimxrt/mod_dotclockframebuffer_elcdif.c` | FBDisplay |
| `mipidsi` | `mipidsi` | `ports/esp32/mod_mipidsi.c`, `ports/mimxrt/mod_mipidsi.c` (+ `mimxrt1176_dsi_display.*`) | FBDisplay |
| `picodvi` | `picodvi` | `ports/rp2/mod_picodvi.c` (+ `picodvi_rp2040.*`, `picodvi_rp2350.*`) | FBDisplay |
| `rgbmatrix` | `rgbmatrix` | `ports/common/rgbmatrix/mod_rgbmatrix.c` (+ per-port `rgbmatrix_pm.c`) | FBDisplay |

**Out of scope:** CircuitPython bindings (displayif does not ship CP); changing pydisplay board_configs to avoid re-init.

---

## Inventory (implemented)

Verify with grep if unsure (`displayif_register_soft_reset`, `*_host_teardown`).

| Module / port | `deinit()` | `__del__` | Soft-reset hook | Notes |
|---------------|------------|-----------|-----------------|-------|
| `spibus` (common) | yes | yes | n/a (Python SPI) | Idempotent `deinited` guard |
| `i2cbus` (common) | yes | yes | n/a (Python I2C) | Clears `vstr`; thin wrapper |
| `i80bus` esp32/rp2/mimxrt/samd | yes | yes | yes (hw ports) | Static host handles; SAMD GPIO is thin |
| `displayif` (DotClock) esp32 | yes | yes | yes | Panel + SPIRAM/panel FB in BSS host; Qualia-proven |
| `displayif` (DotClock) mimxrt eLCDIF | yes | yes | yes | Stops eLCDIF; GC buf not `m_free`'d on soft reset |
| `mipidsi` esp32 | Bus + Display | yes | yes | `esp_lcd_del_dsi_bus` + LDO + SPIRAM FB; P4-proven |
| `mipidsi` mimxrt | Bus + Display | yes | yes | Wires `*_bus_deinit` / `*_display_stop` |
| `picodvi` rp2 | yes | yes | yes | Static HW shadow; no dangling `active_picodvi` |
| `rgbmatrix` | yes | yes | yes (Protomatter) | PM core in BSS; bitbang path has no host IRQs |

Shared: `include/displayif/soft_reset.h` + `ports/common/soft_reset.c` (`--wrap=gc_sweep_all` primary; `--wrap=mp_deinit` idempotent second pass). ESP32 also implements `displayif_port_pre_gc_sweep()` to stop `machine.Timer` before the sweep.

---

## Required contract (all interfaces)

### Public Python API

For each constructible type that owns host resources (`Bus`, `Display`, `displayif.DotClockFramebuffer`, `Framebuffer`, `I80Bus`, matrix objects, etc.):

```python
obj.deinit()   # idempotent; safe if never fully constructed / already deinited
```

Also keep `__del__` that calls the same internal teardown (never raises into GC).

Pattern:

- Internal `foo_deinit_internal(self)` clears handles and sets a `deinited` / NULL-handle flag.
- `deinit()` and `__del__` both call it.
- Constructors call global/singleton teardown for that hardware unit **before** allocating (**tear down + recreate** — matches board_config re-import).

### Idempotent constructor

Second `Bus(...)` / `Display(...)` / `displayif.DotClockFramebuffer(...)` on the same hardware unit must not fail with “no interrupt” / “resource busy”:

1. If module-level (or singleton) handles for that unit are non-NULL → full hardware teardown.
2. Then create fresh.

### Soft-reset registration

Register a callback that tears down **all** live host resources for that module. It **must** run even when no Python references remain.

### What “complete teardown” means

Free or stop, as applicable:

- Panel / panel IO / DPI / DBI handles  
- DSI / LCD / RGB buses  
- DMA channels, GPTIMER, RMT, LEDC  
- PIO state machines, HSTX  
- FlexIO / eLCDIF / LCDIFv2  
- IRQs / interrupt allocations  
- Framebuffer heap (`SPIRAM` / `aligned_alloc` / panel-owned FB) when owned by the module  
- Regulator / LDO channels acquired for PHY  
- GPIO exclusive configs if the module claimed them  

After teardown, a subsequent constructor must behave like first boot (aside from panel power-on timing).

---

## Acceptance criteria

For **each** real module/port above:

1. **Cold start:** construct once — works.  
2. **Explicit deinit + reconstruct:** `obj.deinit();` then construct again with same args — succeeds.  
3. **Double deinit:** `obj.deinit(); obj.deinit();` — no crash / no exception.  
4. **Soft reset + reconstruct:** construct → soft-reset → construct again — **succeeds without hard reset**.  
5. **Import board_config twice across soft reset** — no `ESP_ERR_NOT_FOUND` / interrupt failure (proven on ESP32-P4 `mipidsi` and Qualia `displayif.DotClockFramebuffer`).  
6. Stubs unchanged (still raise / notimpl).  
7. No silent “ignore error and continue” that leaves hardware half-initialized.  
8. Tests: `tests/test_lifecycle_api.py` (import-only); hardware soft-reset smoke documented in `tests/README.md` / [SOFT_RESET_AND_BRINGUP.md](SOFT_RESET_AND_BRINGUP.md).

---

## Engineering standards

- **Fix the root cause** in displayif — do not require hard reset, and do not special-case board_configs to avoid second init.  
- Prefer one clear teardown path used by `deinit`, `__del__`, soft-reset, and idempotent ctor.  
- Match naming: expose **both** `deinit` and `__del__`.  
- Do not commit upstream `micropython/` tree changes; prefer usermod-local wraps/registry inside displayif.

---

## Key files

```
include/displayif/soft_reset.h
ports/common/soft_reset.c
ports/esp32/mod_mipidsi.c                 # P4 reference bring-up
ports/esp32/mod_dotclockframebuffer.c     # Qualia; Python displayif.DotClockFramebuffer
ports/mimxrt/mod_mipidsi.c
ports/mimxrt/mimxrt1176_dsi_display.c
ports/mimxrt/mod_dotclockframebuffer_elcdif.c
ports/esp32/mod_i80bus.c
ports/rp2/mod_i80bus.c / mod_picodvi.c
ports/common/rgbmatrix/mod_rgbmatrix.c
tests/test_lifecycle_api.py
```

Build via sibling MicroPython + `USER_C_MODULES` (see README). Optional: cmods `build_mp.sh` when working inside that workspace — do **not** rewrite public docs to require cmods.

---

## Non-goals

- Making display drivers “reentrant” for nested IRQ callbacks.  
- Changing LVGL / pydisplay app logic except where a tiny `display.deinit()` call helps symmetry (prefer native soft-reset so apps need not change).  
- Supporting concurrent two displays on one DSI/RGB host unless hardware truly allows it — singleton-per-unit is fine.
