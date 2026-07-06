# ESP32 CMake glue (ports/esp32/).

add_library(displayif_esp32 INTERFACE)
target_include_directories(displayif_esp32 INTERFACE ${DISPLAYIF_MOD_DIR}/ports/esp32)
target_link_libraries(usermod INTERFACE displayif_esp32)

# Phase 2 — uncomment when sources land.
# target_sources(displayif_esp32 INTERFACE
#     ${DISPLAYIF_MOD_DIR}/ports/esp32/rgb_panel.c
#     ${DISPLAYIF_MOD_DIR}/ports/esp32/rgb666.c
# )
