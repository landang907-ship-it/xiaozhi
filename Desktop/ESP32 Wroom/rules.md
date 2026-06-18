# Project Rules — ESP32-S3-WROOM-1 (xiaozhi-esp32)

> Quy tắc làm việc **bắt buộc** cho mọi thành viên tham gia dự án này.
> Board: **ESP32-S3-WROOM-1 (N16R8, 4MB Flash, 2MB Octal PSRAM)**
> Firmware: **xiaozhi-esp32 (ESP-IDF)**
> Mọi thay đổi phải tuân thủ các rule dưới đây — nếu vi phạm sẽ bị reject PR.

---

## 1. 🔌 Quy tắc phân bổ GPIO (Pin Allocation)

### 1.1 GPIO đã được chiếm (KHÔNG dùng lại trừ khi được phê duyệt)

| GPIO  | Chức năng | Ghi chú |
|-------|-----------|---------|
| **GPIO0**  | BOOT button (reset SSID, active LOW) | Strapping pin |
| **GPIO4**  | I2S Mic WS  (INMP441) | |
| **GPIO5**  | I2S Mic SCK (INMP441) | |
| **GPIO6**  | I2S Mic SD  (INMP441) | |
| **GPIO7**  | I2S Spk DIN (MAX98357A) | |
| **GPIO8**  | I2C0 SDA (OLED SSD1306) | GPIO thường, không xung đột USB |
| **GPIO9**  | I2C0 SCL (OLED SSD1306) | GPIO thường, không xung đột USB |
| **GPIO15** | I2S Spk BCLK (MAX98357A) | |
| **GPIO16** | I2S Spk LRC (MAX98357A) | |
| **GPIO19** | USB D− | Reserved cho USB-CDC |
| **GPIO20** | USB D+ | Reserved cho USB-CDC |
| **GPIO26–32** | **Octal PSRAM / SPI Flash** | **CẤM dùng** (chip N16R8) |
| **GPIO47** | Wake button (TTP223, active HIGH) | |

### 1.2 Ràng buộc GPIO của ESP32-S3 (quan trọng!)

- ⚠️ **Strapping pins** của ESP32-S3: `GPIO0`, `GPIO3`, `GPIO45`, `GPIO46` — ảnh hưởng boot mode, phải test kỹ trước khi dùng.
- ⚠️ **Input-only pins** trên S3: `GPIO26–GPIO46` (một số) — **không có** driver output, **không có** internal pull-up/pull-down. Bắt buộc dùng pull-up/pull-down ngoài (10kΩ) nếu dùng làm input.
- ⚠️ **PSRAM pins** trên N16R8: `GPIO26–GPIO32` đã bị chiếm bởi Octal PSRAM → **tuyệt đối không touch**.
- ⚠️ **USB pins**: `GPIO19` (D−) và `GPIO20` (D+) reserved cho USB-CDC. Không dùng làm GPIO thường nếu muốn giữ Serial Monitor qua USB.
- ❌ Mọi GPIO dùng cho input phải có pull-up/pull-down (internal **hoặc** 10kΩ ngoài).

### 1.3 Quy tắc khai báo pin

- **Luôn** khai báo pin trong `main/boards/esp32s3-wroom-1/config.h` dưới dạng `GPIO_NUM_x`.
- **Không** hard-code số GPIO trong driver. Phải `#include "config.h"` rồi dùng macro.
- Khi thêm pin mới → cập nhật `PINOUT.md` và `config.h` **cùng lúc** trong cùng PR.
- Khi thay đổi pin nào cũng phải check **xung đột với strapping pin / PSRAM pin / input-only pin**.

---

## 2. ⚡ FreeRTOS & Dual-Core

### 2.1 Dual-Core Utilization

- ESP32-S3 có **2 core**: `Core 0` (PRO CPU) và `Core 1` (APP CPU). Mặc định Arduino chạy trên Core 1.
- **Background / network task** (Wi-Fi, BLE, MQTT) → pin vào **Core 0** bằng `xTaskCreatePinnedToCore()`.
- **Application / UI task** (audio codec, display, button) → pin vào **Core 1**.
- Ví dụ:

