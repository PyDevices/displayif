"""RGB666 dotclock framebuffer (ESP-IDF only — Phase 2)."""

try:
    from displayif._rgbframebuffer import RGBFrameBuffer  # native on esp32
except ImportError as exc:
    raise NotImplementedError(
        "displayif.rgbframebuffer requires ESP-IDF build (ESP32-S3, ESP32-P4, …)"
    ) from exc

__all__ = ("RGBFrameBuffer",)
