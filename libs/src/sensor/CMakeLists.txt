add_library (hdc3020 "hdc3020.c")

target_include_directories(hdc3020
  INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(hdc3020
  bsp_interface
)
