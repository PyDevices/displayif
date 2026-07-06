# mimxrt port drivers

Targets MicroPython and CircuitPython **mimxrt** port (Teensy 4.x, M7 EVKs, etc.).

Drivers here use NXP blocks (LCDIF, ELCDIF, FlexIO, …) and compile only when `mk/mimxrt.mk` is included.

Add sources when a pydisplay board config requires NXP-specific scanout.
