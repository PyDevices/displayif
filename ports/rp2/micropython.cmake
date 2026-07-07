# rp2 CMake glue.

add_library(displayif_rp2 INTERFACE)
target_include_directories(displayif_rp2 INTERFACE
    ${DISPLAYIF_MOD_DIR}/ports/rp2
    ${MICROPY_PORT_DIR}
)
target_link_libraries(usermod INTERFACE displayif_rp2)

target_sources(displayif_rp2 INTERFACE
    ${DISPLAYIF_MOD_DIR}/ports/rp2/rgbmatrix_pm.c
)

target_link_libraries(displayif_rp2 INTERFACE
    hardware_pwm
    hardware_irq
    pico_stdlib
)
