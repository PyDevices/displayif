# Soft-reset & board bring-up — agent notes

Lessons from bringing up **ESP32-P4 MIPI DSI** (`mipidsi`) with pydisplay + LVGL
(`lv_test_timer`) under mpftp soft-reset (July 2026). Patterns apply when
porting **any** displayif interface on **any** MCU port (esp32 / rp2 / mimxrt /
samd).

Companion: [IDEMPOTENT_LIFECYCLE.md](IDEMPOTENT_LIFECYCLE.md) (contract),
[HANDOFF.md](HANDOFF.md) (matrix), [AGENTS.md](AGENTS.md) (entry).

---

## Soft-reset architecture (read first)

On MicroPython MCU ports the soft-reset exit path is typically:

```text
… → gc_sweep_all() → … → mp_deinit() → soft_reset loop
```

| Mechanism | When | What |
|-----------|------|------|
| `--wrap=gc_sweep_all` (`ports/common/soft_reset.c`) | **Before** heap wipe | `displayif_port_pre_gc_sweep()` then `displayif_soft_reset_all()` |
| `--wrap=mp_deinit` (same file) | After sweep | Idempotent second `displayif_soft_reset_all()` |
| `displayif_register_soft_reset(fn)` | At first ctor | Per-interface host teardown (mipidsi, rgbframebuffer, i80bus, picodvi, rgbmatrix, …) |
| `displayif_port_pre_gc_sweep()` | Strong symbol on esp32 | Stop `machine.Timer` / clear handlers (weak empty default elsewhere) |

**Do not** add soft-reset teardown by patching `micropython/ports/*/main.c` when
these wraps suffice. Temporary upstream patches are easy to leave behind and
hard for the next agent to find.

Teardowns must only touch **non-GC** host state (ESP-IDF handles, DMA, PIO,
SPIRAM buffers owned via `heap_caps_*`, etc.). Never rely on `__del__` alone.

Verify wraps landed in the linked ELF after a rebuild:

```bash
nm …/micropython.elf | rg 'wrap_gc_sweep|wrap_mp_deinit|displayif_port_pre_gc_sweep|displayif_soft_reset_all'
```

---

## Troubleshooting method (what worked)

Use this loop on a new board / interface instead of guessing from one symptom.

### 1. Establish a reversible baseline

- Backup the MCU filesystem before mass deletes or firmware experiments
  (`mpftp cp :/ …` or targeted `get`).
- Note board + variant + partition autosize (P4 + LVGL often overflows stock
  app partition; mpftp autosize grows `esp32_partitions/<board>.csv`).

### 2. Prefer fast package install over serial spam

- Push only thin host-side files with mpftp (`wifi.py`, `secrets.py`).
- Use **`mip.install` over Wi‑Fi** for pydisplay board packages / libs — much
  faster than recursive mpftp/mpremote for large trees.
- Soft-reset between major FS changes so imports see a clean heap.

### 3. Separate failure classes early

Reproduce with the **smallest** script that still fails (`import board_config`,
then `import lv_test_timer`, not the whole gallery). Classify:

| Class | Examples |
|-------|----------|
| Host lifecycle | Interrupt already taken, bus not found after soft-reset |
| Timer / GC race | Guru Meditation / Load access fault **after** soft-reset into LVGL or bindings |
| Presentation | Black panel, wrong colors, very slow full-screen updates |
| Timebase | UI timers run at wrong wall-clock rate |
| Input | Taps ignored, inverted axes, release never delivered |
| Tooling noise | mpftp EOF timeout, flash write flicker from debug logs |

Fix the class at its layer; do not paper over with board_config special cases.

### 4. Soft-reset is the acceptance test

For every host-owning change:

```text
construct / import → soft-reset → construct / import again
```

Must succeed **without hard reset**. mpftp connect / Run paths soft-reset and
skip `main.py` — that is the workflow users hit.

**Tooling note:** `timeout waiting for first EOF` right after soft-reset is
often an mpftp session race, not a panicking board. `mpftp resume` / reconnect
and retry once. A real Guru Meditation usually needs a hard reset and shows
up in the serial boot log.

### 5. Prove native vs Python with evidence

- Time hot paths (blit row, refresh) before rewriting.
- After C changes: rebuild + flash firmware (mpftp firmware build/flash when
  available), then re-run the same soft-reset smoke.
- Confirm symbols (`nm`) when debugging wrap/hook issues.
- Remove temporary flash-backed debug instrumentation before calling a UI
  “fixed” — writing NDJSON to flash on every touch can look like flicker.

### 6. Revert failed hypotheses

If a fix does not move the acceptance test, remove it before trying the next
approach. Do not stack silent fallbacks.

