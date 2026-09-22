# The host implementation of bsp.h.  Links only the interface: it provides the
# symbols, it does not consume anyone.
add_library (bsp_mock "bsp_mock.c")

target_include_directories(bsp_mock
  INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(bsp_mock
  bsp_interface
)


# The host stand-in for the vendor Tuya MCU SDK.  PUBLIC on its own directory
# because consumers need tuya_sdk_mock.h; the SDK's own headers arrive through
# tuya_link, which is where the include path for them is set.
add_library (tuya_sdk_mock "tuya_sdk_mock.c")

target_include_directories(tuya_sdk_mock
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(tuya_sdk_mock
  PUBLIC
    tuya_link
)


# Header-only: the wire-frame builder tests stage sensor responses with.  Links
# hdc3020 for its CRC, deliberately -- see sensor_frame.h.
add_library (sensor_frame INTERFACE)

target_include_directories(sensor_frame
  INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(sensor_frame
  INTERFACE
    hdc3020
)
