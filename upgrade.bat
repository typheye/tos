@echo off
setlocal EnableExtensions EnableDelayedExpansion

rem Incremental updater for the TOS HID driver/test patch.
rem Usage:
rem   1. Put this folder next to the project root, or run from inside project root.
rem   2. Double click upgrade.bat, or run: upgrade.bat C:\Code\tos\slave-board

set "PKG_DIR=%~dp0"
set "SRC=%PKG_DIR%payload"

if not exist "%SRC%" (
  echo [ERROR] payload folder not found: "%SRC%"
  exit /b 1
)

if "%~1"=="" (
  if exist "%CD%\CMakeLists.txt" if exist "%CD%\Core" if exist "%CD%\src" (
    set "DST=%CD%"
  ) else (
    set /p "DST=Project root path: "
  )
) else (
  set "DST=%~1"
)

if not exist "%DST%\CMakeLists.txt" (
  echo [ERROR] Not a project root: "%DST%"
  echo         Pass project root explicitly, e.g. upgrade.bat C:\Code\tos\slave-board
  exit /b 1
)

if not exist "%DST%\src" (
  echo [ERROR] src folder not found under: "%DST%"
  exit /b 1
)

echo [INFO] Package : "%PKG_DIR%"
echo [INFO] Project : "%DST%"
echo [INFO] Copying payload...

xcopy "%SRC%\*" "%DST%\" /E /I /Y >nul
if errorlevel 1 (
  echo [ERROR] Copy failed.
  exit /b 1
)

echo [OK] Update applied.
echo.
echo Next steps:
echo   1. Reconfigure/build with CMake.
echo   2. On device menu open: TOS -^> 03 HID Test.
echo   3. PC tool: python tools\hid_host_tool.py
endlocal
