"""Smoke test for i80bus import (esp32-S3 with I80 LCD peripheral)."""

import sys

print("displayif i80bus smoke test")

try:
    from i80bus import I80Bus
except ImportError:
    print("i80bus not available on this SoC")
else:
    print("I80Bus", I80Bus)
    print("i80bus ok on", sys.platform)
