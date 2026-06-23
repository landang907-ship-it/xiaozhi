@echo off
setlocal
set "PYTHONUTF8=0"
set "PYTHONIOENCODING=utf-8"
set "PYTHON=C:\Users\HP\.espressif\python_env\idf5.4_py3.14_env\Scripts\python.exe"
set "PATH=C:\Users\HP\.espressif\python_env\idf5.4_py3.14_env\Scripts;%PATH%"
call "C:\Users\HP\esp\esp-idf\export.bat" >nul 2>&1
cd /d "%~dp0"
call idf.py -p COM6 -b 921600 flash
echo EXITCODE=%errorlevel%
