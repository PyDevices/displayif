# Roadmap notes

Working checklist tracking displayif's build-out, moved out of
[port-matrix.md](port-matrix.md) (a reference page, not a task list). Status
verified against the tree on 2026-08-29.

1. Scaffold — done
2. pydevices board configs on `dotclockframebuffer.DotClockFramebuffer` + `FBDisplay` — done
3. `spibus` + smoke tests — done
4. esp32 `dotclockframebuffer`, `i80bus`, `mipidsi` — done
5. mimxrt eLCDIF `dotclockframebuffer`, RT1176 `mipidsi`, FlexIO `i80bus` — done
6. rp2 `picodvi`, PIO `i80bus` — done
7. samd GPIO `i80bus` via `common/i80bus/gpio_bitbang.c` — done
8. `rgbmatrix` Protomatter backends — done
9. **Hardware validation**
   - **Done:** ESP32-P4 `mipidsi` + LVGL soft-reset (`lv_test_timer`); Qualia S3
     `dotclockframebuffer.DotClockFramebuffer` + touch (`lv_test_timer`) — see
     [soft-reset-and-bring-up.md](soft-reset-and-bring-up.md)
   - **Pending:** RK043 (mimxrt eLCDIF), RT1170 DSI, Pico DVI full panel soak
10. Lifecycle / soft-reset registry for all host-owning backends — **done**
    ([idempotent-lifecycle.md](idempotent-lifecycle.md))
11. mimxrt i80bus: board-specific pydevices config, optional DMA bulk path — pending
12. `displaydev`: remove legacy `RGBDisplay` package — done in pydevices
