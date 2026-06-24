@echo off
setlocal EnableDelayedExpansion
set "IDF_ROOT=C:\Users\HP\esp\esp-idf"
set "PROJECT_DIR=C:\Users\HP\Desktop\ESP32 Wroom\xiaozhi_like"
set "LOG=%PROJECT_DIR%\flash_hw_test.log"
set "PYTHONUTF8=1"

echo [flash_hw_test] Started: %DATE% %TIME% > "%LOG%"
echo IDF_ROOT: %IDF_ROOT% >> "%LOG%"
echo PROJECT_DIR: %PROJECT_DIR% >> "%LOG%"

cd /d "%IDF_ROOT%" || (echo FAILED to cd IDF_ROOT & exit /b 1)
call "%IDF_ROOT%\export.bat" >> "%LOG%" 2>&1
if errorlevel 1 (
  echo FAILED: export.bat >> "%LOG%"
  exit /b 1
)
echo export.bat OK >> "%LOG%"

cd /d "%PROJECT_DIR%" || (echo FAILED to cd PROJECT_DIR & exit /b 1)

echo [flash] idf.py -p COM6 -b 921600 flash >> "%LOG%"
idf.py -p COM6 -b 921600 flash >> "%LOG%" 2>&1
set RC=%ERRORLEVEL%
echo flash RC=%RC% >> "%LOG%"
echo Finished: %DATE% %TIME% >> "%LOG%"

endlocal & exit /b %RC%