```c
xTaskCreatePinnedToCore(
    wifi_task,            // function pointer
    "wifi_task",          // name
    4096,                 // stack size (bytes)
    NULL,                 // param
    5,                    // priority
    NULL,                 // task handle
    0                     // core 0
);
```

### 2.2 Watchdog Timers (WDT) — TUYỆT ĐỐI tránh trigger

- ❌ **Không bao giờ** block CPU bằng `delay()` hoặc `while(1)` không có yield.
- ✅ Luôn dùng `vTaskDelay(pdMS_TO_TICKS(ms))` hoặc `vTaskDelayUntil()` để nhường CPU.
- ✅ Nếu task phải làm việc nặng → chia nhỏ thành nhiều bước, mỗi bước có `vTaskDelay(0)` hoặc `taskYIELD()`.
- ❌ Không dùng `delay()` > 100ms trong bất kỳ task nào.
- Nếu cần "treo chờ sự kiện" → dùng semaphore / queue / `ulTaskNotifyTake()`.

### 2.3 Task Stack Size — quy tắc đặt cỡ

| Loại task | Stack tối thiểu | Ghi chú |
|-----------|-----------------|---------|
| Idle task | 1024 | |
| GPIO / button | 2048 | |
| LED / display refresh | 2048 | |
| Wi-Fi / TLS | **4096+** | TLS cần nhiều stack |
| MQTT / HTTP client | 4096+ | |
| Audio codec (I2S) | 4096+ | buffer lớn |

- Trong debug: bật `CONFIG_ESP_SYSTEM_HLWDT_DETECT=y` và theo dõi `uxTaskGetStackHighWaterMark()` cho mỗi task để chỉnh size vừa đủ (dư ~25%).
- ❌ Không dùng magic number `2048`, `4096` trong code — phải đặt macro:

```c
#define WIFI_TASK_STACK_SIZE  4096
#define AUDIO_TASK_STACK_SIZE 6144
```

### 2.4 Thread Safety — Mutex & Queue

- **Bắt buộc** dùng `SemaphoreHandle_t` (mutex) hoặc `QueueHandle_t` khi chia sẻ:
  - I2C / I2S / SPI bus
  - Global state (display, audio buffer, config)
  - Cấu hình runtime (Wi-Fi credentials, NVS data)
- Pattern chuẩn:

```c
SemaphoreHandle_t i2c_mutex = xSemaphoreCreateMutex();
if (i2c_mutex == NULL) {
    ESP_LOGE(TAG, "Failed to create I2C mutex");
    return ESP_ERR_NO_MEM;
}

if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    // critical section
    xSemaphoreGive(i2c_mutex);
} else {
    ESP_LOGW(TAG, "I2C bus busy (timeout)");
}
```

- Mutex **phải** có timeout rõ ràng — `portMAX_DELAY` chỉ dùng khi thật sự cần chờ vô hạn.
- ❌ **Không** dùng disable interrupt (`portDISABLE_INTERRUPTS`) để "bảo vệ critical section" trong code application — chỉ dùng trong ISR ngắn.

---

## 3. 💾 Memory Management (SRAM & PSRAM)

### 3.1 PSRAM cho buffer lớn

- N16R8 có **2MB Octal PSRAM**. Bật `CONFIG_SPIRAM=y`, `CONFIG_SPIRAM_USE_MALLOC=y`.
- Buffer > 4KB (frame audio, bitmap, JSON parse lớn) → dùng:

```c
uint8_t *buf = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
if (buf == NULL) {
    ESP_LOGE(TAG, "PSRAM alloc failed (%u bytes)", size);
    return ESP_ERR_NO_MEM;
}
```

- Kiểm tra PSRAM free: `heap_caps_get_free_size(MALLOC_CAP_SPIRAM)`.
- Buffer nhỏ (< 1KB) → cấp phát từ SRAM internal để nhanh hơn.

### 3.2 Static vs Dynamic allocation

- ✅ **Ưu tiên static allocation** cho buffer có kích thước cố định (frame size, ring buffer, audio chunk) — tránh heap fragmentation.

```c
static uint8_t audio_buf[AUDIO_CHUNK_SIZE];   // ← ưu tiên
// thay vì: uint8_t *audio_buf = malloc(AUDIO_CHUNK_SIZE);
```

