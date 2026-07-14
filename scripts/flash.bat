@echo off
setlocal EnableExtensions DisableDelayedExpansion

cd /d "%~dp0.."
if errorlevel 1 (
  echo [ERROR] Cannot enter project root: %~dp0..
  exit /b 2
)

set "MODE=%~1"
if not defined MODE set "MODE=system"

set "OPENOCD=C:\ProgramData\chocolatey\lib\openocd\tools\install\bin\openocd.exe"
set "OPENOCD_SPEED=%OPENOCD_SPEED%"
if not defined OPENOCD_SPEED set "OPENOCD_SPEED=50000"
set "ROOT_WIN=%CD%"
set "ROOT_OC=%CD:\=/%"
set "BUILD_WIN=%ROOT_WIN%\build\Release"
set "FLASH_WIN=%ROOT_WIN%\dist\flash"
set "BUILD_OC=%ROOT_OC%/build/Release"
set "FLASH_OC=%ROOT_OC%/dist/flash"

if not exist "%BUILD_WIN%\CMakeCache.txt" (
  echo [INFO] First-time setup: running cmake configure...
  cmake -B "%BUILD_WIN%" -DCMAKE_BUILD_TYPE=Release
  if errorlevel 1 (
    echo [ERROR] CMake configure failed.
    exit /b 4
  )
)

if not exist "%OPENOCD%" (
  echo [ERROR] OpenOCD not found:
  echo         %OPENOCD%
  exit /b 2
)

echo [INFO] Building factory_images...
cmake --build "%BUILD_WIN%" --target factory_images -- -j24
if errorlevel 1 (
  echo [ERROR] Build/export failed.
  exit /b 4
)

if /I "%MODE%"=="system" goto system
if /I "%MODE%"=="runtime" goto runtime
if /I "%MODE%"=="factory" goto factory

echo Usage: flash.bat [system^|runtime^|factory]
echo.
echo   system  - flash SYSTEM only ^(default^)
echo   runtime - flash REC + SAH + SYSTEM
echo   factory - flash ELF + SBL + TEE + REC + SAH + SYSTEM
echo             Required once when migrating from the old boot layout.
exit /b 2

:system
call :require_file "%FLASH_WIN%\system.elf"
if errorlevel 1 exit /b 3

echo [INFO] Flashing SYSTEM only...
echo [INFO] OpenOCD adapter speed: %OPENOCD_SPEED% kHz
echo [FLASH] SYSTEM %FLASH_OC%/system.elf
"%OPENOCD%" -f interface/cmsis-dap.cfg -f target/stm32f4x.cfg ^
  -c "adapter speed %OPENOCD_SPEED%" ^
  -c "init" ^
  -c "reset halt" ^
  -c "flash erase_address 0x08040000 0x000C0000" ^
  -c "flash write_image {%FLASH_OC%/system.elf}" ^
  -c "reset run" ^
  -c "shutdown"
set "RC=%ERRORLEVEL%"
goto finish

:runtime
call :require_file "%FLASH_WIN%\rec.elf"
if errorlevel 1 exit /b 3
call :require_file "%FLASH_WIN%\system.elf"
if errorlevel 1 exit /b 3

echo [INFO] Flashing REC + SAH + SYSTEM...
echo [INFO] OpenOCD adapter speed: %OPENOCD_SPEED% kHz
echo [FLASH] REC    %FLASH_OC%/rec.elf
echo [FLASH] SYSTEM %FLASH_OC%/system.elf
"%OPENOCD%" -f interface/cmsis-dap.cfg -f target/stm32f4x.cfg ^
  -c "adapter speed %OPENOCD_SPEED%" ^
  -c "init" ^
  -c "reset halt" ^
  -c "flash erase_address 0x08010000 0x00010000" ^
  -c "flash write_image {%FLASH_OC%/rec.elf}" ^
  -c "flash erase_address 0x08040000 0x000C0000" ^
  -c "flash write_image {%FLASH_OC%/system.elf}" ^
  -c "reset run" ^
  -c "shutdown"
set "RC=%ERRORLEVEL%"
goto finish

:factory
echo [WARNING] Factory mode writes immutable ELF and the complete boot layout.
if not exist "%BUILD_WIN%\CMakeCache.txt" (
  echo [INFO] First-time setup: configuring CMake...
  cmake -B "%BUILD_WIN%" -DCMAKE_BUILD_TYPE=Release
  if errorlevel 1 ( echo [ERROR] CMake configure failed. & exit /b 4 )
)
call :require_file "%FLASH_WIN%\factory.elf"
if errorlevel 1 exit /b 3
call :require_file "%FLASH_WIN%\sbl.elf"
if errorlevel 1 exit /b 3
call :require_file "%FLASH_WIN%\rec.elf"
if errorlevel 1 exit /b 3
call :require_file "%FLASH_WIN%\system.elf"
if errorlevel 1 exit /b 3

echo [INFO] Flashing complete factory image set...
echo [INFO] OpenOCD adapter speed: %OPENOCD_SPEED% kHz
echo [FLASH] SBL    %FLASH_OC%/sbl.elf
echo [FLASH] REC    %FLASH_OC%/rec.elf
echo [FLASH] SYSTEM %FLASH_OC%/system.elf
echo [FLASH] ELF    %FLASH_OC%/factory.elf
rem Program ELF last so an interrupted migration cannot boot a partial new layout.
"%OPENOCD%" -f interface/cmsis-dap.cfg -f target/stm32f4x.cfg ^
  -c "adapter speed %OPENOCD_SPEED%" ^
  -c "init" ^
  -c "reset halt" ^
  -c "flash erase_address 0x08004000 0x0000C000" ^
  -c "flash write_image {%FLASH_OC%/sbl.elf}" ^
  -c "flash erase_address 0x08010000 0x00010000" ^
  -c "flash write_image {%FLASH_OC%/rec.elf}" ^
  -c "flash erase_address 0x08040000 0x000C0000" ^
  -c "flash write_image {%FLASH_OC%/system.elf}" ^
  -c "flash write_image erase {%FLASH_OC%/factory.elf}" ^
  -c "reset run" ^
  -c "shutdown"
set "RC=%ERRORLEVEL%"
goto finish

:require_file
if not exist "%~1" goto missing_file
for %%I in ("%~1") do set "ACTUAL_SIZE=%%~zI"
if %ACTUAL_SIZE% GTR 0 exit /b 0
echo [ERROR] Required artifact is empty:
echo         %~1
exit /b 1

:missing_file
echo [ERROR] Required artifact is missing:
echo         %~1
echo.
echo Build and export first:
echo   cmake --build build\Release --target factory_images
exit /b 1

:finish
if "%RC%"=="0" (
  echo.
  echo [OK] Upload completed successfully.
) else (
  echo.
  echo [ERROR] Upload failed with exit code %RC%.
)
exit /b %RC%
