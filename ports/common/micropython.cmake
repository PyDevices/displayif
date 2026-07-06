# MCU-shared CMake interface library (see root micropython.cmake).

add_library(displayif_common INTERFACE)
target_include_directories(displayif_common INTERFACE
    ${DISPLAYIF_MOD_DIR}/include
    ${DISPLAYIF_MOD_DIR}/ports/common
)
target_compile_options(displayif_common INTERFACE -Wno-unused-function)
target_link_libraries(usermod INTERFACE displayif_common)

target_sources(displayif_common INTERFACE
    ${DISPLAYIF_MOD_DIR}/ports/common/mp_helpers.c
    ${DISPLAYIF_MOD_DIR}/ports/common/spi/mod_spibus.c
    ${DISPLAYIF_MOD_DIR}/ports/common/i2c/mod_i2cbus.c
    ${DISPLAYIF_MOD_DIR}/ports/common/rgbmatrix/mod_rgbmatrix.c
)
