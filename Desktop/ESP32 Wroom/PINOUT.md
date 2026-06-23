# ESP32-S3-WROOM-1 Board Pinout & Wiring Reference

> **Board:** ESP32-S3-WROOM-1 (N16R8, 4MB Flash, 2MB Octal PSRAM)
> **Date verified:** 2026-06-17
> **Firmware:** xiaozhi-esp32 (ESP-IDF)
> **Status:** ✅ OLED (GPIO8/9), MIC, SPK, BUTTON — config cập nhật ngày 2026-06-17

---

## 📋 Bảng pin đã verify

### OLED Display — SSD1306 128×64 I2C
| Signal | GPIO | Note |
|--------|------|------|
| SDA    | **GPIO8** | I2C0 SDA |
| SCL    | **GPIO9** | I2C0 SCL |
| VCC    | 3.3V | |
| GND    | GND  | |
| Addr   | 0x3C | (mặc định SSD1306) |

**I2C clock:** 100 kHz (default, đủ cho SSD1306)
**Pull-up:** Bật internal pull-up (`flags.enable_internal_pullup = true` trong `i2c_master_bus_config_t`).
Khuyến nghị thêm **pull-up ngoài 4.7kΩ** lên 3.3V cho SDA/SCL nếu dây dài.

> **Note:** GPIO8/9 là GPIO thường trên ESP32-S3, **không phải** chân USB D-/D+
> (USB D-/D+ của S3 là GPIO19/20) và **không phải strapping pin**. Dùng GPIO8/9
> cho I2C vẫn giữ được USB-CDC cho Serial Monitor.

### Microphone — INMP441 I2S MEMS
| Signal | GPIO | Note |
|--------|------|------|
| WS (LRCLK) | **GPIO4** | Word Select |
| SCK (BCLK) | **GPIO5** | Bit Clock |
| SD (DIN)   | **GPIO6** | Data In   |
| VCC | 3.3V | |
| GND | GND  | |
| L/R | GND  | Left channel (SD trên rising edge của WS) |
| **I2S bus** | **I2S_NUM_1** | Tách riêng khỏi Speaker |

### Speaker — MAX98357A I2S Amplifier
| Signal | GPIO | Note |
|--------|------|------|
| LRC  | **GPIO16** | Word Select |
| BCLK | **GPIO15** | Bit Clock |
| DIN  | **GPIO7**  | Data In   |
| **I2S bus** | **I2S_NUM_0** | Tách riêng khỏi Mic |

### Wake Button — TTP223 Touch Sensor
| Signal | GPIO | Note |
|--------|------|------|
| I/O (OUT) | **GPIO47** | Active HIGH khi chạm |
| VCC | 3.3V | |
| GND | GND  | |

**Mode:** TTP223 mặc định = momentary toggle HIGH.
Trong code: `Button(BOOT_BUTTON_GPIO, /*active_high=*/true)`.
- Click → toggle chat state (wake/sleep)
- Dùng làm push-to-talk khi `CONFIG_WAKE_WORD_DISABLED=y`

### Reset SSID Button — BOOT button on GPIO0
| Signal | GPIO | Note |
|--------|------|------|
| BOOT | **GPIO0** | Active LOW |
| Long press 5s | → | Xóa WiFi credentials, reboot |

### LED
| Signal | GPIO | Note |
|--------|------|------|
| Status LED | **NC** | Board này không gắn LED, dùng `NoLed()` |

---

## 🔧 Code settings đã cấu hình (lưu lại)

### File: `main/boards/esp32s3-wroom-1/config.h`
```c
// Speaker (MAX98357A) on I2S_NUM_0
#define AUDIO_I2S_SPK_GPIO_BCLK GPIO_NUM_15
#define AUDIO_I2S_SPK_GPIO_WS   GPIO_NUM_16
#define AUDIO_I2S_SPK_GPIO_DOUT GPIO_NUM_7

// Microphone (INMP441) on I2S_NUM_1
#define AUDIO_I2S_MIC_GPIO_SCK  GPIO_NUM_5
#define AUDIO_I2S_MIC_GPIO_WS   GPIO_NUM_4
#define AUDIO_I2S_MIC_GPIO_DIN  GPIO_NUM_6

// Audio sample rates
#define AUDIO_INPUT_SAMPLE_RATE  24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

// Wake button (TTP223 active-high on GPIO47)
#define BOOT_BUTTON_GPIO        GPIO_NUM_47
#define RESET_SSID_BUTTON_GPIO  GPIO_NUM_0
#define RESET_SSID_LONG_PRESS_MS 5000

// OLED — SSD1306 128x64 trên GPIO8/9 (xoay 180°)
// GPIO8/9 là GPIO thường trên S3, không xung đột với USB CDC
// (USB D-/D+ là GPIO19/20).
#define DISPLAY_SDA_PIN         GPIO_NUM_8
#define DISPLAY_SCL_PIN         GPIO_NUM_9
#define DISPLAY_WIDTH           128
#define DISPLAY_HEIGHT          64
// Rotate display 180° by mirroring X and Y
#define DISPLAY_MIRROR_X        true
#define DISPLAY_MIRROR_Y        true
#define DISPLAY_I2C_NUM         I2C_NUM_0


// No status LED
#define BUILTIN_LED_GPIO        GPIO_NUM_NC

// Wake word off, dùng button
#define HANDS_FREE_AUTO_LISTEN  1
```

