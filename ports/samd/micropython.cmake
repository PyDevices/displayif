# samd CMake glue.

add_library(displayif_samd INTERFACE)
target_include_directories(displayif_samd INTERFACE ${DISPLAYIF_MOD_DIR}/ports/samd)
target_link_libraries(usermod INTERFACE displayif_samd)

if(MCU_SERIES MATCHES "SAMD51")
    target_sources(displayif_samd INTERFACE
        ${DISPLAYIF_MOD_DIR}/ports/samd/rgbmatrix_pm.c
    )
    target_compile_definitions(displayif_samd INTERFACE
        __SAMD51__=1
    )
endif()
