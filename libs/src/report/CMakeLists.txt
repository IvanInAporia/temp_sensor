# Pure decision logic over a stream of samples -- no BSP, no clock.
add_library (report_policy "report_policy.c")

target_include_directories(report_policy
  INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)
