# VERSION 1.0 — Mini-Xiaozhi (xiaozhi_like)
## Release Date: 2026-06-28

---

## 📋 Tổng quan

| Thông tin | Chi tiết |
|-----------|----------|
| **Project** | Mini-Xiaozhi (xiaozhi_like) |
| **Version** | 1.0 |
| **Board** | ESP32-S3-WROOM-1 (N16R8: 4MB Flash, 2MB Octal PSRAM) |
| **Framework** | ESP-IDF v5.x |
| **Source** | github.com/78/xiaozhi-esp32 |

---

## 🔧 Hardware Configuration

### Pinout đã verify

| Ngoại vi | Tín hiệu | GPIO | Status |
|----------|-----------|------|--------|
| Speaker (I2S0) | BCLK | GPIO15 | ✅ |
| Speaker (I2S0) | WS | GPIO16 | ✅ |
| Speaker (I2S0) | DOUT | GPIO7 | ✅ |
| Mic (I2S1) | SCK/BCLK | GPIO5 | ✅ |
| Mic (I2S1) | WS | GPIO4 | ✅ |
| Mic (I2S1) | DIN | GPIO6 | ✅ |
| OLED (I2C0) | SDA | GPIO8 | ✅ |
| OLED (I2C0) | SCL | GPIO9 | ✅ |
| Wake Button | TTP223 OUT | GPIO47 | ✅ |
| Reset SSID | BOOT | GPIO0 | ✅ |

### Components đã kết nối

| Component | Model | Interface | Address/Config |
|-----------|-------|-----------|----------------|
| Speaker | MAX98357A | I2S0 | 24kHz, 16-bit |
| Microphone | INMP441 | I2S1 | 24kHz, 24-bit → 16-bit |
| OLED Display | SSD1306 | I2C0 | 128×64, 0x3C |
| Wake Button | TTP223 | GPIO47 | Active HIGH |
| Reset Button | BOOT | GPIO0 | Active LOW, 5s hold |

---

## ✅ Phases Hoàn thành

### Phase 1 + 1.5 ✅
- [x] Clone xiaozhi-esp32 repo
- [x] ESP-IDF P2.4 skeleton build OK
- [x] Tạo board config cho ESP32-S3-WROOM-1

### Phase 2.1 - 2.4 ✅
- [x] Port board HAL: `Esp32S3Wroom1Board` (I2C0 + I2S0 + I2S1)
- [x] Port audio codec: `Esp32S3AudioCodec`
- [x] `app_main` skeleton với audio loopback
- [x] Build PASS

### Phase 3 ✅ (2026-06-23)
- [x] Audio Loopback (mic → spk)
- [x] Flash COM6, serial log OK
- [x] I2S0 spk + I2S1 mic hoạt động
- [x] Loopback task trên Core 1, chunk 240 samples @ 24kHz (10ms)
- [x] RMS ~14000, peak=32768 (full-scale 16-bit)
- [x] Build verified: xiaozhi_like.bin 251648 bytes (84% free)

### Phase 4 ✅ (2026-06-24)
- [x] **Fix 1**: config.h SDA 1→8, SCL 2→9 (GPIO đúng cho WROOM-1)
- [x] **Fix 2**: audio codec conversion 24→16 bit: clamp → right-shift 16
- [x] **Fix 3**: Dọn double-enable I2S trong codec
- [x] **Fix 4**: OLED driver — port sang esp_lcd/i2c_master trực tiếp
- [x] **Fix 5**: `clock_speed_hz` → `scl_speed_hz` (i2c_device_config_t field name)

---

## 📁 Project Structure

```
xiaozhi_like/
├── CMakeLists.txt              # Root CMake
├── sdkconfig.defaults          # ESP-IDF config (4MB flash, Octal PSRAM)
├── partitions/
│   └── v2/4m.csv              # Custom partition cho 4MB flash
├── main/
│   ├── CMakeLists.txt
│   ├── main.cc                # app_main + audio loopback
│   ├── boards/
│   │   └── esp32s3-wroom-1/
│   │       ├── config.h       # Pin definitions
│   │       ├── config.json    # SDKconfig flags
│   │       └── esp32s3_wroom1_board.cc  # Board HAL
│   └── audio/
│       └── esp32s3_audio_codec.cc  # Audio codec
├── xiaozhi_src/               # Original xiaozhi-esp32 source
│   ├── main/
│   │   ├── audio/
│   │   ├── display/
│   │   ├── protocols/
│   │   └── boards/
│   └── components/
└── tmp/
    └── xiaozhi_src_repo/      # Git clone backup
```

---

## 🔧 Build & Flash Commands

```bash
# Build
cd xiaozhi_like
idf.py set-target esp32s3
idf.py -j8 build

# Flash
idf.py -p COM6 -b 921600 flash

# Monitor
idf.py -p COM6 monitor
```

---

## 📊 Build Statistics

| Metric | Value |
|--------|-------|
| App bin size | 251,648 bytes (0x3D700) |
| App partition usage | 84% free |
| Bootloader size | 21,200 bytes (0x52D0) |
| Bootloader usage | 35% free |
| Build time | ~4.5 phút |
| Ninja steps | 1061/1061 |

---

## 🔜 Next Phases (Pending)

| Phase | Mô tả | Priority |
|-------|-------|----------|
| P4.1 | OTA update (esp_ota_ops) | High |
| P5 | WiFi connect + HTTPS GET | High |
| P5.1 | HTTP server nhận audio stream | Medium |
| P6 | Wake word detection (porcupine) | Medium |
| P6.1 | LLM integration (OpenAI API) | Medium |
| P6.2 | Full voice assistant loop | Low |
| P7 | Production release | Low |

---

## 📝 Known Issues

- None (đã fix hết trong Phase 4)

## 📝 Notes

- WiFi đã enabled trong sdkconfig.defaults
- Wake word disabled, dùng GPIO47 button thay thế
- OLED dùng GPIO8/9 (không xung đột USB D-/D+)
- Audio: 24kHz, mono, 16-bit output

---

## 🏷️ Tags

`v1.0` `esp32s3` `xiaozhi` `audio-loopback` `oled` `inmp441` `max98357a` `ssd1306`

---

*Document generated: 2026-06-28*