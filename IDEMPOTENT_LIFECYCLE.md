# Agent brief: Idempotent init / deinit for all displayif interfaces

**Audience:** Cloud or local coding agent implementing lifecycle teardown across displayif.  
**Repo:** [PyDevices/displayif](https://github.com/PyDevices/displayif) (also present under `cmods/displayif`).  
**Companion docs:** [HANDOFF.md](HANDOFF.md) (port matrix), [README.md](README.md) (module table).  
**Related pydisplay:** `displaysys` already expects displays to support idempotent `deinit()` at the Python layer — native modules must match.

---

## Mission

Make **every** displayif bus / framebuffer / panel constructor **idempotent** with respect to host hardware state, and provide **complete, idempotent teardown** that runs on:

1. Explicit `deinit()` (preferred public API)
2. `__del__` / finalizer (best-effort; must not crash if already deinited)
3. **MicroPython soft reset** (required — see why below)

After this work, a board may soft-reset (or mpftp connect / Run, which soft-resets) and then construct the same interface again **without hard reset** and without ESP-IDF / SDK “resource already taken” failures.

### Motivating failure (ESP32-P4 MIPI DSI)

Symptom after successful first `mipidsi.Bus` + `mipidsi.Display`, then soft reset + re-import `board_config`:

```text
E (…) intr_alloc: No free interrupt inputs for DSI_BRIDGE interrupt (flags 0xE)
E (…) lcd.dsi: esp_lcd_new_panel_dpi(…): allocate DSI Bridge interrupt failed
OSError: ESP-IDF error 261 (ESP_ERR_NOT_FOUND)
```

Cause: soft reset clears the Python heap; it does **not** release ESP-IDF DSI/DPI interrupt allocations. Current `ports/esp32/mod_mipidsi.c` always calls `esp_lcd_new_dsi_bus` / `esp_lcd_new_panel_dpi` and has **no** `Bus` teardown. `Display.__del__` is incomplete and often never runs on soft reset.

This class of bug exists (or will exist) for every accelerated bus that owns DMA, IRQs, PIO SMs, FlexIO, eLCDIF, HSTX, etc.

---

## Terminology (use correctly)

| Term | Meaning for this task |
|------|------------------------|
| **Idempotent** | Calling `Ctor()` / `deinit()` again is safe: reuse live object, or tear down previous host resources then recreate. Second `deinit()` is a no-op. |
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

Therefore each module that owns host resources must register a **soft-reset teardown hook** (or a shared displayif soft-reset dispatcher) that runs **before** the heap is wiped.

---

## Scope — modules and ports

Implement for **all real backends**. Stubs under `ports/common/notimpl/` and ctor-raise stubs stay stubs (no hardware to free).

| Module | Python import | Real implementations | pydisplay backend |
|--------|---------------|----------------------|-------------------|
| `spibus` | `spibus` | `ports/common/spi/mod_spibus.c` | BusDisplay |
| `i2cbus` | `i2cbus` | `ports/common/i2c/mod_i2cbus.c` | BusDisplay |
| `i80bus` | `i80bus` | `ports/esp32/mod_i80bus.c`, `ports/rp2/mod_i80bus.c`, `ports/mimxrt/mod_i80bus.c`, `ports/samd/mod_i80bus.c` (+ `ports/common/i80bus/`) | BusDisplay |
| `rgbframebuffer` | `rgbframebuffer` | `ports/esp32/mod_rgbframebuffer.c`, `ports/mimxrt/mod_rgbframebuffer_elcdif.c` | FBDisplay |
| `mipidsi` | `mipidsi` | `ports/esp32/mod_mipidsi.c`, `ports/mimxrt/mod_mipidsi.c` (+ `mimxrt1176_dsi_display.*`) | FBDisplay |
| `picodvi` | `picodvi` | `ports/rp2/mod_picodvi.c` (+ `picodvi_rp2040.*`, `picodvi_rp2350.*`) | FBDisplay |
| `rgbmatrix` | `rgbmatrix` | `ports/common/rgbmatrix/mod_rgbmatrix.c` (+ per-port `rgbmatrix_pm.c`) | FBDisplay |

Ports tree: `ports/common`, `ports/esp32`, `ports/mimxrt`, `ports/rp2`, `ports/samd`.

**Out of scope:** CircuitPython bindings (displayif does not ship CP); changing pydisplay board_configs to avoid re-init (that is a workaround).

---

## Current gaps (inventory for agents)

Updated after lifecycle implementation (2026-07) — verify with grep if unsure.

| Module / port | `deinit()` | `__del__` | Soft-reset hook | Notes |
|---------------|------------|-----------|-----------------|-------|
| `spibus` (common) | yes | yes | n/a (Python SPI) | Idempotent `deinited` guard |
| `i2cbus` (common) | yes | yes | n/a (Python I2C) | Clears `vstr`; thin wrapper |
| `i80bus` esp32/rp2/mimxrt/samd | yes | yes | yes (hw ports) | Static host handles; SAMD GPIO is thin |
| `rgbframebuffer` esp32 | yes | yes | yes | Panel + SPIRAM buf in BSS host |
| `rgbframebuffer` mimxrt eLCDIF | yes | yes | yes | Stops eLCDIF; GC buf not `m_free`'d on soft reset |
| `mipidsi` esp32 | Bus + Display | yes | yes | Primary fix: `esp_lcd_del_dsi_bus` + LDO release |
| `mipidsi` mimxrt | Bus + Display | yes | yes | Wires `*_bus_deinit` / `*_display_stop` |
| `picodvi` rp2 | yes | yes | yes | Static HW shadow; no dangling `active_picodvi` |
| `rgbmatrix` | yes | yes | yes (Protomatter) | PM core in BSS; bitbang path has no host IRQs |

Shared: `include/displayif/soft_reset.h` + `ports/common/soft_reset.c` (`--wrap=mp_deinit`).

---

## Required contract (all interfaces)

### Public Python API

For each constructible type that owns host resources (`Bus`, `Display`, `RGBFrameBuffer`, `Framebuffer`, `I80Bus`, matrix objects, etc.):

```python
obj.deinit()   # idempotent; safe if never fully constructed / already deinited
```

Also keep or add `__del__` that calls the same internal teardown (never raises into GC).

Recommended pattern:

- Internal `foo_deinit_internal(self)` clears handles and sets a `deinited` / NULL-handle flag.
- `deinit()` and `__del__` both call it.
- Constructors call “force global/singleton teardown for this hardware unit” **before** allocating, **or** return the existing live object if parameters match (document which policy you chose per module; **tear down + recreate** is usually simpler and matches board_config re-import).

### Idempotent constructor

Second `Bus(...)` / `Display(...)` / `RGBFrameBuffer(...)` on the same hardware unit must not fail with “no interrupt” / “resource busy”. Preferred approach:

1. If module-level (or singleton) handles for that unit are non-NULL → full hardware teardown.
2. Then create fresh.

Optional: if args are identical and object still live, return the existing object — only if reference ownership is clear.

### Soft-reset registration

Register a callback that tears down **all** live displayif host resources for that module (or a process-wide displayif registry).

MicroPython mechanisms vary by port; acceptable approaches (pick what fits each port cleanly):

- Port soft-reset / `MACHINE_SOFT_RESET` style hooks if the usermod API allows
- Shared `displayif` registry: each successful ctor registers a teardown fn; soft-reset iterates and clears
- ESP-IDF: ensure `esp_lcd_*_del` / bus del / LDO release run in that hook

**Must run even when no Python references remain.**

### What “complete teardown” means

Free or stop, as applicable:

- Panel / panel IO / DPI / DBI handles  
- DSI / LCD / RGB buses  
- DMA channels, GPTIMER, RMT, LEDC  
- PIO state machines, HSTX  
- FlexIO / eLCDIF / LCDIFv2  
- IRQs / interrupt allocations  
- Framebuffer heap (`SPIRAM` / `aligned_alloc`) when owned by the module  
- Regulator / LDO channels acquired for PHY  
- GPIO exclusive configs if the module claimed them  

After teardown, a subsequent constructor must behave like first boot (aside from panel power-on timing).

---

## Suggested implementation order

1. **`mipidsi` esp32** — reproduces today; prove the pattern (Bus deinit, Display deinit, module singleton teardown, soft-reset hook, idempotent ctor).  
2. **`mipidsi` mimxrt** — wire existing `displayif_mimxrt1176_dsi_*_deinit/stop`.  
3. **`rgbframebuffer` esp32 + mimxrt** — same IRQ/panel class of bugs.  
4. **`i80bus` all ports** — harden existing `deinit`; add soft-reset + idempotent ctor.  
5. **`picodvi`**, **`rgbmatrix`** — expose/complete deinit + soft-reset.  
6. **`spibus` / `i2cbus`** — add missing pieces; keep thin wrappers correct.

Share helpers under `ports/common/` where possible (registry, “already deinited” guards). Avoid copying divergent policies per file.

---

## Acceptance criteria

For **each** real module/port above:

1. **Cold start:** construct once — works as today.  
2. **Explicit deinit + reconstruct:** `obj.deinit();` then construct again with same args — succeeds.  
3. **Double deinit:** `obj.deinit(); obj.deinit();` — no crash / no exception.  
4. **Soft reset + reconstruct:** construct → `machine.soft_reset()` (or mpftp soft-reset) → construct again in fresh REPL — **succeeds without hard reset**.  
5. **Import board_config twice across soft reset** (ESP32-P4 `mipidsi` board) — no `ESP_ERR_NOT_FOUND` / DSI_BRIDGE interrupt failure.  
6. Stubs unchanged (still raise / notimpl).  
7. No silent “ignore error and continue” that leaves hardware half-initialized.  
8. Tests: extend smoke tests or add `tests/test_*_lifecycle.py` that skip on unsupported SoCs; document hardware-required cases in `tests/README.md`.

---

## Engineering standards

- **Fix the root cause** in displayif — do not require hard reset, and do not special-case board_configs or examples to avoid second init.  
- Prefer one clear teardown path used by `deinit`, `__del__`, soft-reset, and idempotent ctor.  
- Match existing naming (`deinit` is already used on `i80bus` / `spibus`; framebuffer modules often use `__del__` — add **both** `deinit` and `__del__` for parity with pydisplay).  
- Do not commit upstream `micropython/` tree changes unless unavoidable; prefer usermod-local hooks/registry inside displayif.

---

## Key files to edit

```
ports/esp32/mod_mipidsi.c                 # start here
ports/mimxrt/mod_mipidsi.c
ports/mimxrt/mimxrt1176_dsi_display.c
ports/mimxrt/mimxrt1176_dsi_display.h
ports/esp32/mod_rgbframebuffer.c
ports/mimxrt/mod_rgbframebuffer_elcdif.c
ports/esp32/mod_i80bus.c
ports/rp2/mod_i80bus.c
ports/mimxrt/mod_i80bus.c
ports/samd/mod_i80bus.c
ports/rp2/mod_picodvi.c
ports/rp2/picodvi_rp2040.c / picodvi_rp2350.c
ports/common/rgbmatrix/mod_rgbmatrix.c
ports/*/rgbmatrix_pm.c
ports/common/spi/mod_spibus.c
ports/common/i2c/mod_i2cbus.c
ports/common/mp_helpers.c                 # possible shared registry
include/displayif/…                       # shared headers if needed
tests/                                    # lifecycle tests
```

Build via sibling MicroPython + `USER_C_MODULES` (see README). Optional: cmods `build_mp.sh` / `build_pydisplay_runtimes.sh` when working inside that workspace — do **not** rewrite public docs to require cmods.

---

## Non-goals

- Making display drivers “reentrant” for nested IRQ callbacks (separate topic).  
- Changing LVGL / pydisplay app logic except where a tiny call to `display.deinit()` is needed for symmetry (prefer native soft-reset so apps need not change).  
- Supporting concurrent two displays on one DSI host unless hardware truly allows it — singleton-per-unit is fine.

---

## Done when

An agent (or human) can soft-reset an ESP32-P4 board that previously initialized `mipidsi`, then `import board_config` (or construct `Bus`+`Display` again) successfully; and the same lifecycle story holds for `rgbframebuffer`, `i80bus`, `picodvi`, `rgbmatrix`, `spibus`, and `i2cbus` on their respective ports.
