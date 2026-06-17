@echo off
setlocal
call "%~dp0upload.bat" factory
exit /b %ERRORLEVEL%
