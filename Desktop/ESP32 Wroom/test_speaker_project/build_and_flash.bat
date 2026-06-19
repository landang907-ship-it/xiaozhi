@echo off
setlocal
set "PYTHONUTF8=0"
set "PYTHONIOENCODING=utf-8"
set "PYTHON=C:\Users\HP\.espressif\python_env\idf5.4_py3.14_env\Scripts\python.exe"
set "PATH=C:\Users\HP\.espressif\python_env\idf5.4_py3.14_env\Scripts;%PATH%"
call "C:\Users\HP\esp\esp-idf\export.bat" >nul 2>&1
if errorlevel 1 (echo [FAIL] export.bat& exit /b 1)
cd /d "%~dp0"
set "PORT=%~1"
if "%PORT%"=="" set "PORT=COM6"
call idf.py build 2>&1
if errorlevel 1 (echo [FAIL] build& exit /b 1)
call idf.py -p %PORT% -b 921600 flash monitor 2>&1
echo EXITCODE=%errorlevel%