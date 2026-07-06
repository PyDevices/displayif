# MicroPython CMake glue for displayif (ESP32, RP2, …).
#
# Included from cmods/micropython.cmake when displayif is a workspace sibling.
# Hardware interfaces build only on MCU ports — not unix, windows, etc.

set(DISPLAYIF_MOD_DIR ${CMAKE_CURRENT_LIST_DIR})

get_filename_component(_DISPLAYIF_PORT_DIR "${MICROPY_PORT_DIR}" ABSOLUTE)

set(DISPLAYIF_PORT_ESP32 FALSE)
set(DISPLAYIF_PORT_MIMXRT FALSE)

if(_DISPLAYIF_PORT_DIR MATCHES "/ports/esp32")
    set(DISPLAYIF_PORT_ESP32 TRUE)
elseif(_DISPLAYIF_PORT_DIR MATCHES "/ports/mimxrt")
    set(DISPLAYIF_PORT_MIMXRT TRUE)
endif()

set(DISPLAYIF_IS_MCU FALSE)
if(DISPLAYIF_PORT_ESP32 OR DISPLAYIF_PORT_MIMXRT)
    set(DISPLAYIF_IS_MCU TRUE)
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
