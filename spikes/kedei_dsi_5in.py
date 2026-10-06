# KeDei 5" DSI (Raspberry Pi-style, 800x480) on the Waveshare ESP32-P4
# WIFI6 DEV-KIT. WORKS (Brad saw it on the glass, 2026-10-06).
#
# What the panel is: a Chipone ICN6211 DSI-to-RGB bridge (it answers vendor
# 0xC1, device 0x62 0x11) that the panel's own microcontroller configures; that
# controller sits at I2C 0x45 on GPIO 7/8 and speaks the official Pi display's
# protocol (0x80 reads 0xC3; 0x85 powers the panel; 0x86 is the backlight). It
# also acknowledges every I2C address, so a bus scan finds nothing useful.
#
# What it takes, every part found by experiment on this spike branch:
#   - non-burst video with sync pulses and a continuous HS clock (ESP-IDF
#     defaults to burst and lets the clock lane idle; with those the bridge
#     produces nothing and the panel stays white);
#   - one lane, the bridge's own timing from its registers 0x20-0x29 (Linux's
#     TC358762 timing for the official display shears and repeats the frame);
#   - no bridge setup writes: the controller has already configured it;
#   - the panel powered on (0x85) after DSI video starts.
# 33 MHz pixel clock at 16-bit (528 Mbps) is what was shown working.
import time
from machine import SoftI2C, Pin
from mipidsi import Bus, Display

PCLK = globals().get("PCLK", 33_000_000)
DEPTH = globals().get("DEPTH", 16)
i2c = SoftI2C(sda=Pin(7), scl=Pin(8), freq=100000)
T = dict(width=800, height=480, pixel_clock_frequency=PCLK,
         hsync_front_porch=46, hsync_pulse_width=20, hsync_back_porch=210,
         vsync_front_porch=22, vsync_pulse_width=10, vsync_back_porch=23,
         non_burst=True, continuous_clock=True, dsi_color_depth=DEPTH)
i2c.writeto_mem(0x45, 0x85, b"\x00")
time.sleep_ms(300)
bus = Bus(frequency=PCLK * DEPTH, num_lanes=1, ldo_chan=3, ldo_voltage_mv=2500)
fb = Display(bus, b"", **T)
time.sleep_ms(100)
i2c.writeto_mem(0x45, 0x85, b"\x01")
time.sleep_ms(300)
i2c.writeto_mem(0x45, 0x86, b"\xff")
fb.fill_rect(0, 0, 400, 240, 0xF800)
fb.fill_rect(400, 0, 400, 240, 0x07E0)
fb.fill_rect(0, 240, 400, 240, 0x001F)
fb.fill_rect(400, 240, 400, 240, 0xFFFF)
for x, y, w, h in ((0, 0, 800, 1), (0, 479, 800, 1), (0, 0, 1, 480), (799, 0, 1, 480)):
    fb.fill_rect(x, y, w, h, 0xFFFF)
fb.fill_rect(20, 20, 40, 40, 0xFFE0)
fb.refresh()
print("SENT match", PCLK, DEPTH)
