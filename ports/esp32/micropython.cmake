# ESP32 CMake glue.

add_library(displayif_esp32 INTERFACE)
target_include_directories(displayif_esp32 INTERFACE ${DISPLAYIF_MOD_DIR}/ports/esp32)
target_compile_options(displayif_esp32 INTERFACE -Wno-unused-function)
target_link_libraries(usermod INTERFACE displayif_esp32)

# Soft-reset: common wraps gc_sweep_all; this file supplies displayif_port_pre_gc_sweep
# (stop machine.Timer). See ports/esp32/soft_reset_gc_sweep.c.
target_sources(displayif_esp32 INTERFACE
    ${DISPLAYIF_MOD_DIR}/ports/esp32/mod_dotclockframebuffer.c
    ${DISPLAYIF_MOD_DIR}/ports/esp32/mod_mipidsi.c
    ${DISPLAYIF_MOD_DIR}/ports/esp32/mod_i80bus.c
    ${DISPLAYIF_MOD_DIR}/ports/esp32/soft_reset_gc_sweep.c
)

# esp_lcd symbols for RGB, I80, and MIPI DSI backends.
if(CONFIG_SOC_LCD_RGB_SUPPORTED OR CONFIG_SOC_LCD_I80_SUPPORTED OR CONFIG_SOC_MIPI_DSI_SUPPORTED)
    target_link_libraries(displayif_esp32 INTERFACE idf::esp_lcd)
endif()

if(CONFIG_IDF_TARGET_ESP32S3)
    target_compile_definitions(displayif_esp32 INTERFACE ESP_PLATFORM=1)
    target_sources(displayif_esp32 INTERFACE
        ${DISPLAYIF_MOD_DIR}/ports/esp32/rgbmatrix_pm.c
    )
endif()
