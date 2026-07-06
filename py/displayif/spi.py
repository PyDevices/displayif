"""Universal SPI display bus (Phase 1).

Native implementation: ``displayif.spi`` C module when built with ports/common/spi/.
"""

try:
    from displayif._spi import SPIDisplayBus  # native submodule (future)
except ImportError:
    SPIDisplayBus = None  # type: ignore[misc, assignment]

__all__ = ("SPIDisplayBus",)
