# MicroPython CMake glue for displayif (ESP32, RP2, …).
#
# Point USER_C_MODULES at this repo directly, e.g.:
#   idf.py build -DUSER_C_MODULES=/path/to/displayif/micropython.cmake
# usermod.cmake also accepts a semicolon-separated list of module paths if you
# want displayif plus other modules — no aggregator file required, e.g.:
#   -DUSER_C_MODULES="/path/to/displayif;/path/to/other_mod"
# Hardware interfaces build only on MCU ports — not unix, windows, etc.

set(DISPLAYIF_MOD_DIR ${CMAKE_CURRENT_LIST_DIR})

get_filename_component(_DISPLAYIF_PORT_DIR "${MICROPY_PORT_DIR}" ABSOLUTE)

set(DISPLAYIF_PORT_ESP32 FALSE)
set(DISPLAYIF_PORT_MIMXRT FALSE)
set(DISPLAYIF_PORT_SAMD FALSE)
set(DISPLAYIF_PORT_RP2 FALSE)

if(_DISPLAYIF_PORT_DIR MATCHES "/ports/esp32")
    set(DISPLAYIF_PORT_ESP32 TRUE)
elseif(_DISPLAYIF_PORT_DIR MATCHES "/ports/mimxrt")
    set(DISPLAYIF_PORT_MIMXRT TRUE)
elseif(_DISPLAYIF_PORT_DIR MATCHES "/ports/samd")
    set(DISPLAYIF_PORT_SAMD TRUE)
elseif(_DISPLAYIF_PORT_DIR MATCHES "/ports/rp2")
    set(DISPLAYIF_PORT_RP2 TRUE)
endif()

set(DISPLAYIF_IS_MCU FALSE)
if(DISPLAYIF_PORT_ESP32 OR DISPLAYIF_PORT_MIMXRT OR DISPLAYIF_PORT_SAMD OR DISPLAYIF_PORT_RP2)
    set(DISPLAYIF_IS_MCU TRUE)
endif()

if(DISPLAYIF_PORT_ESP32 AND CONFIG_IDF_TARGET_ESP32S3)
    set(DISPLAYIF_RGBMATRIX_USE_PROTOMATTER TRUE)
endif()

if(DISPLAYIF_PORT_MIMXRT AND BOARD MATCHES "TEENSY4.*|MIMXRT1060_EVK")
    set(DISPLAYIF_RGBMATRIX_USE_PROTOMATTER TRUE)
endif()

if(DISPLAYIF_PORT_SAMD AND MCU_SERIES MATCHES "SAMD51")
    set(DISPLAYIF_RGBMATRIX_USE_PROTOMATTER TRUE)
endif()

if(DISPLAYIF_PORT_RP2)
    set(DISPLAYIF_RGBMATRIX_USE_PROTOMATTER TRUE)
endif()

if(DISPLAYIF_IS_MCU)
    include(${DISPLAYIF_MOD_DIR}/ports/common/micropython.cmake)
endif()

if(DISPLAYIF_PORT_ESP32)
    include(${DISPLAYIF_MOD_DIR}/ports/esp32/micropython.cmake)
endif()

if(DISPLAYIF_PORT_MIMXRT)
    include(${DISPLAYIF_MOD_DIR}/ports/mimxrt/micropython.cmake)
endif()

if(DISPLAYIF_PORT_SAMD)
    include(${DISPLAYIF_MOD_DIR}/ports/samd/micropython.cmake)
endif()

if(DISPLAYIF_PORT_RP2)
    include(${DISPLAYIF_MOD_DIR}/ports/rp2/micropython.cmake)
endif()
