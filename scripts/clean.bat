@echo off
setlocal
set "ROOT=%~dp0.."
if exist "%ROOT%\build" rmdir /s /q "%ROOT%\build"
if exist "%ROOT%\dist" rmdir /s /q "%ROOT%\dist"
mkdir "%ROOT%\dist\platform-tools" >nul 2>nul
type nul > "%ROOT%\dist\.gitkeep"
type nul > "%ROOT%\dist\platform-tools\.gitkeep"
echo Clean complete.
