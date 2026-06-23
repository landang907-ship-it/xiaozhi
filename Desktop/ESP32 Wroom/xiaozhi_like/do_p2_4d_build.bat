@echo off
setlocal
set PYTHONIOENCODING=utf-8
set PYTHONUTF8=1
set IDF_PATH=C:\Users\HP\esp\esp-idf
set IDF_PYTHON_ENV=C:\Users\HP\.espressif\python_env\idf5.4_py3.11_env
set PATH=%IDF_PYTHON_ENV%\Scripts;%PATH%
call "%IDF_PATH%\export.bat" >nul
cd /d "C:\Users\HP\Desktop\ESP32 Wroom\xiaozhi_like"
echo === Running idf.py build (only bootloader stage, app already configured) ===
echo === Started: %DATE% %TIME% ===
"C:\Users\HP\.espressif\python_env\idf5.4_py3.11_env\Scripts\python.exe" "C:\Users\HP\esp\esp-idf\tools\idf.py" -B build -C build/bootloader bootloader 2>&1
set RC=%ERRORLEVEL%
echo === Done. RC=%RC% | %DATE% %TIME% ===
echo === Build.ninja top commands (bootloader) ===
"C:\Users\HP\.espressif\python_env\idf5.4_py3.11_env\Scripts\python.exe" -c "import sys; data=open(r'C:\Users\HP\Desktop\ESP32 Wroom\xiaozhi_like\build\bootloader\build.ninja','rb').read().decode('utf-8','replace'); print(data[:8000])" 2>nul | findstr /N "^" | findstr "^1:\|^2:\|^3:\|^4:\|^5:\|^6:\|^7:\|^8:\|^9:\|^10:\|^11:\|^12:\|^13:\|^14:\|^15:\|^16:\|^17:\|^18:\|^19:\|^20:" 
exit /b %RC%
