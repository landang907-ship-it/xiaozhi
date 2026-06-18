# Test OLED SSD1306 — ESP32-S3 Firmware

Standalone firmware test màn hình **OLED SSD1306 128×64 I2C** trên board **ESP32-S3-WROOM-1 (N16R8)**.

> 📁 Source code: `main/test_oled.cc` (driver SSD1306 inline + 12 test cases + menu serial console).

---

## 1. Phần cứng

| Pin ESP32-S3 | Kết nối  | Ghi chú                              |
|--------------|----------|--------------------------------------|
| GPIO8        | OLED SDA | I²C SDA (có internal pull-up enabled)|
| GPIO8        | OLED SCL | I²C SCL (có internal pull-up enabled)|
| 3V3          | OLED VCC | Cấp 3.3V                             |
| GND          | OLED GND | Mass chung                            |

Địa chỉ I²C: `0x3C`  •  Tốc độ: **100 kHz**

---

## 2. Cấu trúc project

```
test_oled_project/
├── CMakeLists.txt              ← root ESP-IDF project
├── sdkconfig.defaults          ← cấu hình mặc định (esp32s3, PSRAM, USB-CDC)
├── .gitignore
├── README.md                   ← file này
├── build_and_flash.bat         ← script auto build/flash cho Windows
└── main/
    ├── CMakeLists.txt          ← khai báo component
    └── test_oled.cc            ← source chính (driver + tests + menu)
```

---

## 3. Cách build & flash

### Cách 1 — Dùng script `build_and_flash.bat` (khuyến nghị)

```bat
cd "c:\Users\HP\Desktop\ESP32 Wroom\test_oled_project"
build_and_flash.bat COMx        :: thay COMx bằng cổng thực tế
```

Script sẽ tự động:

1. Load môi trường ESP-IDF (`C:\Users\HP\esp\esp-idf\export.bat`)
2. `idf.py set-target esp32s3`
3. `idf.py -j8 build`
4. `idf.py -p COMx flash monitor`

### Cách 2 — Chạy từng lệnh

```bat
:: 1. Mở Command Prompt (cmd.exe)
cd /d "c:\Users\HP\Desktop\ESP32 Wroom\test_oled_project"

:: 2. Load môi trường ESP-IDF
call C:\Users\HP\esp\esp-idf\export.bat

:: 3. Đặt target (chỉ cần 1 lần)
idf.py set-target esp32s3

:: 4. Build
idf.py -j8 build

:: 5. Flash và mở serial monitor
idf.py -p COM3 -b 921600 flash
idf.py -p COM3 -b 115200 monitor
```

> 🔍 **Tìm cổng COM:** Mở `Device Manager` → `Ports (COM & LPT)` → tìm `USB-SERIAL CH340` / `CP210x` / `ESP32-S3 USB`.

---

## 4. Lần build đầu tiên

Lần build đầu sẽ **tự động download toolchain** (~300 MB) vào `C:\Users\HP\.espressif\tools\`. Cần:

- Internet ổn định
- ~5–10 phút cho lần đầu
- Khoảng 2 GB dung lượng đĩa trống

Lần build sau sẽ nhanh hơn nhiều (chỉ ~30–60s nếu không sửa code).

---

## 5. Menu serial console

Sau khi flash, mở **Serial Monitor @ 115200 baud** (PuTTY / `idf.py monitor` / VS Code Serial Monitor). Bạn sẽ thấy menu:

```
==============================================
 OLED SSD1306 Test Menu (128x64 @ 0x3C)
 SDA=8  SCL=9  MirrorX=1 MirrorY=1
==============================================
  0. I2C bus scan
  1. Clear & fill (alternating)
  2. Text rendering
  3. Pixel / Line / Rect / Circle
  4. Mirror X/Y verification
  5. Contrast sweep
  6. Invert blink
  7. Horizontal scroll
  8. Bouncing ball animation
  9. Progress bar
 10. Bitmap logo
 11. Stress test (perf)
  a. Run ALL tests sequentially
  r. Restart (re-init OLED)
  q. Quit (just idle)
----------------------------------------------
>
```

Gõ **0..9, 10, 11** rồi **Enter** để chạy test. Gõ **`a`** để chạy tất cả test tuần tự.

---

## 6. Cấu hình phần cứng (trong `main/test_oled.cc`)

Nếu muốn đổi pin hoặc địa chỉ I²C, sửa các `#define` ở đầu file:

```cpp
#define DISPLAY_SDA_PIN          GPIO_NUM_8
#define DISPLAY_SCL_PIN          GPIO_NUM_9
#define DISPLAY_I2C_NUM          I2C_NUM_0
#define DISPLAY_I2C_ADDR         0x3C
#define DISPLAY_I2C_SPEED_HZ     100000   // 100 kHz
#define DISPLAY_MIRROR_X         true     // xoay 180° trục X
#define DISPLAY_MIRROR_Y         true     // xoay 180° trục Y
```

Rồi build + flash lại.

---

## 7. Xử lý lỗi thường gặp

| Triệu chứng | Nguyên nhân / Cách sửa |
|-------------|------------------------|
| `A fatal error occurred: Failed to connect to ESP32-S3: No serial port found` | Sai cổng COM hoặc chưa cắm cáp USB |
| `Wrong boot mode detected (0x13)!` | Cần nhấn nút **BOOT** trên board rồi giữ khi flash |
| OLED không hiển thị | Kiểm tra dây SDA/SCL, nguồn 3V3; chạy test `0` (I²C scan) để tìm địa chỉ thực |
| Hiển thị ngược / lệch | Đổi `DISPLAY_MIRROR_X` / `DISPLAY_MIRROR_Y` rồi flash lại |
| Console không nhận phím | Đảm bảo Serial Monitor ở 115200 baud, line ending = `LF` |
| Build lỗi `esp_timer.h: No such file` | Xóa `build/` rồi build lại: `idf.py fullclean && idf.py build` |

---

## 8. Tham khảo

- [ESP-IDF v5.4 Documentation](https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32s3/index.html)
- [I2C Master Driver (new API)](https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32s3/api-reference/peripherals/i2c_master.html)
- [SSD1306 Datasheet (Adafruit)](https://cdn-shop.adafruit.com/datasheets/SSD1306.pdf)
