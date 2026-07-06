# displayif

Native display **interface** cmods for pydisplay. Portable code in `ports/common/`; SoC-specific code under `ports/<mp-port>/`.

pydisplay MP board configs that raise `NotImplementedError` are waiting on modules built here. Native C modules register directly — **no Python re-export layer** in this repo.

**Status:** Phase 1 `spibus` and esp32 `rgbframebuffer` skeleton landed — scanout WIP. See [HANDOFF.md](HANDOFF.md).

## Planned native modules

| Module | Port tree | pydisplay backend |
|--------|-----------|-------------------|
| `spi` (name TBD) | `common` | **BusDisplay** |
| `rgbframebuffer` | `esp32` | **FBDisplay** |
| `rgbmatrix` | `esp32` / `mimxrt` | **FBDisplay** |

Parallel dot-clock RGB (RGB-666 and 16-pin RGB565) both use `rgbframebuffer.RGBFrameBuffer` — no `RGBDisplay`.

## Build (cmods workspace)

```bash
cd ~/github/cmods
git clone https://github.com/PyDevices/displayif.git displayif
./build_mp.sh --port esp32 --board ESP32_GENERIC_S3
```

## Related

- [HANDOFF.md](HANDOFF.md) — pydisplay migration checklist, native API
- [PyDevices/pydisplay](https://github.com/PyDevices/pydisplay)
- [PyDevices/cmods](https://github.com/PyDevices/cmods)
