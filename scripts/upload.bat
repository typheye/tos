@echo off
echo ========================================
echo Uploading JPStm Firmware...
echo ========================================
cd /d "%~dp0.."
echo Current directory: %CD%
echo Firmware: build\release\jpstm.elf
echo.
C:\ProgramData\chocolatey\lib\openocd\tools\install\bin\openocd.exe -f interface/cmsis-dap.cfg -f target/stm32f4x.cfg -c "program build/release/tos.elf verify" -c reset -c exit
echo.
if %errorlevel% equ 0 (
    echo ========================================
    echo Upload Successful!
    echo ========================================
) else (
    echo ========================================
    echo Upload Failed!
    echo ========================================
)