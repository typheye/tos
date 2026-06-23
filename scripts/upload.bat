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
if not defined OPENOCD_SPEED set "OPENOCD_SPEED=16000"
set "ROOT_WIN=%CD%"
set "ROOT_OC=%CD:\=/%"
set "BUILD_WIN=%ROOT_WIN%\build\Release"
set "FW_WIN=%ROOT_WIN%\dist\firmware"
set "FLASH_WIN=%ROOT_WIN%\dist\flash"
set "FACTORY_WIN=%ROOT_WIN%\dist\factory"
set "BUILD_OC=%ROOT_OC%/build/Release"
set "FW_OC=%ROOT_OC%/dist/firmware"
set "FLASH_OC=%ROOT_OC%/dist/flash"
set "FACTORY_OC=%ROOT_OC%/dist/factory"

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

echo Usage: upload.bat [system^|runtime^|factory]
echo.
echo   system  - flash SYSTEM only ^(default^)
echo   runtime - flash REC + SAH + SYSTEM
echo   factory - flash ELF + SBL + TEE + REC + SAH + SYSTEM
echo             Required once when migrating from the old boot layout.
exit /b 2

:system
call :require_size "%FLASH_WIN%\system.bin" 524288
if errorlevel 1 exit /b 3

echo [INFO] Flashing SYSTEM only...
echo [INFO] OpenOCD adapter speed: %OPENOCD_SPEED% kHz
echo [FLASH] SYSTEM 0x08040000 %FLASH_OC%/system.bin
"%OPENOCD%" -f interface/cmsis-dap.cfg -f target/stm32f4x.cfg ^
  -c "adapter speed %OPENOCD_SPEED%" ^
  -c "init" ^
  -c "reset halt" ^
  -c "flash write_image erase {%FLASH_OC%/system.bin} 0x08040000 bin" ^
  -c "reset run" ^
  -c "shutdown"
set "RC=%ERRORLEVEL%"
goto finish

:runtime
call :require_size "%FLASH_WIN%\rec.bin" 65536
if errorlevel 1 exit /b 3
call :require_size "%FLASH_WIN%\sah.bin" 131072
if errorlevel 1 exit /b 3
call :require_size "%FLASH_WIN%\system.bin" 524288
if errorlevel 1 exit /b 3

echo [INFO] Flashing REC + SAH + SYSTEM...
echo [INFO] OpenOCD adapter speed: %OPENOCD_SPEED% kHz
echo [FLASH] REC    0x08010000 %FLASH_OC%/rec.bin
echo [FLASH] SAH    0x08020000 %FLASH_OC%/sah.bin
echo [FLASH] SYSTEM 0x08040000 %FLASH_OC%/system.bin
"%OPENOCD%" -f interface/cmsis-dap.cfg -f target/stm32f4x.cfg ^
  -c "adapter speed %OPENOCD_SPEED%" ^
  -c "init" ^
  -c "reset halt" ^
  -c "flash write_image erase {%FLASH_OC%/rec.bin} 0x08010000 bin" ^
  -c "flash write_image erase {%FLASH_OC%/sah.bin} 0x08020000 bin" ^
  -c "flash write_image erase {%FLASH_OC%/system.bin} 0x08040000 bin" ^
  -c "reset run" ^
  -c "shutdown"
set "RC=%ERRORLEVEL%"
goto finish

:factory
echo [WARNING] Factory mode writes immutable ELF and the complete boot layout.
echo [WARNING] Use this once when migrating from the old 64KB-SBL layout.
call :require_file "%FACTORY_WIN%\elf_stage.elf"
if errorlevel 1 exit /b 3
call :require_size "%FLASH_WIN%\sbl.bin" 32768
if errorlevel 1 exit /b 3
call :require_size "%FLASH_WIN%\tee.bin" 16384
if errorlevel 1 exit /b 3
call :require_size "%FLASH_WIN%\rec.bin" 65536
if errorlevel 1 exit /b 3
call :require_size "%FLASH_WIN%\sah.bin" 131072
if errorlevel 1 exit /b 3
call :require_size "%FLASH_WIN%\system.bin" 524288
if errorlevel 1 exit /b 3

echo [INFO] Flashing complete factory image set...
echo [INFO] OpenOCD adapter speed: %OPENOCD_SPEED% kHz
echo [FLASH] SBL    0x08004000 %FLASH_OC%/sbl.bin
echo [FLASH] TEE    0x0800C000 %FLASH_OC%/tee.bin
echo [FLASH] REC    0x08010000 %FLASH_OC%/rec.bin
echo [FLASH] SAH    0x08020000 %FLASH_OC%/sah.bin
echo [FLASH] SYSTEM 0x08040000 %FLASH_OC%/system.bin
echo [FLASH] ELF    elf image  %FACTORY_OC%/elf_stage.elf
rem Program ELF last so an interrupted migration cannot boot a partial new layout.
"%OPENOCD%" -f interface/cmsis-dap.cfg -f target/stm32f4x.cfg ^
  -c "adapter speed %OPENOCD_SPEED%" ^
  -c "init" ^
  -c "reset halt" ^
  -c "flash write_image erase {%FLASH_OC%/sbl.bin} 0x08004000 bin" ^
  -c "flash write_image erase {%FLASH_OC%/tee.bin} 0x0800C000 bin" ^
  -c "flash write_image erase {%FLASH_OC%/rec.bin} 0x08010000 bin" ^
  -c "flash write_image erase {%FLASH_OC%/sah.bin} 0x08020000 bin" ^
  -c "flash write_image erase {%FLASH_OC%/system.bin} 0x08040000 bin" ^
  -c "flash write_image erase {%FACTORY_OC%/elf_stage.elf}" ^
  -c "reset run" ^
  -c "shutdown"
set "RC=%ERRORLEVEL%"
goto finish

:require_file
if exist "%~1" exit /b 0
echo [ERROR] Required artifact is missing:
echo         %~1
echo.
echo Build and export first:
echo   cmake --build build\Release --target factory_images
exit /b 1

:require_size
call :require_file "%~1"
if errorlevel 1 exit /b 1
for %%I in ("%~1") do set "ACTUAL_SIZE=%%~zI"
if %ACTUAL_SIZE% LEQ 0 (
  echo [ERROR] Artifact is empty:
  echo         %~1
  exit /b 1
)
if %ACTUAL_SIZE% LEQ %~2 exit /b 0
echo [ERROR] Artifact size exceeds partition:
echo         %~1
echo         max %~2 bytes, got %ACTUAL_SIZE% bytes
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
