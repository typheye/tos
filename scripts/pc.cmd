@echo off
setlocal
if defined TOS_PYTHON goto explicit_python
python -c "import sys; sys.exit(sys.version_info < (3, 11))" >nul 2>nul
if not errorlevel 1 goto python
py -3 -c "import sys; sys.exit(sys.version_info < (3, 11))" >nul 2>nul
if not errorlevel 1 goto py
where conda >nul 2>nul
if not errorlevel 1 goto conda
echo Python 3.11+ or Conda is required. 1>&2
exit /b 1
:explicit_python
"%TOS_PYTHON%" "%~dp0manage.py" %*
exit /b %ERRORLEVEL%
:python
python "%~dp0manage.py" %*
exit /b %ERRORLEVEL%
:py
py -3 "%~dp0manage.py" %*
exit /b %ERRORLEVEL%
:conda
call conda run --no-capture-output -n base python "%~dp0manage.py" %*
exit /b %ERRORLEVEL%
