# samd CMake glue.

add_library(displayif_samd INTERFACE)
target_include_directories(displayif_samd INTERFACE ${DISPLAYIF_MOD_DIR}/ports/samd)
target_link_libraries(usermod INTERFACE displayif_samd)

target_sources(displayif_samd INTERFACE
    ${DISPLAYIF_MOD_DIR}/ports/common/notimpl/mod_rgbframebuffer.c
    ${DISPLAYIF_MOD_DIR}/ports/common/notimpl/mod_mipidsi.c
    ${DISPLAYIF_MOD_DIR}/ports/common/notimpl/mod_i80bus.c
)

target_compile_definitions(displayif_samd INTERFACE
    DISPLAYIF_STUB_RGBFRAMEBUFFER_MSG="SAMD has no native RGB LCD scanout"
    DISPLAYIF_STUB_MIPIDSI_MSG="MIPI DSI not available on SAMD"
    DISPLAYIF_STUB_I80BUS_MSG="I80 parallel bus not available on SAMD"
)

if(MCU_SERIES MATCHES "SAMD51")
    target_sources(displayif_samd INTERFACE
        ${DISPLAYIF_MOD_DIR}/ports/samd/rgbmatrix_pm.c
        ${DISPLAYIF_MOD_DIR}/ports/samd/samd_irq_hook.c
    )
    target_compile_definitions(displayif_samd INTERFACE
        __SAMD51__=1
    )
endif()
