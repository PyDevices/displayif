# mimxrt CMake glue.

add_library(displayif_mimxrt INTERFACE)
target_include_directories(displayif_mimxrt INTERFACE ${DISPLAYIF_MOD_DIR}/ports/mimxrt)
target_link_libraries(usermod INTERFACE displayif_mimxrt)

target_sources(displayif_mimxrt INTERFACE
    ${DISPLAYIF_MOD_DIR}/ports/common/notimpl/mod_rgbframebuffer.c
    ${DISPLAYIF_MOD_DIR}/ports/common/notimpl/mod_mipidsi.c
    ${DISPLAYIF_MOD_DIR}/ports/common/notimpl/mod_i80bus.c
)

target_compile_definitions(displayif_mimxrt INTERFACE
    DISPLAYIF_STUB_RGBFRAMEBUFFER_MSG="LCDIF rgbframebuffer not implemented on mimxrt"
    DISPLAYIF_STUB_I80BUS_MSG="FlexIO i80bus not implemented on mimxrt"
)

if(BOARD MATCHES "MIMXRT1170")
    target_compile_definitions(displayif_mimxrt INTERFACE
        DISPLAYIF_STUB_MIPIDSI_MSG="MIPI DSI mipidsi not implemented on mimxrt1176 yet"
    )
else()
    target_compile_definitions(displayif_mimxrt INTERFACE
        DISPLAYIF_STUB_MIPIDSI_MSG="MIPI DSI not available on this mimxrt SoC"
    )
endif()

if(BOARD MATCHES "TEENSY4.*|MIMXRT1060_EVK")
    target_sources(displayif_mimxrt INTERFACE
        ${DISPLAYIF_MOD_DIR}/ports/mimxrt/rgbmatrix_pm.c
    )
endif()
