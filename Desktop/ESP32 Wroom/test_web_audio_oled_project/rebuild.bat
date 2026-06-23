@echo off
setlocal
cd /d "%~dp0"
echo === test_web_audio_oled_project / rebuild ===
call C:\Users\HP\esp\esp-idf\export.bat || goto :err
idf.py set-target esp32s3 || goto :err
idf.py build || goto :err
echo === BUILD OK ===
exit /b 0
:err
echo === BUILD FAILED ===
exit /b 1
