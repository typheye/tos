@echo off
setlocal

echo [INFO] Reconfiguring CMake...
cmake -B "%~dp0..\build\Release" -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 (
  echo [ERROR] CMake configure failed.
  exit /b 4
)

call "%~dp0upload.bat" factory
exit /b %ERRORLEVEL%
