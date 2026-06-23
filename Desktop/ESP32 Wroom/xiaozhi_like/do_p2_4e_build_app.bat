@echo off
setlocal
set PYTHONIOENCODING=utf-8
set PYTHONUTF8=1
set IDF_PATH=C:\Users\HP\esp\esp-idf
set IDF_PYTHON_ENV=C:\Users\HP\.espressif\python_env\idf5.4_py3.11_env
set PATH=%IDF_PYTHON_ENV%\Scripts;%PATH%
call "%IDF_PATH%\export.bat" >nul
cd /d "C:\Users\HP\Desktop\ESP32 Wroom\xiaozhi_like"
echo === Running idf.py app (after format-warning fixes) ===
echo === Started: %DATE% %TIME% ===
"C:\Users\HP\.espressif\python_env\idf5.4_py3.11_env\Scripts\python.exe" "C:\Users\HP\esp\esp-idf\tools\idf.py" -B build app 2>&1
set RC=%ERRORLEVEL%
echo === Done. RC=%RC% | %DATE% %TIME% ===
exit /b %RC%
