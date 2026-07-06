# displayif

Native display interface drivers for pydisplay — **universal** buses plus **MCU-specific** scanout (ESP-IDF, i.MX RT, …).

**Status:** scaffolded layout — Phase 1 is universal SPI. See [HANDOFF.md](HANDOFF.md).

## Layout

```
displayif/
  micropython.mk / micropython.cmake / circuitpython.mk / manifest.py   # cmods discovery (repo root)
  mk/                  # port-gated build fragments
  drivers/             # universal (machine.SPI, …)
  ports/               # esp_idf/, imxrt/, …
  py/displayif/        # frozen Python package
  tests/
```

## Consumers

| Tier | Module | pydisplay backend |
|------|--------|-------------------|
| Universal | `displayif.spi` | **BusDisplay** |
| ESP-IDF | `displayif.rgb565` | **RGBDisplay** |
| ESP-IDF | `displayif.rgbframebuffer` | **FBDisplay** |
| i.MX RT | TBD | TBD |

## Build (cmods workspace)

```bash
cd ~/github/cmods
git clone https://github.com/PyDevices/displayif.git displayif

# Add to cmods/manifest.py:
#   package("displayif", base_path="displayif/py", opt=3)

./build_mp.sh --port rp2 --board RPI_PICO2_W      # SPI only (Phase 1)
./build_mp.sh --port esp32 --board ESP32_GENERIC_S3   # SPI + ESP-IDF RGB (Phase 2)
./build_mp.sh --port mimxrt --board TEENSY40      # SPI only until imxrt drivers land
```

CircuitPython: `circuitpython.mk` at repo root; use `apply_cp_displayif_patches.sh` (to be added) before `lv_circuitpython_mod/build_cp.sh`.

## Related

- [HANDOFF.md](HANDOFF.md) — architecture, phases, PR sequence
- [PyDevices/pydisplay](https://github.com/PyDevices/pydisplay) — board configs, displaysys backends
- [PyDevices/cmods](https://github.com/PyDevices/cmods) — workspace build scripts
