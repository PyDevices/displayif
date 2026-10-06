# jpegio CMake glue (see micropython.mk for what the variables mean).

set(JPEGIO_DIR ${DISPLAYIF_MOD_DIR}/src/jpegio)

add_library(displayif_jpegio INTERFACE)
target_include_directories(displayif_jpegio INTERFACE
    ${JPEGIO_DIR}/tjpgd
)
target_link_libraries(usermod INTERFACE displayif_jpegio)

# The vendored TJpgDec is the firmware's only one (LVGL's is off by config).
target_sources(displayif_jpegio INTERFACE
    ${JPEGIO_DIR}/jpegio.c
    ${JPEGIO_DIR}/tjpgd/tjpgd.c
)

# LVGL image decoder on this TJpgDec, only beside the lvgl-micropython usermod
# (D4: displayif self-detects). CMake has no sibling scan: the default is a
# lvgl-micropython/micropython.cmake in the directory this repo sits in (the
# textual parent, the same "workspace" py.mk globs -- not resolved through a
# symlink), or an lv_micropython target already defined by an lvgl-micropython
# listed earlier in a semicolon-separated USER_C_MODULES. Pass
# -DJPEGIO_LVGL=ON/OFF to decide by hand (the list form with lvgl-micropython
# elsewhere needs ON plus JPEGIO_LVGL_BINDINGS_DIR).
# The LVGL image decoder (lvgl_decoder.c) is built only when lvgl-micropython is
# in this build, decided from USER_C_MODULES the way micropython.mk decides it:
# what is on disk beside displayif says nothing about what this build selected,
# and jpegio has to build without LVGL. Override with -DJPEGIO_LVGL=ON/OFF.
# The decoder includes "lvgl/lvgl.h" and "lvgl/src/...", which resolve through
# the bindings directory lvgl-micropython itself puts on the include path, so
# nothing here needs to know where the bindings are.
if(NOT DEFINED JPEGIO_LVGL)
    if("${USER_C_MODULES}" MATCHES "lvgl-micropython")
        set(JPEGIO_LVGL ON)
    else()
        set(JPEGIO_LVGL OFF)
    endif()
endif()

if(JPEGIO_LVGL)
    target_compile_definitions(displayif_jpegio INTERFACE JPEGIO_LVGL_DECODER=1)
    # No qstrs of its own (the Python-visible names live in jpegio.c).
    target_sources(displayif_jpegio INTERFACE
        ${JPEGIO_DIR}/lvgl_decoder.c
    )
endif()
