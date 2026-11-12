enable_testing()

add_executable(button_debounce_test button_debounce_test.cpp)

# Header-only (libs/src/bsp/button_debounce.h): bsp_interface is its include path.
target_link_libraries(button_debounce_test
  GTest::gtest_main
  bsp_interface
)

include(GoogleTest)
gtest_discover_tests(button_debounce_test)
