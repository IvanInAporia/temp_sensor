enable_testing()

add_executable(hdc3020_test hdc3020_test.cpp)

target_link_libraries(hdc3020_test
  GTest::gtest_main
  hdc3020
  bsp_mock
  sensor_frame
)

include(GoogleTest)
gtest_discover_tests(hdc3020_test)
