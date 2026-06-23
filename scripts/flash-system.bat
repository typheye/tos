@echo off
setlocal
set "ROOT=%~dp0.."
set "PORT=%~1"
if "%PORT%"=="" set "PORT=COM8"
set "TOOL=%ROOT%\dist\platform-tools\tsblboot.exe"
set "IMAGE=%ROOT%\..\slave-board\dist\firmware\system.bin"

if not exist "%TOOL%" (
    echo tsblboot.exe not found. Run scripts\build.bat first.
    exit /b 1
)
if not exist "%IMAGE%" (
    echo SYSTEM image not found: %IMAGE%
    exit /b 1
)

"%TOOL%" --port "%PORT%" flash system "%IMAGE%"
exit /b %ERRORLEVEL%
