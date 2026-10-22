@echo off
setlocal
"%~dp0cproject.exe" new --interactive %*
set "EXIT_CODE=%ERRORLEVEL%"
echo.
pause
exit /b %EXIT_CODE%
