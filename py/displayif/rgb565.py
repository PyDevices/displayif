"""RGB565 parallel panel (ESP-IDF only — Phase 2)."""

try:
    from displayif._rgb565 import RGB565Panel  # native on esp32
except ImportError as exc:
    raise NotImplementedError(
        "displayif.rgb565 requires ESP-IDF build (ESP32-S3, ESP32-P4, …)"
    ) from exc

__all__ = ("RGB565Panel",)
