# ESP-IDF CMake glue.

add_library(displayif_esp_idf INTERFACE)
target_include_directories(displayif_esp_idf INTERFACE ${DISPLAYIF_MOD_DIR}/ports/esp_idf)
target_link_libraries(usermod INTERFACE displayif_esp_idf)

# Phase 2: uncomment when ports/esp_idf/*.c exist.
# target_sources(displayif_esp_idf INTERFACE
#     ${DISPLAYIF_MOD_DIR}/ports/esp_idf/rgb565_panel.c
#     ${DISPLAYIF_MOD_DIR}/ports/esp_idf/rgb_framebuffer.c
# )
