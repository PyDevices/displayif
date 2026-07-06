"""Parallel RGB LCD panel (ESP32 only — Phase 2).

Drives timed RGB parallel scanout (16-bit pixels). Not to be confused with
color helpers like ``rgb565(r, g, b)`` — this is the panel interface.
"""

try:
    from displayif._rgbpanel import RGBPanel  # native on esp32
except ImportError as exc:
    raise NotImplementedError(
        "displayif.rgbpanel requires esp32 port build (ESP32-S3, ESP32-P4, …)"
    ) from exc

__all__ = ("RGBPanel",)
