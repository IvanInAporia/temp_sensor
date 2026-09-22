@echo off
setlocal enabledelayedexpansion

rem ===========================================================================
rem Build and run the libs/ host-side GoogleTest suite (gcc-debug preset).
rem
rem What runs here is the same C the firmware is built from -- libs/src/* and
rem temp_sensor_main/ are compiled straight into these test binaries, not
rem copied.  Only the two edges are faked: the board (test/mocks/bsp_mock.c,
rem standing in for temp_sensor_STM32L010F4/Core/Src/bsp_stm32.c) and the vendor
rem Tuya MCU SDK (test/mocks/tuya_sdk_mock.c).
rem
rem CMake/Ninja are not on PATH; they ship bundled with Visual Studio.  The GCC
rem toolchain (MSYS2 UCRT64) is pinned by the gcc-debug preset itself.
rem
rem Run this from PowerShell or cmd, NOT from Git Bash -- under an MSYS shell
rem every link step dies with a bare "collect2.exe: error: ld returned 116 exit
rem status" and no further diagnostic, because Git Bash's MSYS environment leaks
rem into the MSYS2 UCRT64 toolchain the preset pins.  Identical sources link
rem fine from PowerShell.
rem
rem Usage:
rem   build_and_test.bat                 Build everything and run all tests.
rem   build_and_test.bat <regex>         Build, then run only tests matching
rem                                      the ctest -R regex.
rem ===========================================================================

set "LIBS_DIR=%~dp0"
set "BUILD_DIR=%LIBS_DIR%out\build\gcc-debug"
set "VS_CMAKE=C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake"
set "PATH=%VS_CMAKE%\CMake\bin;%VS_CMAKE%\Ninja;%PATH%"

where cmake >nul 2>nul
if errorlevel 1 (
    echo [build_and_test] ERROR: cmake.exe not found. Expected it under:
    echo                  %VS_CMAKE%\CMake\bin
    exit /b 1
)

rem Configure on first use (or after the build cache was deleted).
if not exist "%BUILD_DIR%\build.ninja" (
    echo [build_and_test] Configuring gcc-debug preset...
    cmake --preset gcc-debug -S "%LIBS_DIR%."
    if errorlevel 1 exit /b 1
)

echo [build_and_test] Building...
cmake --build "%BUILD_DIR%"
if errorlevel 1 exit /b 1

echo [build_and_test] Running tests...
if "%~1"=="" (
    ctest --test-dir "%BUILD_DIR%" --output-on-failure
) else (
    ctest --test-dir "%BUILD_DIR%" --output-on-failure -R "%~1"
)

exit /b %errorlevel%
