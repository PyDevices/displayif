"""Verify displayif modules expose idempotent lifecycle methods on their types.

This test runs without display hardware: it imports modules that exist on the
current port and checks that constructible types declare ``deinit`` (and usually
``__del__``) on their MicroPython type locals.

Hardware-backed behaviour (double deinit, soft reset + reconstruct, DMA/PIO
teardown) is covered by manual smoke scripts listed in tests/README.md.
"""

import sys


def _type_has_method(type_obj, name):
    """Return True if name appears on the type (locals_dict or attr)."""
    if hasattr(type_obj, name):
        return True
    try:
        return name in dir(type_obj)
    except TypeError:
        return False


def _check_type(module_name, type_name, required=("deinit",)):
    mod = __import__(module_name)
    cls = getattr(mod, type_name)
    missing = [n for n in required if not _type_has_method(cls, n)]
    if missing:
        raise AssertionError(
            f"{module_name}.{type_name} missing lifecycle methods: {missing}"
        )
    print(f"ok: {module_name}.{type_name} has {', '.join(required)}")


def _try_import(module_name, checks):
    try:
        __import__(module_name)
    except ImportError:
        print(f"skip: {module_name} not on {sys.platform}")
        return
    for type_name, required in checks:
        _check_type(module_name, type_name, required)


print("displayif lifecycle API test on", sys.platform)

_try_import("spibus", [("SPIBus", ("deinit", "__del__"))])
_try_import("i2cbus", [("I2CBus", ("deinit", "__del__"))])
_try_import("i80bus", [("I80Bus", ("deinit", "__del__"))])
_try_import("displayif", [("DotClockFramebuffer", ("deinit", "__del__"))])
_try_import("rgbmatrix", [("RGBMatrix", ("deinit", "__del__"))])
_try_import("picodvi", [("Framebuffer", ("deinit", "__del__"))])
_try_import("mipidsi", [
    ("Bus", ("deinit", "__del__")),
    ("Display", ("deinit", "__del__")),
])

print("lifecycle API checks passed")
