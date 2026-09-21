@echo off
setlocal
cd /d "%~dp0"
python -X utf8 "%~dp0tools\package_title.py" %*
set "MGO2MT_BUILD_RESULT=%ERRORLEVEL%"
if not "%MGO2MT_BUILD_RESULT%"=="0" echo Build failed. Please read the error above.
pause
exit /b %MGO2MT_BUILD_RESULT%
