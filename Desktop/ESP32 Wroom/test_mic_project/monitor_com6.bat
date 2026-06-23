@echo off
setlocal
set "IDF_ROOT=C:\Users\HP\esp\esp-idf"
set "PROJECT_DIR=C:\Users\HP\Desktop\ESP32 Wroom\test_mic_project"

cd /d "%IDF_ROOT%" || (echo IDF_ROOT not found & exit /b 1)
call "%IDF_ROOT%\export.bat" >nul 2>&1

cd /d "%PROJECT_DIR%" || (echo PROJECT_DIR not found & exit /b 1)

echo.
echo =======================================
echo Connecting to test_mic on COM6...
echo =======================================
echo.
echo Sau khi thay prompt "mic>" thi co the go lenh:
echo   help       - xem danh sach lenh
echo   init       - khoi tao I2S mic
echo   read 1024  - doc va in peak + RMS
echo   watch 10   - theo doi 10 giay, vo tay se thay log
echo   fft 2048   - uo tính tan so
echo.
idf.py -p COM6 monitor

endlocal
