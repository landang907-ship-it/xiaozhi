# test_mic_oled — INMP441 Mic + SSD1306 OLED Combined Test

Project ESP-IDF **kết hợp** micro INMP441 và OLED SSD1306.
Stream trực tiếp dữ liệu âm thanh từ mic lên màn hình OLED.

## Pinout

| Peripheral | Signal | GPIO | Bus |
|-----------|--------|------|-----|
| OLED SSD1306 | SDA | GPIO8 | I2C_NUM_0 |
| OLED SSD1306 | SCL | GPIO9 | I2C_NUM_0 |
| INMP441 Mic | WS (LRCLK) | GPIO4 | I2S_NUM_1 |
| INMP441 Mic | SCK (BCLK) | GPIO5 | I2S_NUM_1 |
| INMP441 Mic | SD (DIN) | GPIO6 | I2S_NUM_1 |

## Display Modes

| Mode | Mô tả |
|------|-------|
| 0 | **Waveform** — Oscilloscope, hiển thị dạng sóng âm thanh |
| 1 | **Level Meter** — Thanh bar peak/RMS + giá trị số |
| 2 | **Combined** — Waveform trên + Level meter dưới + tần số |

## Build & Flash

```bat
rebuild.bat
build_and_flash.bat COM6
```

## Console Commands

| Lệnh | Mô tả |
|------|-------|
| `stream [mode]` | Bắt đầu stream mic lên OLED (0/1/2) |
| `stop` | Dừng stream |
| `mode [0-2]` | Đổi/xem chế độ hiển thị |
| `read [frames]` | Đọc mic, in peak/RMS |
| `fft` | Ước tính tần số |
| `thresh <val>` | Đặt ngưỡng peak |
| `rate <hz>` | Đổi sample rate |
| `hello` | Hiển thị HELLO trên OLED |
| `cls` | Xóa OLED |
| `init` | Re-init mic |

## Cách test

1. Flash xong → OLED hiển thị "READY!"
2. Gõ `stream 0` → OLED hiện waveform (nói/khúc sẽ thấy sóng)
3. Gõ `stream 1` → OLED hiện level meter (bar nhảy theo âm lượng)
4. Gõ `stream 2` → OLED hiện cả hai + tần số
5. Gõ `stop` → dừng stream
6. Gõ `hello` → OLED hiện chữ HELLO
