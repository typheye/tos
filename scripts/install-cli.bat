@echo off
setlocal
set "ROOT=%~dp0.."
if "%CONDA_PREFIX%"=="" (
    echo Activate the tos Conda environment first.
    exit /b 1
)
if not exist "%ROOT%\dist\platform-tools\tsblboot.exe" (
    echo Build output not found. Run scripts\build.bat first.
    exit /b 1
)
copy /y "%ROOT%\dist\platform-tools\tsblboot.exe" "%CONDA_PREFIX%\Scripts\tsblboot.exe" >nul
copy /y "%ROOT%\dist\platform-tools\tdb.exe" "%CONDA_PREFIX%\Scripts\tdb.exe" >nul
if errorlevel 1 exit /b %ERRORLEVEL%
echo Installed tsblboot and tdb into %CONDA_PREFIX%\Scripts