### File: `main/boards/esp32s3-wroom-1/config.json`
```json
{
  "target": "esp32s3",
  "builds": [
    {
      "name": "esp32s3-wroom-1",
      "sdkconfig_append": [
        "CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y",
        "CONFIG_PARTITION_TABLE_CUSTOM_FILENAME=\"partitions/v2/4m.csv\"",
        "CONFIG_WAKE_WORD_DISABLED=y",
        "CONFIG_ESP_CONSOLE_USB_CDC=y",
        "CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=n"
      ]
    }
  ]
}
```

### I2C init snippet (trong `esp32s3_wroom1_board.cc` / `oled_ssd1306.cc`)
```c
i2c_master_bus_config_t bus_config = {};
bus_config.i2c_port = DISPLAY_I2C_NUM;       // I2C_NUM_0
bus_config.sda_io_num = DISPLAY_SDA_PIN;     // GPIO8
bus_config.scl_io_num = DISPLAY_SCL_PIN;     // GPIO9
bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
bus_config.glitch_ignore_cnt = 7;
bus_config.flags.enable_internal_pullup = true;
ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &display_i2c_bus_));
```

### Audio codec (NoAudioCodecSimplex — 2 I2S bus riêng)
```c
static NoAudioCodecSimplex audio_codec(
    AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
    AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_WS, AUDIO_I2S_SPK_GPIO_DOUT,
    AUDIO_I2S_MIC_GPIO_SCK,  AUDIO_I2S_MIC_GPIO_WS,  AUDIO_I2S_MIC_GPIO_DIN);
```

---

## 🧪 Kết quả verify

### OLED SSD1306 — test trên GPIO8/9
- I2C scan sẽ log "device at 0x3C" → init OK.
- Sau khi flash bản build mới, kiểm tra log cổng COM xem có dòng:
  ```
  I (...) OledSsd1306: Initializing I2C bus for OLED SSD1306 (SDA=8, SCL=9)
  I (...) OledSsd1306:   I2C device ACK at 0x3C
  I (...) OledSsd1306: SSD1306 ready at 0x3C @ 100000 Hz
  ```
  Nếu thấy → OLED hoạt động bình thường trên GPIO8/9 ✅

### Main firmware tự kiểm tra mic
Board init chạy 1 self-test 5s cho INMP441 (`MicSelfTestTask`).
Log mong đợi:
```
I (xxx) ESP32S3Wroom1Board: MIC_TEST: starting INMP441 self-test (WS=4 SCK=5 SD=6)
I (xxx) ESP32S3Wroom1Board: MIC_TEST: frames=... peak=... rms=...
I (xxx) ESP32S3Wroom1Board: MIC_TEST: PASS - mic is producing audio (peak=... rms=...).
```

---

## 🔌 Sơ đồ kết nối (tóm tắt)

```
ESP32-S3-WROOM-1            Peripherals
┌──────────────────┐
│              3V3 ├──────┬─── OLED VCC
│              GND ├──────┼─── OLED GND
│              GND ├──────┼─── INMP441 GND, L/R
│              3V3 ├──────┼─── INMP441 VCC
│              GND ├──────┼─── MAX98357A GND
│              3V3 ├──────┼─── MAX98357A VIN
│              GND ├──────┼─── TTP223 GND
│              3V3 ├──────┴─── TTP223 VCC
│                  │
│  GPIO9  (I2C SCL)├────── OLED SCL
│  GPIO8  (I2C SDA)├────── OLED SDA
│                  │
│  GPIO4  (I2S WS) ├────── INMP441 WS
│  GPIO5  (I2S SCK)├────── INMP441 SCK
│  GPIO6  (I2S SD) ├────── INMP441 SD
│                  │
│  GPIO16 (I2S LRC)├───── MAX98357A LRC
│  GPIO15 (I2S BCLK)├──── MAX98357A BCLK
│  GPIO7  (I2S DIN)├───── MAX98357A DIN
│                  │
│  GPIO47          ├────── TTP223 I/O (wake)
│  GPIO0           ├────── BOOT button (reset SSID)
│                  │
└──────────────────┘
```

--- 

## 📚 Tham khảo nhanh

| File | Vai trò |
|------|---------|
| `config.h` | Pin mapping + audio/wake/display settings |
| `config.json` | sdkconfig flags (flash size, partition, USB CDC) |
| `esp32s3_wroom1_board.cc` | Board class — audio codec, buttons, mic selftest |
| `oled_ssd1306.h/.cc` | OLED driver (I2C bus + SSD1306 panel + Display) |
| `README.md` | (file hướng dẫn cũ) |
| `HUONG_DAN_SU_DUNG.md` | (file hướng dẫn tiếng Việt) |
| `PINOUT.md` | ← **file này — pin reference nhanh** |

---

## 🛠 Lệnh build/flash (từ thư mục `C:\Users\HP\Desktop\c3`)

```bat
:: Set target & build
idf.py set-target esp32s3
idf.py -j8 build

:: Flash
idf.py -p COM6 -b 921600 flash

:: Monitor
idf.py -p COM6 monitor
```

Hoặc dùng script:
```bat
C:\esp\build_s3.bat          REM build
C:\esp\flash_s3_8mb_dio.bat  REM flash 8MB DIO mode
C:\esp\monitor_s3.bat        REM monitor
```
