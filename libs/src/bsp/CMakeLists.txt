# Headers only: bsp.h is an interface, and the implementation that satisfies it
# is chosen by whoever links (bsp_stm32.c on the target, bsp_mock.c here).
add_library (bsp_interface INTERFACE)

target_include_directories(bsp_interface
  INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)
