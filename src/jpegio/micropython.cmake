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
get_filename_component(_JPEGIO_WORKSPACE_DIR ${DISPLAYIF_MOD_DIR} DIRECTORY)
if(NOT DEFINED JPEGIO_LVGL)
    if(EXISTS ${_JPEGIO_WORKSPACE_DIR}/lvgl-micropython/micropython.cmake OR TARGET lv_micropython)
        set(JPEGIO_LVGL ON)
    else()
        set(JPEGIO_LVGL OFF)
    endif()
endif()

if(JPEGIO_LVGL)
    # Derived the way lvgl-micropython/micropython.cmake derives BINDINGS_DIR
    # (the workspace sibling lvgl-bindings), under a displayif-private name;
    # a BINDINGS_DIR already set (command line, or lvgl-micropython included
    # earlier) wins so both usermods see one lv_conf.h.
    if(NOT DEFINED JPEGIO_LVGL_BINDINGS_DIR)
        if(DEFINED BINDINGS_DIR)
            set(JPEGIO_LVGL_BINDINGS_DIR ${BINDINGS_DIR})
        else()
            set(JPEGIO_LVGL_BINDINGS_DIR ${_JPEGIO_WORKSPACE_DIR}/lvgl-bindings)
        endif()
    endif()
    if(NOT EXISTS ${JPEGIO_LVGL_BINDINGS_DIR}/lv_conf.h)
        message(FATAL_ERROR "jpegio: JPEGIO_LVGL is ON but ${JPEGIO_LVGL_BINDINGS_DIR}/lv_conf.h does not exist (set JPEGIO_LVGL_BINDINGS_DIR)")
    endif()
    target_compile_definitions(displayif_jpegio INTERFACE JPEGIO_LVGL_DECODER=1)
    target_include_directories(displayif_jpegio INTERFACE
        ${JPEGIO_LVGL_BINDINGS_DIR}
        ${JPEGIO_LVGL_BINDINGS_DIR}/lvgl
    )
    target_sources(displayif_jpegio INTERFACE
        ${JPEGIO_DIR}/lvgl_decoder.c
    )
endif()
