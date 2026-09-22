# A qstr must be reachable on every port that compiles the file

**The rule: never put an `MP_QSTR_` name behind a define this usermod supplies
itself.** The feature flag decides what a function *does*, never whether the
name exists. If a module exposes `I80Bus`, then `MP_QSTR_I80Bus` is compiled on
every build of that file, and the unsupported build raises instead.

Check it with:

```bash
python3 tools/check_qstr_reachability.py
```

It reads every `micropython.cmake` here for the macros we hand out as INTERFACE
defines, walks `src/`, and fails with a file and a line for any qstr that only
exists when one of them is set.

## Why — the part that makes it non-obvious

MicroPython's `py/mkrules.cmake` builds the QSTR-extraction preprocessor flags
from the **port target's** own `COMPILE_DEFINITIONS`:

```cmake
get_target_property(MICROPY_CPP_DEF ${MICROPY_TARGET} COMPILE_DEFINITIONS)
```

A usermod's `target_compile_definitions(... INTERFACE ...)` reaches the
*compile* and never reaches `makeqstrdefs.py`. So on a CMake port the two passes
disagree about the same `#if`: extraction sees the guard as false and collects
nothing; the compiler sees it as true and references names that were never
collected. The build fails with

```
error: 'MP_QSTR_register_lvgl_decoder' undeclared
```

and the fix looks like it should be in the file that broke, which is the part
that costs the time.

**The Makefile ports cannot show you this.** They put the define in
`CFLAGS_USERMOD`, `py/py.mk` folds that into `CFLAGS`, and their QSTR pass reads
`CFLAGS` — so the same source is green on unix and red on esp32. A unix build is
not evidence here.

In MicroPython v1.29.0 the CMake ports are esp32 and rp2. samd, mimxrt and stm32
carry CMake glue here for the day they gain a `CMakeLists.txt`, and build
through their Makefile today, which is why a break can sit in the tree for
months looking like nothing.

## The two ways a name survives anyway, and why neither is a plan

A guarded name is only safe if something else collects it:

1. **It is in MicroPython's static pool** (`py/qstrdefs.h`) — `__init__`,
   `__name__` and the rest. `src/jpegio/jpegio.c` relies on this for
   `MP_QSTR___init__`, which is why the checker carries a short `CORE_POOL`
   list. Add to that list only with the reason written down; the pool is
   MicroPython's, and it changes between versions.
2. **The same file references it unguarded elsewhere.**
   `src/ports/common/spi/mod_spibus.c` guards `sck`/`mosi`/`miso` behind
   `ESP_PLATFORM` at line 95 and passes them unguarded at 154 and 210, so the
   pass collects them from there.

Same **file**, not same repo — that distinction is the whole check. Every name
in `src/ports/samd/mod_i80bus.c` is also referenced by the esp32 backend and by
`src/ports/common/notimpl/mod_i80bus.c`, and neither of those is compiled into a
SAMD51 build. A repo-wide "is it referenced anywhere?" test passes the bug
this page is about.

## What it looks like when you get it right

`src/ports/samd/mod_i80bus.c` is the worked example. The implementation — the
SAMD51 struct, the `PORT->Group` register work, `sam.h` — stays behind
`DISPLAYIF_I80BUS_SAMD51`. The constructor's argument table, the method table,
the type and `MP_REGISTER_MODULE` are outside it, and the non-SAMD51 path raises
`NotImplementedError` with a sentence. Preprocessing the file the way the QSTR
pass does, with no INTERFACE define:

| | names the QSTR pass collects | names the compile references |
|---|---|---|
| before | 0 | 14 |
| after | 14 | 14 |

`src/jpegio/README.md` has the same story from the day it was found, with the
LVGL decoder's specifics.

<!-- sources: displayif#24; the jpegio fix fb4fd6a; micropython v1.29.0
py/mkrules.cmake and py/py.mk -->
