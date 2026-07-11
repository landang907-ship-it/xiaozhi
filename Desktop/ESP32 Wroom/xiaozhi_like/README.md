# xiaozhi_like — Mini Xiaozhi cho ESP32-S3-WROOM-1

> Dự án "mini xiaozhi" chạy trên board **ESP32-S3-WROOM-1 (N16R8)**.
> Voice chat AI (tiếng Việt) dùng protocol từ [78/xiaozhi-esp32](https://github.com/78/xiaozhi-esp32), nhưng với **board driver tự viết** cho ESP32-S3-WROOM-1.

## 📖 Quick links

| File | Vai trò |
|---|---|
| **[STATE.md](STATE.md)** | "Bộ nhớ ngoài" — đọc đầu tiên mỗi session |
| **[../PINOUT.md](../PINOUT.md)** | GPIO mapping đã verify |
| **[../rules.md](../rules.md)** | Quy tắc làm việc |

## 🔧 Hardware

| Peripheral | Pins | Bus |
|---|---|---|
| OLED SSD1306 128×64 | SDA=GPIO8, SCL=GPIO9, addr=0x3C | I2C_NUM_0 |
| Mic INMP441 | SCK=GPIO5, WS=GPIO4, DIN=GPIO6 | I2S_NUM_1 |
| Speaker MAX98357A | BCLK=GPIO15, WS=GPIO16, DOUT=GPIO7 | I2S_NUM_0 |
| Wake button (TTP223) | GPIO47 | active-HIGH |
| Reset SSID (BOOT) | GPIO0 | active-LOW long-press 5s |
| LED | NC | dùng `NoLed()` |

Xem chi tiết: [../PINOUT.md](../PINOUT.md)

## 🏗 Build / Flash

```bat
cd xiaozhi_like

REM Lần đầu: set target
idf.py set-target esp32s3

REM Build
idf.py -j8 build

REM Flash + monitor
idf.py -p COM6 -b 921600 flash monitor
```

Hoặc dùng script (sẽ tạo ở Phase 7):
```bat
rebuild.bat
build_and_flash.bat
```

## 📚 Project structure

```
xiaozhi_like/
├── STATE.md              ← external memory (READ FIRST)
├── main/
│   └── boards/esp32s3-wroom-1/   ← board driver tự viết
├── xiaozhi_src/          ← copy từ xiaozhi-esp32 upstream
└── components/           ← custom (web debug UI, etc)
```

## 📅 Status

- ✅ Phase 1: skeleton
- ⏳ Phase 2-7: pending

Xem chi tiết trong [STATE.md](STATE.md).

## 📜 License

GPL-3.0 (theo upstream [78/xiaozhi-esp32](https://github.com/78/xiaozhi-esp32)).
  
