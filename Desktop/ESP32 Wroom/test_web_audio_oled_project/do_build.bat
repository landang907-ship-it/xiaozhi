@echo off
setlocal
cd /d "c:\Users\HP\Desktop\ESP32 Wroom\test_web_audio_oled_project"

set PY=C:\Users\HP\.espressif\python_env\idf5.4_py3.14_env\Scripts\python.exe
set IDF_PATH=C:\Users\HP\esp\esp-idf
set IDF_PYTHON_ENV_PATH=C:\Users\HP\.espressif\python_env\idf5.4_py3.14_env
set IDF_TOOLS_PATH=C:\Users\HP\.espressif

set ESP_TOOLS=C:\Users\HP\.espressif\tools
set XTENSA=%ESP_TOOLS%\xtensa-esp-elf\esp-14.2.0_20260121\xtensa-esp-elf\bin
set ESP_ROM_ELF_DIR=%ESP_TOOLS%\esp-rom-elfs\20241011

set PATH=%XTENSA%;%ESP_TOOLS%\cmake\3.30.2\bin;%ESP_TOOLS%\ninja\1.12.1;%ESP_TOOLS%\idf5.4_py3.14_env\Scripts;%IDF_PATH%\tools;%PATH%

echo === CWD: %CD% ===
echo === ESP_ROM_ELF_DIR=%ESP_ROM_ELF_DIR% ===
where xtensa-esp32s3-elf-gcc

if exist build rmdir /s /q build
echo === Cleaned build dir ===

%PY% "%IDF_PATH%\tools\idf.py" build
if errorlevel 1 goto :err
echo === BUILD OK ===
exit /b 0
:err
echo === BUILD FAILED ===
exit /b 1
