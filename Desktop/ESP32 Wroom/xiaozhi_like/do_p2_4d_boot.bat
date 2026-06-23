@echo off
setlocal
set PYTHONIOENCODING=utf-8
set PYTHONUTF8=1
set NINJA_EXE=C:\Users\HP\.espressif\tools\ninja\1.12.1\ninja.exe
set PATH=C:\Users\HP\.espressif\tools\ninja\1.12.1;C:\Users\HP\.espressif\tools\xtensa-esp-elf\esp-14.2.0_20260121\xtensa-esp-elf\bin;%PATH%
cd /d "C:\Users\HP\Desktop\ESP32 Wroom\xiaozhi_like"
echo === ninja -C build/bootloader [START %DATE% %TIME%] ===
"%NINJA_EXE%" -C build/bootloader > p2_4d_boot.log 2>&1
set RC=%ERRORLEVEL%
echo === DONE RC=%RC% [%DATE% %TIME%] ===
exit /b %RC%
