# mimxrt CMake glue.

add_library(displayif_mimxrt INTERFACE)
target_include_directories(displayif_mimxrt INTERFACE ${DISPLAYIF_MOD_DIR}/ports/mimxrt)
target_link_libraries(usermod INTERFACE displayif_mimxrt)

if(BOARD MATCHES "TEENSY4.*|MIMXRT1060_EVK")
    target_sources(displayif_mimxrt INTERFACE
        ${DISPLAYIF_MOD_DIR}/ports/mimxrt/rgbmatrix_pm.c
    )
endif()
