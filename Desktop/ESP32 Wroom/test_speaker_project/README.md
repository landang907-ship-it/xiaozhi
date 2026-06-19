# test_speaker — MAX98357A Speaker Test

Project ESP-IDF **độc lập** để test loa MAX98357A trên ESP32-S3-WROOM-1.
Không dùng chung build/, sdkconfig, hay flash với `test_oled_project`.

## Pinout (theo `PINOUT.md`)

| Signal | GPIO | I2S bus |
|--------|------|---------|
| BCLK   | GPIO15 | I2S_NUM_0 |
| WS (LRC) | GPIO16 | I2S_NUM_0 |
| DIN (DOUT) | GPIO7 | I2S_NUM_0 |
| VIN | 3.3V | |
| GND | GND | |

Định dạng audio: 16-bit stereo, 44100 Hz.

## Build & flash

```bat
REM Build only
rebuild.bat

REM Build + flash + monitor (mặc định COM6, hoặc truyền cổng khác)
build_and_flash.bat COM6
```

## Console menu (Serial Monitor 115200, UART0)

Khi khởi động, board sẽ phát 2 tiếng bíp xác nhận loa hoạt động, rồi vào REPL `spk>`.

| Lệnh | Mô tả |
|------|-------|
| `tone <freq> <ms>` | Phát sine tại `freq` Hz trong `ms` ms |
| `sweep <f0> <f1> <ms>` | Quét tần số từ `f0` → `f1` |
| `wave <sine\|square\|triangle\|saw> <ms>` | Phát dạng sóng tại 440 Hz |
| `chan <l\|r\|both> <ms>` | Test kênh trái / phải / cả hai |
| `vol <ms>` | Quét âm lượng 0 → max |
| `beep <n>` | `n` tiếng bíp ngắn |
| `noise <ms>` | White noise |
| `stress <sec>` | Phát liên tục `sec` giây để test ổn định |
| `all` | Chạy tuần tự toàn bộ test |
| `help` | In danh sách lệnh |
