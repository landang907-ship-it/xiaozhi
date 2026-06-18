@echo off
REM ============================================================================
REM  build_and_flash.bat - Auto build + flash firmware cho ESP32-S3
REM  Usage:
REM    build_and_flash.bat           (default: auto-detect COM port)
REM    build_and_flash.bat COM3      (use specific COM port)
REM    build_and_flash.bat COM3 2    (use COM3 + 2 MB flash image)
REM ============================================================================
setlocal enabledelayedexpansion

set "PORT=%~1"
set "FLASH_SIZE=%~2"

if "%PORT%"=="" (
    echo [INFO] Auto-detect COM port...
    set "PORT=AUTO"
)
if "%FLASH_SIZE%"=="" set "FLASH_SIZE=4"

set "IDF_PATH=C:\Users\HP\esp\esp-idf"
if not exist "%IDF_PATH%\export.bat" (
    echo [ERROR] ESP-IDF not found at %IDF_PATH%
    exit /b 1
)

echo [STEP 1] Loading ESP-IDF environment...
set "PYTHONIOENCODING=utf-8"
set "PYTHONUTF8=1"
set "PYTHON=C:\Users\HP\.espressif\python_env\idf5.4_py3.14_env\Scripts\python.exe"
set "PATH=C:\Users\HP\.espressif\python_env\idf5.4_py3.14_env\Scripts;%PATH%"
call "%IDF_PATH%\export.bat" >nul
if errorlevel 1 (
    echo [ERROR] Failed to load ESP-IDF environment
    exit /b 1
)

cd /d "%~dp0"

if "%PORT%"=="AUTO" (
    echo [STEP 2] Detecting COM port...
    python -m esptool detect-port --port auto
    if errorlevel 1 (
        echo [WARN] Could not auto-detect. Listing available ports:
        python -m esptool list-port
    )
)

echo [STEP 3] Setting target esp32s3...
call idf.py set-target esp32s3
if errorlevel 1 goto :error

echo [STEP 4] Building firmware (parallel = 8 cores)...
call idf.py build -- -j8
if errorlevel 1 goto :error

if not "%PORT%"=="AUTO" (
    echo [STEP 5] Flashing to %PORT%...
    call idf.py -p %PORT% -b 921600 flash
    if errorlevel 1 goto :error

    echo [STEP 6] Opening serial monitor @ 115200...
    call idf.py -p %PORT% -b 115200 monitor
)

echo.
echo [DONE] Build successful. Binary: build\test_oled.bin
exit /b 0

:error
echo.
echo [FAILED] Build/flash stopped with error.
exit /b 1
