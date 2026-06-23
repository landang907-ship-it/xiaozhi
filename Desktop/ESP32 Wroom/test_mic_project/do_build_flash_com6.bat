@echo off
setlocal
set "IDF_ROOT=C:\Users\HP\esp\esp-idf"
set "PROJECT_DIR=C:\Users\HP\Desktop\ESP32 Wroom\test_mic_project"
set "PYTHONUTF8=1"
set "PYTHONIOENCODING=utf-8"

cd /d "%IDF_ROOT%" || (echo IDF_ROOT not found & exit /b 1)
call "%IDF_ROOT%\export.bat" >nul 2>&1
if errorlevel 1 (echo export.bat FAILED & exit /b 1)

cd /d "%PROJECT_DIR%" || (echo PROJECT_DIR not found & exit /b 1)

echo =======================================
echo Building test_mic_project...
echo =======================================
idf.py set-target esp32s3
if errorlevel 1 (echo set-target FAILED & exit /b 1)

idf.py build
if errorlevel 1 (echo BUILD FAILED & exit /b 1)

echo =======================================
echo Flashing test_mic_project to COM6...
echo =======================================
idf.py -p COM6 flash
if errorlevel 1 (echo FLASH FAILED & exit /b 1)

echo.
echo ===== DONE =====
echo Mo monitor: idf.py -p COM6 monitor
echo Sau khi thay prompt "mic^>" thi go lenh:
echo   init          - khoi tao I2S mic
echo   read 1024     - doc 1024 frames, in peak + RMS
echo   watch 10      - theo doi 10 giay, canh bao khi co am thanh
echo   all           - chay toan bo test
endlocal