@echo off
setlocal
set "ROOT=%~dp0.."
if exist "%ROOT%\build" rmdir /s /q "%ROOT%\build"
if exist "%ROOT%\dist" rmdir /s /q "%ROOT%\dist"
echo Clean complete.
