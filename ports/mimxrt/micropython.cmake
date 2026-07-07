# mimxrt CMake glue.

add_library(displayif_mimxrt INTERFACE)
target_include_directories(displayif_mimxrt INTERFACE ${DISPLAYIF_MOD_DIR}/ports/mimxrt)
target_link_libraries(usermod INTERFACE displayif_mimxrt)

set(DISPLAYIF_NXP_SDK "${MICROPY_DIR}/lib/nxp_driver/sdk")

target_sources(displayif_mimxrt INTERFACE
    ${DISPLAYIF_MOD_DIR}/ports/common/notimpl/mod_rgbframebuffer.c
    ${DISPLAYIF_MOD_DIR}/ports/common/notimpl/mod_mipidsi.c
    ${DISPLAYIF_MOD_DIR}/ports/common/notimpl/mod_i80bus.c
)

target_compile_definitions(displayif_mimxrt INTERFACE
    DISPLAYIF_STUB_RGBFRAMEBUFFER_MSG="LCDIF rgbframebuffer not implemented on mimxrt"
    DISPLAYIF_STUB_I80BUS_MSG="FlexIO i80bus not implemented on mimxrt"
)

if(BOARD MATCHES "MIMXRT1170|PHYBOARD_RT1170")
    target_sources(displayif_mimxrt INTERFACE
        ${DISPLAYIF_MOD_DIR}/ports/mimxrt/mod_mipidsi.c
        ${DISPLAYIF_MOD_DIR}/ports/mimxrt/mimxrt1176_dsi_display.c
        ${DISPLAYIF_NXP_SDK}/drivers/mipi_dsi_split/fsl_mipi_dsi.c
        ${DISPLAYIF_NXP_SDK}/drivers/lcdifv2/fsl_lcdifv2.c
    )
    target_include_directories(displayif_mimxrt INTERFACE
        ${DISPLAYIF_NXP_SDK}/drivers/mipi_dsi_split
        ${DISPLAYIF_NXP_SDK}/drivers/lcdifv2
        ${DISPLAYIF_NXP_SDK}/devices/MIMXRT1176/drivers
    )
else()
    target_compile_definitions(displayif_mimxrt INTERFACE
        DISPLAYIF_STUB_MIPIDSI_MSG="MIPI DSI not available on this mimxrt SoC"
    )
endif()

if(BOARD MATCHES "MIMXRT1060_EVK")
    target_sources(displayif_mimxrt INTERFACE
        ${DISPLAYIF_MOD_DIR}/ports/mimxrt/mod_rgbframebuffer_elcdif.c
        ${DISPLAYIF_MOD_DIR}/ports/mimxrt/mimxrt1060_lcd_pins.c
        ${DISPLAYIF_NXP_SDK}/drivers/elcdif/fsl_elcdif.c
    )
    target_include_directories(displayif_mimxrt INTERFACE
        ${DISPLAYIF_NXP_SDK}/drivers/elcdif
    )
endif()

if(BOARD MATCHES "TEENSY4.*|MIMXRT1060_EVK")
    target_sources(displayif_mimxrt INTERFACE
        ${DISPLAYIF_MOD_DIR}/ports/mimxrt/rgbmatrix_pm.c
    )
endif()
