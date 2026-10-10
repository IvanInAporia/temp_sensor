@echo off
rem Open this project's datasheets in the Datasheet Browser.
rem
rem One copy of the app serves every project: it lives in the water_tank checkout and
rem is told which manifest to read. Nothing of it is duplicated here -- this repo
rem contributes docs\manifest.json and the digests beside it.
rem
rem Set DATASHEET_BROWSER if that checkout is not beside this one:
rem     set DATASHEET_BROWSER=D:\src\water_tank\docs\datasheet_browser

setlocal
if "%DATASHEET_BROWSER%"=="" set "DATASHEET_BROWSER=%~dp0..\..\water_tank\docs\datasheet_browser"

if not exist "%DATASHEET_BROWSER%\app.py" (
    echo The Datasheet Browser was not found at:
    echo     %DATASHEET_BROWSER%
    echo.
    echo Set DATASHEET_BROWSER to the folder holding app.py, or clone water_tank
    echo beside this repo.
    exit /b 2
)

rem Run from source rather than dist\DatasheetBrowser.exe: a build older than the
rem code cannot be told which project to open, and would quietly show water_tank's
rem datasheets instead of these. Rebuilt, the exe takes the manifest the same way:
rem     DatasheetBrowser.exe <path>\docs\manifest.json
python "%DATASHEET_BROWSER%\app.py" "%~dp0manifest.json"
endlocal
