@echo off
REM ============================================================
REM P2.4 build verify - xiaozhi_like
REM Source export.bat then run idf.py build
REM ============================================================
setlocal EnableDelayedExpansion
set "LOG_DIR=C:\Users\HP\Desktop\ESP32 Wroom\xiaozhi_like"
set "LOG=%LOG_DIR%\p2_4_build.log"
set "ERR_LOG=%LOG_DIR%\p2_4_build.err.log"
set "IDF_ROOT=C:\Users\HP\esp\esp-idf"
set "PROJECT_DIR=%LOG_DIR%"

REM Force UTF-8 stdout for kconfgen (avoids charmap crash on non-ASCII in sdkconfig.defaults)
set "PYTHONUTF8=1"
set "PYTHONIOENCODING=utf-8"

echo ============================================================ > "%LOG%"
echo P2.4 BUILD VERIFY - xiaozhi_like >> "%LOG%"
echo Started: %DATE% %TIME% >> "%LOG%"
echo IDF_ROOT: %IDF_ROOT% >> "%LOG%"
echo PROJECT_DIR: %PROJECT_DIR% >> "%LOG%"
echo PYTHONUTF8=%PYTHONUTF8% >> "%LOG%"
echo ============================================================ >> "%LOG%"

cd /d "%IDF_ROOT%" || (echo FAILED to cd to IDF_ROOT & exit /b 1)

echo [step] calling export.bat >> "%LOG%"
call "%IDF_ROOT%\export.bat" >> "%LOG%" 2>&1
if errorlevel 1 (
  echo FAILED: export.bat returned error >> "%LOG%"
  exit /b 1
)
echo export.bat OK >> "%LOG%"

echo [step] which python (after export) >> "%LOG%"
where python >> "%LOG%" 2>&1
echo [step] which idf.py (after export) >> "%LOG%"
where idf.py >> "%LOG%" 2>&1

echo [step] idf.py --version >> "%LOG%"
idf.py --version >> "%LOG%" 2>> "%ERR_LOG%"
set RC0=%ERRORLEVEL%
echo idf --version RC=%RC0% >> "%LOG%"

cd /d "%PROJECT_DIR%" || (echo FAILED to cd to PROJECT_DIR & exit /b 1)

echo [step] idf.py set-target esp32s3 >> "%LOG%"
idf.py set-target esp32s3 >> "%LOG%" 2>> "%ERR_LOG%"
set RC1=%ERRORLEVEL%
echo set-target RC=%RC1% >> "%LOG%"

if not "%RC1%"=="0" (
  echo STOP: set-target failed RC=%RC1% >> "%LOG%"
  exit /b %RC1%
)

echo [step] idf.py build >> "%LOG%"
idf.py build >> "%LOG%" 2>> "%ERR_LOG%"
set RC2=%ERRORLEVEL%
echo build RC=%RC2% >> "%LOG%"
echo Finished: %DATE% %TIME% >> "%LOG%"

if "%RC2%"=="0" (
  echo BUILD SUCCESS >> "%LOG%"
) else (
  echo BUILD FAILED >> "%LOG%"
)

endlocal & exit /b %RC2%
