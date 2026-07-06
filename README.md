# displayif

Native MicroPython display interface drivers for pydisplay framebuffer backends (RGB parallel, HUB75, and related SoC peripherals).

**Status:** early / planning — see [HANDOFF.md](HANDOFF.md) for full context and first implementation steps.

## Consumers

| pydisplay backend | displayif module | Example board config |
|-------------------|------------------|----------------------|
| `displaysys.rgbdisplay.RGBDisplay` | `displayif.rgb565.RGB565Panel` | `pydisplay/board_configs/fbdisplay/t-rgb_480` |
| `displaysys.fbdisplay.FBDisplay` | `rgbframebuffer.RGBFrameBuffer` (planned) | `pydisplay/board_configs/fbdisplay/qualia_tl040hds20` |
| `displaysys.fbdisplay.FBDisplay` | `rgbmatrix.RGBMatrix` (planned) | `pydisplay/board_configs/fbdisplay/cp_matrixportal_s3_64x64` (CP reference) |

## Build

Clone as a sibling under [PyDevices/cmods](https://github.com/PyDevices/cmods) with MicroPython, then:

```bash
./build_mp.sh --port esp32 --board ESP32_GENERIC_S3
```

Details will be added when the first module lands.

## Related

- [PyDevices/pydisplay](https://github.com/PyDevices/pydisplay) — display drivers, board configs, `displaysys` backends
- [HANDOFF.md](HANDOFF.md) — agent/developer continuation guide