- Dùng `static const` cho lookup table, magic strings, font data.
- Dynamic allocation (`malloc`) chỉ dùng khi size phụ thuộc runtime (config từ NVS, payload size, v.v.).

### 3.3 Tránh Memory Leak

- ✅ Mọi `malloc` / `heap_caps_malloc` / `calloc` phải có `free` / `heap_caps_free` tương ứng.
- Khi code nhiều early-return → check rò rỉ: dùng `goto cleanup;` pattern.

```c
char *buf = malloc(256);
if (!buf) return ESP_ERR_NO_MEM;
if (parse_failed) goto cleanup;   // ← cleanup sẽ free(buf)
cleanup:
    free(buf);
    return err;
```

- Debug bằng `esp_get_free_heap_size()` log định kỳ, hoặc bật `CONFIG_HEAP_POISONING_COMPREHENSIVE=y`.

### 3.4 String class — TRÁNH trong Arduino core

- ❌ **Không dùng** `String` class (Arduino) — gây heap fragmentation cực nhanh.
- ✅ Dùng `std::string` (C++) hoặc `char[]` / `char *` (C) với `snprintf` / `strncpy`.
- Ví dụ:

```cpp
// ❌ Sai
String status = "WiFi: " + ssid + " (" + rssi + " dBm)";

// ✅ Đúng
char status[64];
snprintf(status, sizeof(status), "WiFi: %s (%d dBm)", ssid, rssi);
```

---

## 4. 🧱 Quy tắc Code C/C++ (ESP-IDF style)

### 4.1 Style & formatting

- Dùng **C++17** cho file `.cc`/`.cpp`, **C11** cho file `.c`.
- Indent: **4 spaces** (không tab).
- Brace style: **Allman** (mở `{` trên dòng riêng) cho function/class, **K&R** cho control flow.
- Max line length: **120 ký tự**.
- File header phải có comment bản quyền (nếu áp dụng) + mô tả ngắn.

### 4.2 Naming convention

| Loại | Quy tắc | Ví dụ |
|------|---------|-------|
| Macro / const | `UPPER_SNAKE_CASE` | `BOOT_BUTTON_GPIO` |
| Class | `PascalCase` | `Esp32S3Wroom1Board` |
| Method | `camelCase` | `initAudioCodec()` |
| Local variable | `snake_case_` (có underscore cuối cho member) | `i2c_bus_` |
| Enum value | `kPascalCase` hoặc `UPPER_SNAKE_CASE` | `kStateIdle` |
| Namespace | `lower_snake_case` | `driver::ssd1306` |

### 4.3 Framework preference — ESP-IDF vs Arduino

Tùy cấu trúc project hiện tại để chọn framework:

- ✅ Có `CMakeLists.txt` + file trong `main/` → dùng **ESP-IDF (C/C++ native)**.
- ✅ Có `main.cpp` với `#include <Arduino.h>` → dùng **Arduino framework (C++)**.
- ❌ **Không** mix 2 framework trong cùng 1 file (gây xung đột `vTaskDelay` vs `delay`, `digitalWrite` vs `gpio_set_level`).
- Dự án hiện tại dùng **ESP-IDF** (`xiaozhi-esp32`).

### 4.4 Bắt buộc khi viết code

- ✅ Mọi lệnh ESP-IDF API trả về `esp_err_t` **phải** bọc trong `ESP_ERROR_CHECK()` hoặc check thủ công + log.
- ✅ Tài nguyên (bus, handle, buffer) **phải** được giải phóng trong destructor hoặc hàm `deinit()`.
- ❌ **Không** dùng `printf()` trong production — phải dùng `ESP_LOGI/W/E`.

### 4.5 Logging chuẩn

```c
// Format: TAG tối đa 8 ký tự, mô tả module
static const char *TAG = "OledSsd13";

ESP_LOGI(TAG, "Initialized SDA=%d SCL=%d", sda, scl);
ESP_LOGW(TAG, "I2C timeout, retrying (%d/%d)", retry, max_retry);
ESP_LOGE(TAG, "Failed to init codec: %s", esp_err_to_name(err));
```

