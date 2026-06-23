@echo off
setlocal
set "IDF_ROOT=C:\Users\HP\esp\esp-idf"
set "PROJECT_DIR=C:\Users\HP\Desktop\ESP32 Wroom\test_speaker_project"
set "PYTHONUTF8=1"
set "PYTHONIOENCODING=utf-8"

cd /d "%IDF_ROOT%" || (echo IDF_ROOT not found & exit /b 1)
call "%IDF_ROOT%\export.bat" >nul 2>&1
if errorlevel 1 (echo export.bat FAILED & exit /b 1)

cd /d "%PROJECT_DIR%" || (echo PROJECT_DIR not found & exit /b 1)

echo =======================================
echo Building test_speaker_project...
echo =======================================
idf.py set-target esp32s3
if errorlevel 1 (echo set-target FAILED & exit /b 1)

idf.py build
if errorlevel 1 (echo BUILD FAILED & exit /b 1)

echo =======================================
echo Flashing test_speaker_project to COM6...
echo =======================================
idf.py -p COM6 flash
if errorlevel 1 (echo FLASH FAILED & exit /b 1)

echo.
echo ===== DONE =====
echo Sau khi reset board, loa phai phat 2 tieng bip.
echo Mo monitor: idf.py -p COM6 monitor
echo Lenh test: tone 440 1000 / beep 3 / all
endlocal