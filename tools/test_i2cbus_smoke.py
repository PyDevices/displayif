"""Smoke test for i2cbus import (MCU ports with machine.I2C)."""

import sys

print("displayif i2cbus smoke test")

from i2cbus import I2CBus

print("I2CBus", I2CBus)
print("i2cbus ok on", sys.platform)
