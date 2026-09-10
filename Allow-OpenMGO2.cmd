@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\allow_openmgo2.ps1" -Apply
set "MGO2WIN_FIREWALL_RESULT=%ERRORLEVEL%"
if not "%MGO2WIN_FIREWALL_RESULT%"=="0" echo OpenMGO2 access setup failed or administrator consent was cancelled.
pause
exit /b %MGO2WIN_FIREWALL_RESULT%
