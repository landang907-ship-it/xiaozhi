Set-Location 'C:\Users\HP\Desktop\ESP32 Wroom\test_web_audio_oled_project'

$env:IDF_PATH = 'C:\Users\HP\esp\esp-idf'
$env:IDF_PYTHON_ENV_PATH = 'C:\Users\HP\.espressif\python_env\idf5.4_py3.14_env'
$env:IDF_TOOLS_PATH = 'C:\Users\HP\.espressif'

$toolsPath = 'C:\Users\HP\.espressif\tools'
$env:PATH = "$toolsPath\xtensa-esp-elf\esp-14.2.0_20260121\xtensa-esp-elf\bin;$toolsPath\cmake\3.30.2\bin;$toolsPath\ninja\1.12.1;C:\Users\HP\.espressif\python_env\idf5.4_py3.14_env\Scripts;C:\Users\HP\esp\esp-idf\tools;$env:PATH"

$py = 'C:\Users\HP\.espressif\python_env\idf5.4_py3.14_env\Scripts\python.exe'

Write-Host "=== CWD: $(Get-Location) ==="
Write-Host "=== Flashing to COM6 ==="

& $py "$env:IDF_PATH\tools\idf.py" -p COM6 flash 2>&1 | Tee-Object -FilePath 'C:\Users\HP\Desktop\ESP32 Wroom\test_web_audio_oled_project\flash_run_new.log'

Write-Host "=== Exit code: $LASTEXITCODE ==="
