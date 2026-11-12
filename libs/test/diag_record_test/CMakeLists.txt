enable_testing()

add_executable(diag_record_test diag_record_test.cpp)

target_link_libraries(diag_record_test
  GTest::gtest_main
  diag_record
)

include(GoogleTest)
gtest_discover_tests(diag_record_test)