---

## Symptom → likely cause (proven cases)

| Symptom | Likely cause | Fix location |
|---------|--------------|--------------|
| Soft-reset then `import` → Guru Meditation / Load access fault in LVGL (`get_native_obj`, `set_draw_buffers`, weird `.type` pointing at a method) | `machine.Timer` / `esp_timer` still armed; fires into swept Python callbacks | `displayif_port_pre_gc_sweep()` (esp32); do **not** leave this only in micropython `main.c` |
| Soft-reset then reconstruct → `ESP_ERR_NOT_FOUND` / “No free interrupt” (DSI bridge, etc.) | Host bus/panel/IRQ not released; `__del__` never ran | `displayif_register_soft_reset` + complete `*_host_teardown`; must run before heap wipe |
| Black panel, process “works” | Missing `show` / `refresh_cb`, backlight off, wrong fb path | `displaysys` + board_config; keep presentation wired for mipidsi |
| UI ~1–2 FPS / multi‑second full redraws | Python `memoryview` blit into SPIRAM framebuffer (~tens of ms/row) | Native `Display.blit` in displayif + `fbdisplay` using `raw_buffer.blit` when present |
| Illegal instruction / crash inside LVGL draw or early init (not only soft-reset) | `LV_GLOBAL_CUSTOM` not rooted in `MP_STATE_VM(mp_lv_roots)` | `lv_bindings` conf / emit / generated |
| LVGL seconds advance ~½ wall clock | Fixed `tick_inc(period)` vs real elapsed | `lv.tick_inc(elapsed_ms)` from wall time in the timer callback |
| Crash / corruption if Runtime timer runs during LVGL DisplayDriver setup | Tick into half-initialized LVGL | Stop Runtime timer around setup; arm loop only when ready (`display_driver`) |
| Touch down never seen / stuck pressed | indev `read_cb` polls but does not always write PRESSED/RELEASED | Always call the touch callback after poll so LVGL sees edges |
| Touch mirrored / inverted | GT911 (or similar) axis flags wrong for panel orientation | `board_config` `reverse_axis` / `reverse_y` (validate by tapping known UI targets) |
| Flicker only while “debugging” | Flash writes from agent NDJSON / probe files on the hot path | Delete instrumentation; keep functional fixes |

---

## Checklist: new interface or new port

When adding or porting a backend that owns host resources:

1. [ ] File-static BSS mirrors of live SDK/DMA/PIO handles
2. [ ] Single internal teardown used by `deinit`, `__del__`, idempotent ctor, soft-reset
3. [ ] `displayif_register_soft_reset(teardown)` on first successful init
4. [ ] Soft-reset + reconstruct acceptance (no hard reset)
5. [ ] No `m_free` of GC objects from soft-reset teardown
6. [ ] If the port has async hardware timers that schedule Python (esp32
      `machine.Timer`), ensure `displayif_port_pre_gc_sweep()` stops them — or
      implement the strong hook for that port
7. [ ] Large framebuffers: PSRAM / sdkconfig sized (see HANDOFF); prefer native
      blit paths into SPIRAM
8. [ ] pydisplay board_config: touch axes, backlight, `FBDisplay` refresh wiring

Stubs under `ports/common/notimpl/` stay stubs — no soft-reset registration.

---

## pydisplay / LVGL interaction (outside this repo)

These are not displayif bugs but show up during the same bring-up:

- **`eventsys.Runtime` + `multimer`:** one shared periodic timer; LVGL claims
  presentation via `runtime.claim_display_refresh()` / `display_driver`.
- **Interactive REPL / mpftp:** `run_forever` / select paths must not assume a
  non-interactive process (see pydisplay `eventsys` / `multimer`).
- **Do not leave flash logging** in `display_driver`, examples, or board_config
  on the touch/refresh path.

When changing only Python on-device, soft-reset + re-import is enough. When
changing displayif C or LVGL bindings, rebuild/flash firmware first.

---

## Quick commands (mpftp-oriented)

```bash
mpftp connect COM4
mpftp soft-reset
mpftp exec 'import board_config; print("ok")'
# or
mpftp exec 'import lv_test_timer; print(lv_test_timer.get_state())'

mpftp firmware build --port esp32 --board ESP32_GENERIC_P4 --variant C6_WIFI
mpftp firmware flash -d COM4
```

Disconnect the UI session (or `mpftp disconnect`) before flashing if the port
is busy.

---

*Seeded from ESP32-P4-WIFI6-Touch-LCD-4B + `lv_test_timer` bring-up, 2026-07-20.
Update this file when a new port/interface reveals a durable failure mode.*
