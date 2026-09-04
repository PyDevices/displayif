# jpegio CMake glue (see micropython.mk for the variables and the Phase 2 note).

set(JPEGIO_DIR ${DISPLAYIF_MOD_DIR}/src/jpegio)
if(NOT DEFINED JPEGIO_VENDOR_TJPGD)
    set(JPEGIO_VENDOR_TJPGD 1)
endif()

add_library(displayif_jpegio INTERFACE)
target_include_directories(displayif_jpegio INTERFACE
    ${JPEGIO_DIR}/tjpgd
)
target_link_libraries(usermod INTERFACE displayif_jpegio)

target_sources(displayif_jpegio INTERFACE
    ${JPEGIO_DIR}/jpegio.c
)

if(JPEGIO_VENDOR_TJPGD)
    target_sources(displayif_jpegio INTERFACE
        ${JPEGIO_DIR}/tjpgd/tjpgd.c
    )
endif()
