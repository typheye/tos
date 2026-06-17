@echo off
setlocal EnableExtensions DisableDelayedExpansion

cd /d "%~dp0.."
if errorlevel 1 (
  echo [ERROR] Cannot enter project root: %~dp0..
  exit /b 2
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0export_partitions.ps1" %*
exit /b %ERRORLEVEL%
