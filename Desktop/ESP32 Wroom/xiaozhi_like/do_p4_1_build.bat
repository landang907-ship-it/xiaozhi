@echo off
REM P4.1 Build script for xiaozhi_like
REM Auto-detects ESP-IDF installation

setlocal enabledelayedexpansion

set IDF_DIR=C:\esp\esp-idf
set ESPPORT=COM6
set ESPBAUD=921600

echo ========================================
echo   Mini-Xiaozhi P4.1 Build Script
echo ========================================
echo.

REM Find ESP-IDF
set IDF_FOUND=0

if exist "%IDF_DIR%\export.bat" (
    echo [OK] Found ESP-IDF at %IDF_DIR%
    set IDF_PATH=%IDF_DIR%
    set IDF_FOUND=1
) else if exist "C:\Espressif\frameworks\esp-idf-v5.2\export.bat" (
    echo [OK] Found ESP-IDF v5.2
    set IDF_PATH=C:\Espressif\frameworks\esp-idf-v5.2
    set IDF_FOUND=1
) else if exist "C:\Espressif\frameworks\esp-idf-v5.1\export.bat" (
    echo [OK] Found ESP-IDF v5.1
    set IDF_PATH=C:\Espressif\frameworks\esp-idf-v5.1
    set IDF_FOUND=1
) else if exist "%USERPROFILE%\esp\esp-idf\export.bat" (
    echo [OK] Found ESP-IDF in user profile
    set IDF_PATH=%USERPROFILE%\esp\esp-idf
    set IDF_FOUND=1
)

if %IDF_FOUND%==0 (
    echo [ERROR] ESP-IDF not found!
    echo.
    echo Please install ESP-IDF from:
    echo   https://dl.espressif.com/dl/esp-idf/
    echo.
    echo Or run this command to install:
    echo   pip install esptool
    echo.
    pause
    exit /b 1
)

echo.
echo [*] Setting up ESP-IDF...
call "%IDF_PATH%\export.bat" >nul 2>&1

REM Change to project directory
cd /d "%~dp0"

echo.
echo [*] Cleaning old build...
if exist build rmdir /s /q build
if exist sdkconfig del /q sdkconfig 2>nul
if exist sdkconfig.lock del /q sdkconfig.lock 2>nul

echo.
echo [*] Configuring for ESP32-S3...
idf.py set-target esp32s3

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Target configuration failed!
    pause
    exit /b 1
)

echo.
echo [*] Building (this may take a few minutes)...
idf.py build

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ========================================
    echo   BUILD SUCCESS!
    echo ========================================
    echo.
    echo Bin: %~dp0build\xiaozhi_like.bin
    echo.
    echo To flash: idf.py -p COM6 -b 921600 flash
    echo To monitor: idf.py -p COM6 monitor
) else (
    echo.
    echo ========================================
    echo   BUILD FAILED!
    echo ========================================
)

echo.
pause