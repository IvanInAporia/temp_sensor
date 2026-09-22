cmake_minimum_required (VERSION 3.8)

# Host-side test harness for the board-agnostic half of the firmware.
#
# Everything under src/ is the same C that is compiled for the STM32 -- the
# CubeMX project links these directories straight in (see
# ../temp_sensor_STM32L010F4/.project). Nothing here is a copy. What changes
# between the two builds is only which bsp.h implementation is linked:
# Core/Src/bsp_stm32.c on the target, test/mocks/bsp_mock.c here.

if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
# For Windows: prevent overriding the parent project's compiler/linker settings.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

project ("libs")

# Enable CTest at the top level so the root CTestTestfile.cmake is generated.
# ctest (and the VS Code Test Explorer) read tests from the build-dir root, so
# enable_testing() must run here, not only inside the test subdirectories.
enable_testing()

# The vendor Tuya MCU SDK, and the application folder, both live outside libs/.
# Named once here so no subdirectory has to spell out how many levels up they
# are.
set(TUYA_SDK_DIR "${CMAKE_SOURCE_DIR}/../tuya" CACHE PATH "Tuya MCU SDK")
set(APP_DIR      "${CMAKE_SOURCE_DIR}/../temp_sensor_main" CACHE PATH "Application")

add_subdirectory ("src")
add_subdirectory ("test")
