enable_testing()

add_executable(battery_test battery_test.cpp)

target_link_libraries(battery_test
  GTest::gtest_main
  battery
)

include(GoogleTest)
gtest_discover_tests(battery_test)
