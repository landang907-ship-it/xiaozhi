# STATE.md — Mini-Xiaozhi (xiaozhi_like)

## Board
- **ESP32-S3-WROOM-1** (N16R8: 4MB flash, 8MB Octal PSRAM)
- **OLED**: SSD1306 128x64 I2C, addr 0x3C, GPIO8(SDA)/GPIO9(SCL)
- **Speaker**: MAX98357A I2S, GPIO15(BCLK)/GPIO16(WS)/GPIO7(DOUT)
- **Mic**: INMP441 I2S, GPIO5(SCK)/GPIO4(WS)/GPIO6(DIN)

## Phase Status

### P1 + P1.5 ✅
- Clone repo → `xiaozhi_like/`
- esp-idf P2.4 skeleton build OK
- App builds and flashes

### P2.1–P2.4 ✅
- Port board HAL: `Esp32S3Wroom1Board` (I2C0 + I2S0 + I2S1)
- Port audio codec: `Esp32S3AudioCodec`
- `app_main` skeleton with audio loopback
- Build PASS

### P3 Audio Loopback ✅
- Flash COM6, serial log OK
- I2S0 spk + I2S1 mic hoạt động
- Loopback task trên Core 1, chunk 240 samples @ 24kHz (10ms)
- RMS ~14000, peak=32768 (full-scale 16-bit) → mic nhận tốt

### P4 Bug Fixes ✅
- **Fix 1**: config.h SDA 1→8, SCL 2→9 (GPIO đúng cho WROOM-1)
- **Fix 2**: audio codec conversion 24→16 bit: clamp → right-shift 16
- **Fix 3**: Dọn double-enable I2S trong codec
- **Fix 4**: OLED driver — thay esp_lcd API (không tồn tại) bằng driver/i2c_master trực tiếp
  - Ssd1306 class: nhận bus handle, add device, send init sequence
  - Font bitmap 5x7, DrawText/DrawRect/Display methods
  - SSD1306 init done, OLED test pattern flushed ✅
- **Fix 5**: `clock_speed_hz` → `scl_speed_hz` (i2c_device_config_t field name)

### Current Status
- ✅ Build OK (xiaozhi_like.bin 243KB)
- ✅ Flash COM6 OK
- ✅ OLED hiện text pattern
- ✅ Audio loopback chạy (mic→spk)
- ✅ Serial log sạch, không lỗi

## Next Steps
- P4.1: OTA update (esp_ota_ops)
- P5: WiFi connect + HTTPS GET (stream từ server)
- P5.1: HTTP server nhận audio stream
- P6: Wake word detection (porcupine/speech央企)
- P6.1: LLM integration (OpenAI-compatible API)
- P6.2: Full voice assistant loop
