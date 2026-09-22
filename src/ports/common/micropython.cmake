# MCU-shared CMake interface library (see root micropython.cmake).

add_library(displayif_common INTERFACE)
target_include_directories(displayif_common INTERFACE
    ${DISPLAYIF_MOD_DIR}/src/include
    ${DISPLAYIF_MOD_DIR}/src/ports/common
)
target_compile_options(displayif_common INTERFACE -Wno-unused-function)
target_compile_definitions(displayif_common INTERFACE
    DISPLAYIF_WRAP_GC_SWEEP=1
    DISPLAYIF_WRAP_MP_DEINIT=1
)
# Soft-reset: tear down host resources before gc_sweep_all; mp_deinit is a
# second idempotent pass (see src/ports/common/soft_reset.c).
target_link_options(displayif_common INTERFACE
    -Wl,--wrap=gc_sweep_all
    -Wl,--wrap=mp_deinit
)
target_link_libraries(usermod INTERFACE displayif_common)

target_sources(displayif_common INTERFACE
    ${DISPLAYIF_MOD_DIR}/src/ports/common/build.c
    ${DISPLAYIF_MOD_DIR}/src/ports/common/mp_helpers.c
    ${DISPLAYIF_MOD_DIR}/src/ports/common/soft_reset.c
    ${DISPLAYIF_MOD_DIR}/src/ports/common/spi/mod_spibus.c
    ${DISPLAYIF_MOD_DIR}/src/ports/common/i2c/mod_i2cbus.c
    ${DISPLAYIF_MOD_DIR}/src/ports/common/rgbmatrix/mod_rgbmatrix.c
)

if(DISPLAYIF_RGBMATRIX_USE_PROTOMATTER)
    target_include_directories(displayif_common INTERFACE
        ${DISPLAYIF_MOD_DIR}/src/ports/common/rgbmatrix/protomatter
    )
    target_compile_definitions(displayif_common INTERFACE
        DISPLAYIF_RGBMATRIX_USE_PROTOMATTER=1
    )
    # CIRCUITPY is scoped inside protomatter_mp.c — do not set it INTERFACE-wide.
    target_sources(displayif_common INTERFACE
        ${DISPLAYIF_MOD_DIR}/src/ports/common/rgbmatrix/protomatter_mp.c
    )
endif()

# --- which displayif this firmware was built from ---------------------------
# Computed at build time from this repo's own git, never stored; "unknown" when
# there is no git (a tarball). Read on a target as <module>.__revision__.
execute_process(
    COMMAND git -C ${DISPLAYIF_MOD_DIR} describe --always --dirty --abbrev=7
    OUTPUT_VARIABLE DISPLAYIF_REVISION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
if(NOT DISPLAYIF_REVISION)
    set(DISPLAYIF_REVISION "unknown")
endif()
target_compile_definitions(displayif_common INTERFACE DISPLAYIF_REVISION=\"${DISPLAYIF_REVISION}\")
