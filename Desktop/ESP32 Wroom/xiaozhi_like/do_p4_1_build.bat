@echo off
REM P4.1 Build script for xiaozhi_like
REM Usage: run from xiaozhi_like directory

set IDF_PATH=C:\Users\HP\Desktop\c3\esp\esp-idf
set ESPPORT=COM6
set ESPBAUD=921600

echo [BUILD] Setting up ESP-IDF...
call "%IDF_PATH%\export.bat" 2>nul

echo [BUILD] Cleaning old build...
if exist build rmdir /s /q build
if exist sdkconfig del sdkconfig
if exist dependencies.lock del dependencies.lock

echo [BUILD] Configuring for ESP32-S3...
idf.py set-target esp32s3

echo [BUILD] Building...
idf.py build

if %ERRORLEVEL% EQU 0 (
    echo [BUILD] SUCCESS!
    echo [BUILD] Bin: build\xiaozhi_like.bin
) else (
    echo [BUILD] FAILED!
)

echo [BUILD] Done.