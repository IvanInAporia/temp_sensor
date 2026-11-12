# Pure decision logic over a stream of samples -- no BSP, no clock.
add_library (report_policy "report_policy.c")

target_include_directories(report_policy
  INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

# The diagnostic record (DP 104). Only bsp.h's reset reasons, for their names.
add_library (diag_record "diag_record.c")

target_include_directories(diag_record
  INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(diag_record
  bsp_interface
)
