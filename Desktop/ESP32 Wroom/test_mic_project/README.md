# test_mic — INMP441 Microphone Test

Project ESP-IDF **độc lập** để test micro INMP441 trên ESP32-S3-WROOM-1.
Không dùng chung build/, sdkconfig, hay flash với `test_oled_project` hay `test_speaker_project`.

## Pinout (theo `PINOUT.md`)

| Signal | GPIO | I2S bus |
|--------|------|---------|
| WS (LRCLK) | GPIO4 | I2S_NUM_1 |
| SCK (BCLK) | GPIO5 | I2S_NUM_1 |
| SD (DIN) | GPIO6 | I2S_NUM_1 |
| VCC | 3.3V | |
| GND | GND | |
| L/R | GND | Left channel |

Định dạng audio: 24-bit mono (Left channel), 24000 Hz mặc định.

## Build & flash

```bat
REM Build only
rebuild.bat

REM Build + flash + monitor (mặc định COM6, hoặc truyền cổng khác)
build_and_flash.bat COM6
```

## Console menu (Serial Monitor 115200, UART0)

Khi khởi động, board sẽ vào REPL `mic>`. Gõ `help` để xem danh sách lệnh.

| Lệnh | Mô tả |
|------|-------|
| `init` | Khởi tạo lại I2S RX |
| `rate <hz>` | Đổi sample rate (16000, 22050, 24000, 32000, 44100, 48000) |
| `read [frames]` | Đọc N frames, in peak/RMS |
| `hex [frames]` | Dump raw hex N samples |
| `fft [frames]` | Ước tính tần số (zero-crossing) |
| `thresh <val>` | Đặt ngưỡng peak detect |
| `watch [sec]` | Theo dõi, log khi peak > threshold |
| `all` | Chạy toàn bộ test tuần tự |
| `help` | In danh sách lệnh |

## Cách test

1. **Kiểm tra wiring**: Chạy `read 1024` — nếu peak > 0, mic hoạt động.
2. **Test tần số**: Gõ `fft 2048` rồi nói/khúc — xem ước tính Hz.
3. **Test threshold**: Đặt `thresh 1000`, rồi `watch 10` — vỗ tay, nói to sẽ thấy log "Threshold exceeded".
4. **Test sample rate**: `rate 44100` rồi `read 2048` — so sánh peak/RMS.
5. **Debug wiring**: `hex 128` — xem giá trị thô, nếu toàn 0 hoặc 0x800000 → wiring sai.

## Xử lý lỗi

| Triệu chứng | Nguyên nhân / Cách sửa |
|-------------|------------------------|
| peak = 0, RMS = 0 | Kiểm tra dây WS/SCK/SD, nguồn 3V3, L/R pin |
| peak luôn max (0x7FFFFF) | Có thể SCK/WS bị đảo — kiểm tra lại pin |
| Build lỗi `i2s_std.h` | Chạy `idf.py fullclean` rồi build lại |
| Console không nhận lệnh | Đảm bảo Serial Monitor ở 115200 baud |
