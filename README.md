# displayif

Native display **interface** cmods for pydisplay. Portable code in `ports/common/`; SoC-specific code under `ports/<mp-port>/`.

pydisplay MicroPython board configs that raise `NotImplementedError` for missing native interfaces are **waiting on this repo**.

**Status:** scaffolded — Phase 1 is universal SPI. See [HANDOFF.md](HANDOFF.md).

## Layout

```
displayif/
  micropython.mk / micropython.cmake / circuitpython.mk / manifest.py
  ports/
    common/              # micropython.mk, micropython.cmake, circuitpython.mk + spi/
    esp32/               # same trio + RGB panel, rgb666, rgbmatrix (planned)
    mimxrt/              # same trio + NXP drivers (planned)
  py/displayif/
  tests/
```

Each `ports/<name>/` directory contains the same build filenames; root makefiles detect the active port and include the right trees.

## Planned modules

| Module | Port tree | pydisplay backend |
|--------|-----------|-------------------|
| `displayif.spi` | `common` | **BusDisplay** |
| `displayif.rgbpanel` | `esp32` | **RGBDisplay** |
| `displayif.rgb666` | `esp32` | **FBDisplay** |
| `displayif.rgbmatrix` | `esp32` / `mimxrt` | **FBDisplay** |

## Build (cmods workspace)

```bash
cd ~/github/cmods
git clone https://github.com/PyDevices/displayif.git displayif

# cmods/manifest.py:
#   package("displayif", base_path="displayif/py", opt=3)

./build_mp.sh --port rp2 --board RPI_PICO2_W
./build_mp.sh --port esp32 --board ESP32_GENERIC_S3
./build_mp.sh --port mimxrt --board TEENSY40
```

## Related

- [HANDOFF.md](HANDOFF.md)
- [PyDevices/pydisplay](https://github.com/PyDevices/pydisplay)
- [PyDevices/cmods](https://github.com/PyDevices/cmods)
