# ESP32 CMake glue.

add_library(displayif_esp32 INTERFACE)
target_include_directories(displayif_esp32 INTERFACE ${DISPLAYIF_MOD_DIR}/ports/esp32)
target_compile_options(displayif_esp32 INTERFACE -Wno-unused-function)
target_link_libraries(usermod INTERFACE displayif_esp32)

target_sources(displayif_esp32 INTERFACE
    ${DISPLAYIF_MOD_DIR}/ports/esp32/mod_rgbframebuffer.c
    ${DISPLAYIF_MOD_DIR}/ports/esp32/mod_mipidsi.c
)

# MIPI DSI headers and symbols (mod_mipidsi.c is a no-op when SOC_MIPI_DSI_SUPPORTED=0).
if(CONFIG_SOC_MIPI_DSI_SUPPORTED)
    target_link_libraries(displayif_esp32 INTERFACE idf::esp_lcd)
endif()
