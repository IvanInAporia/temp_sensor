@echo off
rem Fetch the sensors' DP reports from the Tuya cloud. See README.md.
py "%~dp0tuya_logs.py" %*
rem Keep the window open when double-clicked.
if "%~1"=="" pause
