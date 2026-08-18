# docs/

| Doc | Contents |
|-----|----------|
| [port-matrix.md](port-matrix.md) | Module/port matrix, pydevices board-config map, RP2350 DSI notes, ESP32 PSRAM notes, per-port native module details (mimxrt, rp2, samd), build commands |
| [idempotent-lifecycle.md](idempotent-lifecycle.md) | Required `deinit` / soft-reset contract for backends owning DMA/IRQ/PIO/SDK handles |
| [soft-reset-and-bring-up.md](soft-reset-and-bring-up.md) | Proven failure modes and bring-up methods (ESP32-P4 `mipidsi`, Qualia S3 `dotclockframebuffer.DotClockFramebuffer`) |
| [ports/](ports/) | Per-port notes; currently just [ports/esp32.md](ports/esp32.md) (Qualia DotClock + P4 mipidsi behavioral notes) — see [ports/README.md](ports/README.md) for the `src/ports/` layout rules |

**Start here for agents:** [../AGENTS.md](../AGENTS.md) → [idempotent-lifecycle.md](idempotent-lifecycle.md) → [soft-reset-and-bring-up.md](soft-reset-and-bring-up.md).