- Level mặc định: `INFO`. Debug build mới dùng `DEBUG`.
- Không log sensitive data (token, password, MAC cá nhân…).

---

## 5. ⚡ ISR & Power Management

### 5.1 Interrupt Service Routines (ISR) — phải cực ngắn

- ✅ ISR **chỉ** làm: set flag, ghi vào queue (`xQueueSendFromISR`), gọi `vTaskNotifyGiveFromISR`.
- ❌ **Không** gọi `Serial.print`, `printf`, `ESP_LOG*`, `delay`, `malloc` trong ISR.
- ❌ **Không** dùng mutex trong ISR (deadlock với task priority).
- ✅ Hàm ISR phải có attribute `IRAM_ATTR`:

```c
static volatile bool wake_flag = false;

static void IRAM_ATTR gpio_isr_handler(void *arg) {
    uint32_t gpio = (uint32_t)arg;
    wake_flag = true;
}
```

- Khi register ISR, dùng `ESP_INTR_FLAG_LEVEL3 | ESP_INTR_FLAG_IRAM` nếu cần độ trễ thấp.
- Debounce trong task, **không** debounce trong ISR.

### 5.2 Deep Sleep & RTC

- Khi dùng `esp_deep_sleep_start()`:
  - ✅ Biến cần giữ qua sleep → khai báo với `RTC_DATA_ATTR`:

```c
RTC_DATA_ATTR static int boot_count = 0;
RTC_DATA_ATTR static uint8_t last_state[16];
```

  - ❌ RAM thường **bị mất** sau deep sleep (chỉ RTC slow/fast memory giữ được).
  - ✅ Configure wake-up sources **trước khi** gọi `esp_deep_sleep_start()`:
    - `esp_sleep_enable_ext0_wakeup(GPIO_NUM_47, 1)` — wake khi GPIO HIGH
    - `esp_sleep_enable_timer_wakeup(uS)` — wake sau N giây
    - `esp_sleep_enable_touchpad_wakeup()` — wake từ touch
- Test deep sleep: xác nhận dòng điện rơi xuống < 50 µA (bằng `uCurrent` hoặc multimeter).

---

## 6. 📁 Quy tắc tổ chức file

### 6.1 Cấu trúc thư mục board

```
main/boards/esp32s3-wroom-1/
├── config.h           ← Pin macros + settings (BẮT BUỘC có)
├── config.json        ← sdkconfig append (BẮT BUỘC có)
├── esp32s3_wroom1_board.h
├── esp32s3_wroom1_board.cc
├── oled_ssd1306.h
└── oled_ssd1306.cc
```

### 6.2 Quy tắc đặt tên file

- Driver IC: `<tên_ic>_<role>.cc` (vd: `oled_ssd1306.cc`, `mic_inmp441.cc`).
- Board class: `<board_name>_board.cc` (snake_case).
- Header guard: `#pragma once` (ưu tiên) hoặc `#ifndef __FILE_H__`.

---

## 7. 🔨 Quy tắc Build & Flash

### 7.1 Quy trình build bắt buộc

```bat
:: 1. Set target (chỉ cần chạy 1 lần hoặc khi đổi chip)
idf.py set-target esp32s3

:: 2. Build với parallel
idf.py -j8 build

:: 3. Flash (chọn đúng COM port!)
idf.py -p COMx -b 921600 flash

:: 4. Monitor
idf.py -p COMx monitor
```

- **Không** commit file build (`.bin`, `.elf`, `.map`) lên git.
- Trước khi push phải chạy `idf.py build` thành công **0 warning mới** (warning cũ phải được document trong PR).

### 7.2 sdkconfig

- Mọi flag `sdkconfig` phải được append trong `config.json` qua `sdkconfig_append`, **không sửa trực tiếp `sdkconfig`**.
- Nếu cần flag mới → PR phải giải thích lý do + ảnh hưởng.

---

## 8. 🌿 Quy tắc Git & PR

### 8.1 Branch naming

