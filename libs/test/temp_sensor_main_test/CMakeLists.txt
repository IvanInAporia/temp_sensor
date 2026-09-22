enable_testing()

# The real cycle, compiled from where the firmware compiles it.  Everything
# under it is real too -- the sensor driver, the gauge, the report policy and
# the Tuya glue.  Only the two edges are faked: the board (bsp_mock) and the
# vendor SDK (tuya_sdk_mock).
add_executable(temp_sensor_main_test
  temp_sensor_main_test.cpp
  "${APP_DIR}/temp_sensor_main.c"
)

target_include_directories(temp_sensor_main_test
  PRIVATE
    "${APP_DIR}"
)

target_link_libraries(temp_sensor_main_test
  GTest::gtest_main
  hdc3020
  battery
  report_policy
  tuya_link
  tuya_sdk_mock
  bsp_mock
  sensor_frame
)

include(GoogleTest)
gtest_discover_tests(temp_sensor_main_test)
