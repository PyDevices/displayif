# Portable CMake interface library.

add_library(displayif_common INTERFACE)
target_include_directories(displayif_common INTERFACE
    ${DISPLAYIF_MOD_DIR}/include
    ${DISPLAYIF_MOD_DIR}/ports/common
)
target_link_libraries(usermod INTERFACE displayif_common)

# Phase 1: SPI — uncomment when ports/common/spi/*.c exist.
# target_sources(displayif_common INTERFACE
#     ${DISPLAYIF_MOD_DIR}/ports/common/spi/mod_spi.c
#     ${DISPLAYIF_MOD_DIR}/ports/common/spi/spi_bus.c
# )
