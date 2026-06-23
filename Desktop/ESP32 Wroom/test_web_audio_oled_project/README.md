# test_web_audio_oled_project — Hướng dẫn sử dụng

ESP32-S3 stream audio từ I2S mic lên web qua WebSocket, hiển thị OLED.

## 1. Chuẩn bị phần cứng

| Module | Chân ESP32-S3 |
|---|---|
| I2S Mic (INMP441 / SPH0645) | BCK, WS, DOUT |
| OLED SSD1306 (I2C) | SDA, SCL |
| USB serial | COM6 (CH343) |

## 2. Cấu hình WiFi (mặc định)

Sửa trong `main/test_web_audio_oled.cc`:
```c
#define DEFAULT_WIFI_SSID  "Thanh long-2.4G-ext"
#define DEFAULT_WIFI_PASS  "17111976"
```
Hoặc đổi qua NVS khi đã chạy (lưu vào flash, không cần flash lại).

## 3. Build & Flash

```bat
cd "C:\Users\HP\Desktop\ESP32 Wroom\test_web_audio_oled_project"
do_build.bat          :: build only (clean + build)
flash_only.bat        :: flash binary đã build xuống COM6
```
Mặc định flash qua **COM6** (sửa trong `flash_only.bat` nếu khác).

Sau khi flash, ESP tự reset và in log qua serial monitor @ 115200.

## 4. Mở web UI

1. Đợi log `WiFi connected, IP=<x.x.x.x>`
2. Trên máy tính/điện thoại **cùng mạng WiFi**, mở trình duyệt:
   ```
   http://<x.x.x.x>/
   ```
   (Không cần gõ `/index.html`, server tự serve `components/web/index.html`)

## 5. Giao diện web

- **Waveform (live PCM)** — vẽ sóng âm thanh thời gian thực
- **PEAK / RMS / FREQ** — chỉ số âm thanh (FFT 256 bins)
- **Spectrum** — phổ tần số
- **Nút Bắt đầu / Dừng** — bật tắt stream
- **Mode** — đổi cách hiển thị trên OLED (0=Waveform, 1=VU Meter, 2=Combined, 3=Text)
- **Speech → OLED** — nhấn nút 🎤 để nhận diện giọng nói (tiếng Việt/Anh) và gửi text xuống OLED (Chrome/Edge yêu cầu HTTPS hoặc http://localhost)

Trạng thái kết nối WebSocket hiện ở góc phải tiêu đề (● Connected / ● Disconnected).
Tự động reconnect mỗi 2s nếu rớt mạng.

## 6. API endpoints

| Method | URL   | Mô tả |
|---|---|---|
| GET    | `/`    | Trang web UI (index.html) |
| GET    | `/ws`  | WebSocket stream PCM int16, 16 kHz, mono |
| POST   | `/ctrl` | `mode=0\|1\|2\|3` đổi OLED mode; `stream=0\|1` bật/tắt stream |
| POST   | `/api/text` | JSON `{"text":"..."}` đổi text hiển thị lên OLED (mode 3) |

Ví dụ test với curl:
```bash
curl -X POST -d 'mode=1'  http://<ip>/ctrl   # chuyển sang VU Meter
curl -X POST -d 'stream=0' http://<ip>/ctrl  # tắt stream
curl -X POST -H 'Content-Type: application/json' -d '{"text":"xin chào"}' http://<ip>/api/text
curl -X POST -d 'mode=3' http://<ip>/ctrl   # chuyển sang Text mode
```

## 6b. Tính năng Speech → OLED

1. Mở web UI từ Chrome/Edge trên máy tính (hoặc điện thoại Android)
2. Bấm nút **🎤 Bắt đầu nhận diện** trong panel "Speech → OLED"
3. Cho phép trình duyệt dùng microphone
4. Nói tiếng Việt hoặc tiếng Anh — text nhận diện sẽ tự động gửi xuống ESP32 hiển thị trên OLED
5. ESP32 tự strip dấu tiếng Việt (`xin chào` → `xin chao`) để font 5x7 không cần UTF-8
6. Nếu text dài, tự động wrap và scroll trên OLED

Lưu ý: Web Speech API cần kết nối HTTPS, **trừ khi** truy cập qua `localhost` hoặc IP nội bộ (Chrome coi đây là secure context).

## 7. Debug

```bat
:: Xem log serial
python monitor_serial.py
:: hoặc
powershell -c "idf.py -p COM6 monitor"
```

Log quan trọng cần kiểm:
- `WiFi connected, IP=...`
- `Webserver started on port 80`
- `i2s_reader_task started on core X`
- `WS client connected`

## 8. Đổi WiFi không cần flash lại

Gửi POST `/ctrl` không hỗ trợ đổi WiFi. Để đổi:
1. Sửa `#define DEFAULT_WIFI_SSID/PASS` rồi build + flash lại, hoặc
2. Thêm endpoint `/wifi` (đã có sẵn hàm `wifi_creds_save`).
