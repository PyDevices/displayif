# ESP32 CMake glue.

add_library(displayif_esp32 INTERFACE)
target_include_directories(displayif_esp32 INTERFACE ${DISPLAYIF_MOD_DIR}/ports/esp32)
target_compile_options(displayif_esp32 INTERFACE -Wno-unused-function)
target_link_libraries(usermod INTERFACE displayif_esp32)

target_sources(displayif_esp32 INTERFACE
    ${DISPLAYIF_MOD_DIR}/ports/esp32/mod_rgbframebuffer.c
)
