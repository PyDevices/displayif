"""Smoke test for mipidsi import (MIMXRT1176 with MIPI DSI only).

Full panel bring-up requires MIMXRT1170-EVK + Waveshare 5\" DSI on J84.
"""

import sys

print("displayif mimxrt mipidsi smoke test")

try:
    from mipidsi import Bus, Display
except ImportError:
    print("mipidsi not available on this SoC (expected off MIMXRT1176)")
    raise SystemExit(0)

assert Bus is not None
assert Display is not None
print("mipidsi ok on", sys.platform)
