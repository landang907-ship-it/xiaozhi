# VERSION 1.1 — Mini-Xiaozhi (xiaozhi_like)
## Release Date: 2026-06-28

> **Changelog**: v1.0 → v1.1: +P4.1 OTA Update + WiFi modules

---

## 📋 Tổng quan

| Thông tin | Chi tiết |
|-----------|----------|
| **Project** | Mini-Xiaozhi (xiaozhi_like) |
| **Version** | 1.1 |
| **Board** | ESP32-S3-WROOM-1 (N16R8: 4MB Flash, 2MB Octal PSRAM) |
| **Framework** | ESP-IDF v5.x |

---

## ✅ Phases Hoàn thành

| Phase | Mô tả | Status |
|-------|-------|--------|
| P1 + P1.5 | Clone repo, ESP-IDF skeleton | ✅ |
| P2.1-P2.4 | Port HAL, audio codec, app_main | ✅ |
| P3 | Audio Loopback (mic → spk) | ✅ |
| P4 | Bug fixes (OLED, audio, I2S) | ✅ |
| **P4.1** | **OTA Update + WiFi** | **✅ (2026-06-28)** |

---

## 🆕 P4.1 Features

### WiFi Manager (wifi_manager.h/cc)
- WiFi connection với NVS credential storage
- Auto-connect với stored credentials
- Event-based connection handling

### OTA Update (ota_update.h/cc)
- HTTP/HTTPS OTA via `esp_https_ota`
- Progress tracking với callback
- Boot validation với rollback protection

### Partition Table (OTA)
- factory: 1MB (0x100000) - Primary app
- ota_0: 1MB (0x100000) - OTA target
- storage: ~1.8MB FAT

### Console Commands
```
wifi connect <ssid> <password>  - Connect WiFi
wifi status                      - Show WiFi status
wifi clear                       - Clear credentials
ota <url>                        - Start OTA update
ota status                       - Show OTA status
status                           - System status
volume <0-100>                   - Set volume
```

---

## 📁 Files Added (P4.1)

```
main/wifi_manager.h    - WiFi API header
main/wifi_manager.cc   - WiFi implementation
main/ota_update.h      - OTA API header
main/ota_update.cc     - OTA implementation
do_p4_1_build.bat      - Build script
```

---

## 🔜 Next Phases

| Phase | Mô tả |
|-------|-------|
| P5 | WiFi connect + HTTPS GET (stream) |
| P5.1 | HTTP server nhận audio stream |
| P6 | Wake word detection |

---

## 🏷️ Tags

`v1.1` `esp32s3` `ota-update` `wifi` `xiaozhi`

---

*Generated: 2026-06-28*