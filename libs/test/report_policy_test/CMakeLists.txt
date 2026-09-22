enable_testing()

add_executable(report_policy_test report_policy_test.cpp)

target_link_libraries(report_policy_test
  GTest::gtest_main
  report_policy
)

include(GoogleTest)
gtest_discover_tests(report_policy_test)
