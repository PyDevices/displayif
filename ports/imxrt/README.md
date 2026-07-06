# i.MX RT display drivers

Targets: **mimxrt** MicroPython port and CircuitPython `mimxrt` port (Teensy 4.x, M7 EVKs, etc.).

Drivers here use NXP blocks (LCDIF, ELCDIF, FlexIO, …) and **must not** be compiled on ESP or RP2 builds.

Gated exclusively by `mk/imxrt.mk` / `mk/imxrt_cp.mk`.

Add drivers when a pydisplay board config requires NXP-specific scanout.
