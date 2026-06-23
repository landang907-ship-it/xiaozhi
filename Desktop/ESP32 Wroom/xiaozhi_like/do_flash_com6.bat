@echo off
setlocal
set "IDF_ROOT=C:\Users\HP\esp\esp-idf"
set "PROJECT_DIR=C:\Users\HP\Desktop\ESP32 Wroom\xiaozhi_like"
set "PYTHONUTF8=1"
set "PYTHONIOENCODING=utf-8"

cd /d "%IDF_ROOT%" || (echo IDF_ROOT not found & exit /b 1)
call "%IDF_ROOT%\export.bat" >nul 2>&1
if errorlevel 1 (echo export.bat FAILED & exit /b 1)

cd /d "%PROJECT_DIR%" || (echo PROJECT_DIR not found & exit /b 1)

echo =======================================
echo Flashing xiaozhi_like to COM6...
echo =======================================
idf.py -p COM6 flash
set RC=%ERRORLEVEL%
echo Flash RC=%RC%
if "%RC%"=="0" (echo OK - now reset board manually) else (echo FAILED)
echo.
if "%RC%"=="0" (
  echo To see log, run: idf.py -p COM6 monitor
)
endlocal & exit /b %RC%