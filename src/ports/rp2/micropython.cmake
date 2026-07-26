# rp2 CMake glue.

set(PICODVI_LIBDVI_DIR ${DISPLAYIF_MOD_DIR}/src/ports/rp2/picodvi/libdvi)

add_library(displayif_rp2 INTERFACE)
target_include_directories(displayif_rp2 INTERFACE
    ${DISPLAYIF_MOD_DIR}/src/ports/rp2
    ${PICODVI_LIBDVI_DIR}
    ${MICROPY_PORT_DIR}
)
target_link_libraries(usermod INTERFACE displayif_rp2)

target_sources(displayif_rp2 INTERFACE
    ${DISPLAYIF_MOD_DIR}/src/ports/rp2/rgbmatrix_pm.c
    ${DISPLAYIF_MOD_DIR}/src/ports/rp2/mod_i80bus.c
    ${DISPLAYIF_MOD_DIR}/src/ports/common/notimpl/mod_dotclockframebuffer.c
    ${DISPLAYIF_MOD_DIR}/src/ports/common/notimpl/mod_mipidsi.c
    ${DISPLAYIF_MOD_DIR}/src/ports/common/notimpl/mod_qspibus.c
    ${DISPLAYIF_MOD_DIR}/src/ports/rp2/mod_picodvi.c
)

if(PICO_RP2350)
    target_sources(displayif_rp2 INTERFACE
        ${DISPLAYIF_MOD_DIR}/src/ports/rp2/picodvi_rp2350.c
    )
else()
    target_sources(displayif_rp2 INTERFACE
        ${DISPLAYIF_MOD_DIR}/src/ports/rp2/picodvi_rp2040.c
        ${PICODVI_LIBDVI_DIR}/dvi.c
        ${PICODVI_LIBDVI_DIR}/dvi_serialiser.c
        ${PICODVI_LIBDVI_DIR}/dvi_timing.c
        ${PICODVI_LIBDVI_DIR}/tmds_encode.c
        ${PICODVI_LIBDVI_DIR}/tmds_encode.S
    )
endif()

target_compile_definitions(displayif_rp2 INTERFACE
    DISPLAYIF_STUB_DOTCLOCKFRAMEBUFFER_MSG="RP2040 has no native RGB LCD scanout"
    DISPLAYIF_STUB_MIPIDSI_MSG="RP2040 has no MIPI DSI host"
    DISPLAYIF_STUB_QSPIBUS_MSG="qspibus not supported on rp2"
)

target_link_libraries(displayif_rp2 INTERFACE
    hardware_pwm
    hardware_irq
    hardware_pio
    hardware_dma
    hardware_gpio
    hardware_interp
    hardware_sync_spin_lock
    pico_stdlib
    pico_multicore
    pico_util
)
