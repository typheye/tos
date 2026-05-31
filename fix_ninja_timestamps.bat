@echo off
setlocal
cd /d "%~dp0"
echo Fixing file timestamps to current local time...
powershell -NoProfile -ExecutionPolicy Bypass -Command "Get-ChildItem -LiteralPath . -Recurse -Force | ForEach-Object { try { $_.LastWriteTime = Get-Date } catch {} }; try { (Get-Item -LiteralPath .).LastWriteTime = Get-Date } catch {}"
echo Removing stale CMake/Ninja build directory...
if exist build\release rmdir /s /q build\release
if exist build\Release rmdir /s /q build\Release
if exist build\debug rmdir /s /q build\debug
if exist build\Debug rmdir /s /q build\Debug
echo Done. Reconfigure with: cmake --preset Release
echo Then build with:     cmake --build --preset Release
endlocal