| Loại | Format | Ví dụ |
|------|--------|-------|
| Feature | `feat/<short-desc>` | `feat/oled-ssd1306` |
| Bugfix  | `fix/<short-desc>` | `fix/mic-noise-gpio6` |
| Refactor | `refactor/<short-desc>` | `refactor/i2s-codec` |
| Docs | `docs/<short-desc>` | `docs/update-pinout` |

### 8.2 Commit message (Conventional Commits)

```
<type>(<scope>): <subject>

<body>

<footer>
```

- Types: `feat`, `fix`, `refactor`, `docs`, `test`, `chore`, `perf`.
- Subject: tối đa 72 ký tự, **không** viết hoa chữ cái đầu, **không** kết thúc bằng dấu chấm.

### 8.3 Quy tắc PR

- Mỗi PR **chỉ giải quyết 1 vấn đề** (không mix feat + fix).
- Phải có mô tả: **What** (làm gì) + **Why** (tại sao) + **How to test** (cách test).
- Nếu PR thay đổi pin/wiring → **bắt buộc** cập nhật `PINOUT.md` + sửa `config.h`.
- Không merge khi CI đỏ.

### 8.4 .gitignore bắt buộc

```
build/
sdkconfig.old
*.bin
*.elf
*.map
.vscode/
.DS_Store
```

---

## 9. 🧪 Quy tắc Testing

### 9.1 Trước khi push

- [ ] Code build thành công (0 warning mới).
- [ ] Flash lên board thật và boot không crash.
- [ ] Verify log boot có TAG board xuất hiện đúng.
- [ ] Nếu thay đổi audio → test mic self-test (xem log `MIC_TEST: PASS`).
- [ ] Nếu thay đổi display → verify OLED sáng, không trắng xóa / dump toàn 0xFF.
- [ ] Nếu thay đổi Wi-Fi → test connect, disconnect, reconnect, mất Wi-Fi giữa chừng.
- [ ] Nếu thay đổi deep sleep → đo dòng sleep bằng multimeter.

### 9.2 Hardware test

- Mỗi lần đổi pin phải đo bằng **multimeter** trước khi flash.
- Bus I2S chia **2 instance riêng**: Mic dùng `I2S_NUM_1`, Speaker dùng `I2S_NUM_0`.
- Bus I2C OLED dùng `I2C_NUM_0`, 100 kHz, có pull-up internal ở driver (và pull-up ngoài 4.7kΩ nếu dây > 10cm).
- Khi đổi pin input-only (`GPIO26–GPIO46`) → xác nhận pull-up ngoài 10kΩ đã gắn.

---

## 10. 📚 Quy tắc tài liệu

### 10.1 File bắt buộc phải có

- `README.md` — mô tả tổng quan dự án, cách build/flash nhanh.
- `PINOUT.md` — bảng pin đã verify (đã có sẵn, update khi đổi pin).
- `rules.md` — file này.
- `HUONG_DAN_SU_DUNG.md` — hướng dẫn sử dụng tiếng Việt (nếu có user-facing UI).

### 10.2 Quy tắc viết doc

- Dùng **tiếng Việt** cho doc nội bộ + comment giải thích "tại sao".
- Dùng **tiếng Anh** cho doc public, commit message, comment API.
- Mỗi function public phải có comment Doxygen ngắn gọn:

```cpp
/// Initialize I2C bus for OLED display.
/// @param sda SDA GPIO number
/// @param scl SCL GPIO number
/// @return ESP_OK on success, error code otherwise.
esp_err_t init_oled_i2c(gpio_num_t sda, gpio_num_t scl);
```

---

## 11. 🔒 Quy tắc bảo mật & an toàn

### 11.1 Wi-Fi Credentials

- ❌ **Không bao giờ** hard-code SSID/password trong source code.
- ✅ Ưu tiên lưu trong **NVS** (namespace `wifi`, key `ssid`, `password`).
- ✅ Nếu chưa có credential trong NVS → chạy provisioning:
  - **SmartConfig** (ESP-Touch) — đơn giản, 1 nút.
  - **Wi-Fi Manager** với captive portal (HTTP form).
  - Hoặc BLE provisioning (esp_prov).
- ❌ Không commit NVS dump có chứa password thật.

### 11.2 NVS / LittleFS / SPIFFS

