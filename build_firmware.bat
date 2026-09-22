@echo off
rem Headless build of the temp_sensor firmware (Debug config) using
rem STM32CubeIDE's managed build -- the same configuration the IDE uses.
rem The ELF and map land in temp_sensor_STM32L010F4\Debug\.
rem
rem The Debug configuration is built with -Os, not -O0.  That is not a
rem preference: the part has 16 KB of flash and the CubeMX skeleton alone is
rem 14.1 KB at -O0.  See design/firmware_design.md.
rem
rem Usage:  build_firmware.bat

setlocal

set CUBEIDE=C:\ST\STM32CubeIDE_1.16.1\STM32CubeIDE
set WORKSPACE=%TEMP%\temp_sensor_cubews
set REPO=%~dp0

if not exist "%CUBEIDE%\headless-build.bat" (
    echo STM32CubeIDE not found at %CUBEIDE% -- adjust CUBEIDE in this script.
    exit /b 1
)

call "%CUBEIDE%\headless-build.bat" -data "%WORKSPACE%" ^
    -import "%REPO%temp_sensor_STM32L010F4" ^
    -cleanBuild temp_sensor/Debug

endlocal
