"""Smoke test for mipidsi import (ESP32-P4 with MIPI DSI only).

Full panel bring-up requires the Waveshare ESP32-P4-WIFI6-Touch-LCD-4B hardware.
"""

import sys

print("displayif mipidsi smoke test")

try:
    from mipidsi import Bus, Display
except ImportError:
    print("mipidsi not available on this SoC (expected off ESP32-P4)")
    raise SystemExit(0)

assert Bus is not None
assert Display is not None
print("mipidsi ok on", sys.platform)
