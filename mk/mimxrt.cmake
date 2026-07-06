# mimxrt CMake glue (ports/mimxrt/).

add_library(displayif_mimxrt INTERFACE)
target_include_directories(displayif_mimxrt INTERFACE ${DISPLAYIF_MOD_DIR}/ports/mimxrt)
target_link_libraries(usermod INTERFACE displayif_mimxrt)
