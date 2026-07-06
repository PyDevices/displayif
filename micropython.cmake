# MicroPython CMake glue for displayif (ESP32, RP2, …).
#
# Included from cmods/micropython.cmake when displayif is a workspace sibling.

set(DISPLAYIF_MOD_DIR ${CMAKE_CURRENT_LIST_DIR})

include(${DISPLAYIF_MOD_DIR}/mk/detect.cmake)

# Universal drivers (SPI, …)
include(${DISPLAYIF_MOD_DIR}/mk/common.cmake)

if(DISPLAYIF_PORT_ESP32)
    include(${DISPLAYIF_MOD_DIR}/mk/esp_idf.cmake)
endif()

if(DISPLAYIF_PORT_IMXRT)
    include(${DISPLAYIF_MOD_DIR}/mk/imxrt.cmake)
endif()