- Dùng `nvs_open()` / `nvs_get_*` / `nvs_set_*` / `nvs_commit()` cho config nhỏ (SSID, token, settings).
- Dùng **LittleFS** (ưu tiên trên S3, SPIFFS đã deprecated) cho file lớn (cert, font, log).
- ✅ Luôn `close` handle / file sau khi dùng xong.
- ✅ Set `nvs_set_blob` với size chính xác, check `nvs_get_blob` với `required_size` để tránh buffer overflow.

### 11.3 TLS / HTTPS — bắt buộc bảo mật

- ✅ Mọi kết nối network ra ngoài **phải dùng TLS**:
  - ESP-IDF: dùng `esp_tls` API.
  - Arduino: dùng `WiFiClientSecure` thay vì `WiFiClient`.
- ✅ Validate certificate với **CA bundle** (`esp_crt_bundle_attach()`) hoặc pin cứng cert SHA-256.
- ❌ Không tắt `verify_peer` trừ khi đang dev local.
- Với MQTT: dùng port `8883` (TLS), không dùng `1883` (plaintext) trong production.
- Với HTTP API: dùng `https://` endpoint, không `http://`.

### 11.4 Quy tắc chung bảo mật

- ❌ **Không** commit API key, token, MAC address thật, password Wi-Fi, certificate private key.
- ❌ **Không** flash firmware lên board khác mà không kiểm tra `sdkconfig` đúng target.
- ⚠️ Khi sửa code liên quan Wi-Fi → test cả case **mất Wi-Fi** (phải retry/notify user, không crash).
- ⚠️ Mọi input từ network/MQTT/HTTP phải validate trước khi dùng (size, range, magic bytes, JSON schema).
- ⚠️ Khi dùng OTA → verify signature firmware trước khi apply.

---

## 12. 🐛 Quy trình xử lý bug

1. **Reproduce**: ghi lại log + bước tái hiện trong issue.
2. **Isolate**: tách bug ra branch riêng, viết test/log để xác nhận.
3. **Fix**: sửa tối thiểu, không "tiện tay refactor" lúc fix bug.
4. **Verify**: test lại trên board thật + check không regression.
5. **Document**: cập nhật `CHANGELOG.md` (nếu có) hoặc comment trong code.

---

## 13. ✅ Checklist trước khi merge vào `main`

- [ ] Code build sạch, 0 warning mới.
- [ ] Đã test trên phần cứng thật (không chỉ test trên simulator).
- [ ] `PINOUT.md` đã cập nhật nếu đổi pin.
- [ ] `config.h` / `config.json` đã cập nhật nếu đổi setting.
- [ ] Commit message đúng convention.
- [ ] PR description đầy đủ (What / Why / How to test).
- [ ] Không có file binary / build artifact trong commit.
- [ ] Không có secret / credential trong commit.
- [ ] Không có hard-coded Wi-Fi password / API key.
- [ ] Nếu dùng task mới → đã pin core đúng + stack size hợp lý.
- [ ] Nếu thay đổi ISR → đã thêm `IRAM_ATTR` + không log trong ISR.
- [ ] Nếu dùng PSRAM → đã check `heap_caps_malloc` thành công.
- [ ] Nếu thêm kết nối network → đã dùng TLS + validate cert.

---

## 📌 Lưu ý cuối

- Khi rule này cần thay đổi → mở PR riêng cho `rules.md`, **không sửa rule nền trong PR feature**.
- Mọi thành viên mới onboard **phải đọc file này trước** khi viết code đầu tiên.
- Nếu có câu hỏi / cần làm rõ rule → hỏi trong team channel trước khi tự ý quyết.

---

## 📎 Phụ lục: Bảng tra nhanh Strapping Pins

| Chip | Strapping pins |
|------|----------------|
| ESP32 (classic) | GPIO 0, 2, 5, 12, 15 |
| ESP32-S2 | GPIO 0, 45, 46 |
| **ESP32-S3** (của dự án này) | **GPIO 0, 3, 45, 46** |
| ESP32-C3 | GPIO 2, 8, 9 |

---

*File cập nhật: 2026-06-18 · Phiên bản: 2.0 · Maintainer: project owner*
