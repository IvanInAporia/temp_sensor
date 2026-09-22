# Pure functions over a millivolt reading -- no BSP, nothing to mock.
add_library (battery "battery.c")

target_include_directories(battery
  INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)
