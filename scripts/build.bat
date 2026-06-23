@echo off
setlocal
set "INSTALL_ARG="
if /I "%~1"=="--install" set "INSTALL_ARG=-Install"

if /I "%CONDA_DEFAULT_ENV%"=="tos" (
    powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build.ps1" %INSTALL_ARG%
    exit /b %ERRORLEVEL%
)

where conda >nul 2>nul
if errorlevel 1 (
    echo Activate the tos Conda environment or run scripts\setup-conda.bat first.
    exit /b 1
)

call conda run --no-capture-output -n tos powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build.ps1" %INSTALL_ARG%
exit /b %ERRORLEVEL%
