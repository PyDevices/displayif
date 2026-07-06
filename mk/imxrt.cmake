# i.MX RT CMake glue (mimxrt uses Make today; stub for future CMake ports).

add_library(displayif_imxrt INTERFACE)
target_include_directories(displayif_imxrt INTERFACE ${DISPLAYIF_MOD_DIR}/ports/imxrt)
target_link_libraries(usermod INTERFACE displayif_imxrt)
