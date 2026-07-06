# Universal CMake interface library for displayif.

add_library(displayif_common INTERFACE)
target_include_directories(displayif_common INTERFACE ${DISPLAYIF_MOD_DIR}/include)
target_link_libraries(usermod INTERFACE displayif_common)

# Phase 1: SPI — uncomment when drivers/spi/*.c exist.
# target_sources(displayif_common INTERFACE
#     ${DISPLAYIF_MOD_DIR}/drivers/spi/mod_spi.c
#     ${DISPLAYIF_MOD_DIR}/drivers/spi/spi_bus.c
# )
