# stm32 CMake glue — portable spibus/i2cbus; DotClock/MIPI/i80 stubbed.

add_library(displayif_stm32 INTERFACE)
target_include_directories(displayif_stm32 INTERFACE
    ${DISPLAYIF_MOD_DIR}/ports/stm32
)
target_link_libraries(usermod INTERFACE displayif_stm32)

target_sources(displayif_stm32 INTERFACE
    ${DISPLAYIF_MOD_DIR}/ports/common/notimpl/mod_dotclockframebuffer.c
    ${DISPLAYIF_MOD_DIR}/ports/common/notimpl/mod_mipidsi.c
    ${DISPLAYIF_MOD_DIR}/ports/common/notimpl/mod_i80bus.c
)

target_compile_definitions(displayif_stm32 INTERFACE
    DISPLAYIF_STUB_DOTCLOCKFRAMEBUFFER_MSG="STM32 has no displayif DotClock backend yet"
    DISPLAYIF_STUB_MIPIDSI_MSG="MIPI DSI not available on this STM32 port"
    DISPLAYIF_STUB_I80BUS_MSG="I80 parallel bus not available on this STM32 port"
)
