# 🎤 Hướng dẫn Test Microphone INMP441

## ✅ Trạng thái hiện tại
- **Loa (Speaker)**: ✅ OK
- **Mic**: 🔄 Đang build + flash...

## 📋 Các bước Test Mic

### 1️⃣ **Chờ flash xong** (build đang chạy background)
- Khi flash xong, board sẽ reset và hiện prompt:
```
mic>
```

### 2️⃣ **Kiểm tra kết nối đơn giản**
```
mic> init
mic> read 1024
```
**Kỳ vọng:**
- Peak > 0 (nếu có âm thanh môi trường)
- RMS > 0
- Nếu peak = 0 → kiểm tra wiring (GPIO4/5/6)

### 3️⃣ **Test ngưỡng threshold**
```
mic> thresh 5000      # Đặt ngưỡng peak = 5000
mic> watch 10         # Theo dõi 10 giây, vỗ tay hoặc nói to
```
**Kỳ vọng:**
- Khi vỗ tay → thấy log `[THRESHOLD EXCEEDED]`
- Nếu không thấy → threshold quá cao hoặc mic không nhận được âm thanh

### 4️⃣ **Test tần số**
```
mic> fft 2048         # Ước tính tần số từ 2048 samples
```
**Kỳ vọng:**
- Nói "Aaaaaa" (kéo dài) → thấy tần số ≈ 200-300 Hz
- Nói "Eeeeee" → thấy tần số ≈ 2000-3000 Hz
- Nói chuyện bình thường → thấy tần số ≈ 100-500 Hz

### 5️⃣ **Test sample rate**
```
mic> rate 44100       # Đổi sang 44100 Hz
mic> read 2048
```
Quan sát peak/RMS có thay đổi không.

### 6️⃣ **Kiểm tra dữ liệu thô (Debug wiring)**
```
mic> hex 128          # Dump 128 samples dạng hex
```
**Kỳ vọng:**
- Các giá trị khác nhau (không toàn 0 hoặc 0x800000)
- Nếu toàn 0 → kiểm tra WS/SCK/SD pins
- Nếu toàn 0x800000 → có thể SCK/WS bị đảo

### 7️⃣ **Chạy toàn bộ test**
```
mic> all              # Chạy init + read + watch + fft liên tiếp
```

## 🔧 Xử lý lỗi

| Triệu chứng | Nguyên nhân | Cách sửa |
|-------------|-----------|---------|
| peak = 0, RMS = 0 | Wiring sai | Kiểm tra GPIO4(WS), GPIO5(SCK), GPIO6(SD) |
| peak luôn max (0x7FFFFF) | SCK/WS bị đảo | Đảo pin SCK và WS |
| Build lỗi `i2s_std.h` | Cache cũ | Chạy `idf.py fullclean` rồi rebuild |
| Console không nhận lệnh | Baud rate sai | Đảm bảo Serial Monitor ở 115200 |

## 📍 Pinout (GPIO)
```
INMP441      ESP32-S3
━━━━━━━━━━━━━━━━━━━━
WS (LRCLK)  →  GPIO4
SCK (BCLK)  →  GPIO5
SD (DIN)    →  GPIO6
VCC         →  3.3V
GND         →  GND
L/R         →  GND (Left channel)
```

## 📊 Định dạng Audio
- **Sample rate**: 24000 Hz (default), có thể đổi 16k/22.05k/32k/44.1k/48k
- **Bit depth**: 24-bit
- **Channels**: Mono (Left)
- **I2S bus**: I2S_NUM_1 (không xung đột với speaker)

## 🎯 Mục tiêu
✅ peak > 0 khi có âm thanh  
✅ threshold detect hoạt động  
✅ FFT ước tính tần số chính xác  
✅ Hex dump có giá trị thay đổi  
