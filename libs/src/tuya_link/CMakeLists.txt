add_library (tuya_link "tuya_link.c")

# PUBLIC on the SDK path: tuya_link.h does not expose it, but the tests need the
# same DPID_* and WIFI_* constants to assert against, and so does the SDK mock
# that stands in for mcu_api.c here.
target_include_directories(tuya_link
  PUBLIC
    "${TUYA_SDK_DIR}"
  INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

# battery: tuya_link.h takes a Battery_State. bsp_interface: the module's power
# gate and UART. The mcu_api.c side is NOT linked here -- on the host it is
# supplied by tuya_sdk_mock, on the target by the real SDK.
target_link_libraries(tuya_link
  bsp_interface
  battery
)
