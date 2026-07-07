# rp2 CMake glue.

add_library(displayif_rp2 INTERFACE)
target_include_directories(displayif_rp2 INTERFACE
    ${DISPLAYIF_MOD_DIR}/ports/rp2
    ${MICROPY_PORT_DIR}
)
target_link_libraries(usermod INTERFACE displayif_rp2)

target_sources(displayif_rp2 INTERFACE
    ${DISPLAYIF_MOD_DIR}/ports/rp2/rgbmatrix_pm.c
    ${DISPLAYIF_MOD_DIR}/ports/rp2/mod_i80bus.c
    ${DISPLAYIF_MOD_DIR}/ports/common/notimpl/mod_rgbframebuffer.c
    ${DISPLAYIF_MOD_DIR}/ports/common/notimpl/mod_mipidsi.c
    ${DISPLAYIF_MOD_DIR}/ports/common/notimpl/mod_picodvi.c
)

target_compile_definitions(displayif_rp2 INTERFACE
    DISPLAYIF_STUB_RGBFRAMEBUFFER_MSG="RP2040 has no native RGB LCD scanout"
    DISPLAYIF_STUB_MIPIDSI_MSG="RP2040 has no MIPI DSI host"
    DISPLAYIF_STUB_PICODVI_MSG="picodvi backend not implemented on rp2 yet"
)

target_link_libraries(displayif_rp2 INTERFACE
    hardware_pwm
    hardware_irq
    hardware_pio
    hardware_dma
    pico_stdlib
)
