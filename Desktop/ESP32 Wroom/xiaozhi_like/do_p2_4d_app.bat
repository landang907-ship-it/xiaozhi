@echo off
setlocal
set PYTHONIOENCODING=utf-8
set PYTHONUTF8=1
set NINJA_EXE=C:\Users\HP\.espressif\tools\ninja\1.12.1\ninja.exe
set PATH=C:\Users\HP\.espressif\tools\ccache\4.12.1\ccache-4.12.1-windows-x86_64;C:\Users\HP\.espressif\tools\ninja\1.12.1;C:\Users\HP\.espressif\tools\xtensa-esp-elf\esp-14.2.0_20260121\xtensa-esp-elf\bin;C:\Users\HP\.espressif\tools\cmake\3.30.2;C:\Users\HP\.espressif\tools\openocd-esp32\v0.13.0\openocd-esp32\bin;C:\Users\HP\.espressif\tools\dfu-util\0.11\dfu-util-0.11-windows;%PATH%
cd /d "C:\Users\HP\Desktop\ESP32 Wroom\xiaozhi_like"
echo === ninja -C build [START %DATE% %TIME%] ===
"%NINJA_EXE%" -C build > p2_4d_app.log 2>&1
set RC=%ERRORLEVEL%
echo === DONE RC=%RC% [%DATE% %TIME%] ===
exit /b %RC%
