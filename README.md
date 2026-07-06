# displayif

Native display interface drivers for pydisplay — portable code in `ports/common/`, SoC-specific code under `ports/<mp-port>/`.

**Status:** scaffolded layout — Phase 1 is universal SPI. See [HANDOFF.md](HANDOFF.md).

## Layout

```
displayif/
  micropython.mk / micropython.cmake / circuitpython.mk / manifest.py
  mk/                    # port-gated build fragments
  ports/
    common/              # portable (SPI, …) — all ports
    esp32/               # ESP-IDF RGB panel, rgb666
    mimxrt/              # NXP LCD blocks
  py/displayif/
  tests/
```

## Consumers

| Location | Module | pydisplay backend |
|----------|--------|-------------------|
| `ports/common` | `displayif.spi` | **BusDisplay** |
| `ports/esp32` | `displayif.rgbpanel` | **RGBDisplay** |
| `ports/esp32` | `displayif.rgb666` | **FBDisplay** |
| `ports/mimxrt` | TBD | TBD |

## Build (cmods workspace)

```bash
cd ~/github/cmods
git clone https://github.com/PyDevices/displayif.git displayif

# Add to cmods/manifest.py:
#   package("displayif", base_path="displayif/py", opt=3)

./build_mp.sh --port rp2 --board RPI_PICO2_W
./build_mp.sh --port esp32 --board ESP32_GENERIC_S3
./build_mp.sh --port mimxrt --board TEENSY40
```

CircuitPython: `circuitpython.mk` at repo root; `apply_cp_displayif_patches.sh` (to be added) before `lv_circuitpython_mod/build_cp.sh`.

## Related

- [HANDOFF.md](HANDOFF.md) — architecture, phases, naming
- [PyDevices/pydisplay](https://github.com/PyDevices/pydisplay)
- [PyDevices/cmods](https://github.com/PyDevices/cmods)
