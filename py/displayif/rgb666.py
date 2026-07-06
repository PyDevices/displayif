"""RGB666 dotclock framebuffer (ESP32 only — Phase 2).

Matches CircuitPython ``dotclockframebuffer`` / RGB666 naming.
"""

try:
    from displayif._rgb666 import RGBFrameBuffer  # native on esp32
except ImportError as exc:
    raise NotImplementedError(
        "displayif.rgb666 requires esp32 port build (ESP32-S3, ESP32-P4, …)"
    ) from exc

__all__ = ("RGBFrameBuffer",)
