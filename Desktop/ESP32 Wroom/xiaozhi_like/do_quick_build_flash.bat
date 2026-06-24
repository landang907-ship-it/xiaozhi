@echo off
setlocal
set "IDF_ROOT=C:\Users\HP\esp\esp-idf"
set "PROJECT_DIR=C:\Users\HP\Desktop\ESP32 Wroom\xiaozhi_like"
set "PYTHONUTF8=1"
set "PYTHONIOENCODING=utf-8"

echo [build_flash] Started: %DATE% %TIME%
cd /d "%IDF_ROOT%" || (echo IDF_ROOT not found & exit /b 1)
call "%IDF_ROOT%\export.bat" >nul 2>&1
if errorlevel 1 (echo export.bat FAILED & exit /b 1)

cd /d "%PROJECT_DIR%" || (echo PROJECT_DIR not found & exit /b 1)
echo [build] cmake + idf.py build...
"C:\Users\HP\.espressif\python_env\idf5.4_py3.11_env\Scripts\python.exe" "C:\Users\HP\esp\esp-idf\tools\idf.py" -B build build 2>&1
set BUILD_RC=%ERRORLEVEL%
echo [build] RC=%BUILD_RC%
if %BUILD_RC% neq 0 (echo BUILD FAILED & exit /b %BUILD_RC%)

echo [flash] idf.py -p COM6 -b 921600 flash
"C:\Users\HP\.espressif\python_env\idf5.4_py3.11_env\Scripts\python.exe" "C:\Users\HP\esp\esp-idf\tools\idf.py" -p COM6 -b 921600 flash 2>&1
set FLASH_RC=%ERRORLEVEL%
echo [flash] RC=%FLASH_RC%
echo Finished: %DATE% %TIME%
endlocal
exit /b %FLASH_RC%
