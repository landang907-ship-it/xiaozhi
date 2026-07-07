# Project State - P8 Voice Assistant

## Mục tiêu
Hoàn thành P8 - Full Voice Assistant với tích hợp:
- Wake word detection (button GPIO47)
- WebSocket communication
- Audio capture/streaming
- TTS playback
- OLED status display

## Trạng thái hiện tại: ✅ Hoàn thành

## Các file đã tạo/cập nhật

### 1. xiaozhi_like/main/main.cc (P8)
- Voice Assistant State Machine: IDLE, READY, LISTENING, THINKING, SPEAKING, ERROR
- WiFi Auto-Connect Task - tự động quét và kết nối mạng đã biết
- WebSocket integration với callbacks
- Mic audio streaming (480 samples @ 24kHz)
- OLED display với trạng thái real-time
- Console commands

### 2. xiaozhi_like/main/wake_word.cc
- Button wake on GPIO47 (BOOT_BUTTON_GPIO)
- Callback mechanism cho wake detection

### 3. xiaozhi_like/main/websocket_client.cc
- WebSocket client với esp_websocket_client
- Binary/text message handling
- Auto-reconnect

### 4. xiaozhi_like/main/stream_player.cc
- Audio stream player với ring buffer
- Volume control

### 5. xiaozhi_like/main/audio_stream.cc
- HTTP/HTTPS audio streaming

### 6. send_wifi_cmd.py (helper script)
- Script gửi lệnh WiFi qua serial

## Các component modules

### WiFi Auto-Connect
Danh sách mạng đã biết (hardcoded):
- Lan-mini / 123456788
- Thanh long-2.4G-ext / 17111976
- Thanh / long-2.4G-ext

Tự động quét mỗi 30 giây nếu chưa kết nối.

### Voice Assistant State Machine
```
IDLE -> (WiFi+WS connected) -> READY
READY -> (button wake) -> LISTENING -> THINKING -> SPEAKING -> READY
ERROR -> (2s) -> READY
```

### Console Commands
- `wifi add <ssid> <pass>` - Kết nối WiFi
- `wifi status` - Trạng thái WiFi
- `ws <url>` - Kết nối WebSocket
- `status` - Trạng thái VA/WiFi/WS
- `volume <0-100>` - Chỉnh âm lượng
- `wake` - Kích hoạt thủ công
- `reset` - Reset trạng thái

## Build và Flash

```bash
# Build
cd xiaozhi_like
idf.py build

# Flash
idf.py -p COM6 erase-flash flash monitor
```

## WiFi Credentials
- SSID: Lan-mini
- Password: 123456788

## Kế hoạch tiếp theo
1. Test P8 trên hardware
2. Tối ưu WiFi auto-connect
3. Thêm nhiều mạng WiFi vào danh sách
4. Test WebSocket server integration

## Ngày cập nhật
2026-07-05
