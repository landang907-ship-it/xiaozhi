# FIX MILESTONE — Phiên đánh giá P3 (2026-06-24)

## Bối cảnh
3 project test độc lập đều ĐẠT (mic, speaker, oled), nhưng khi gộp vào
`xiaozhi_like` thì OLED chết + loa rè. Phiên này xác định root cause.

## LUẬT GPIO (BẤT BIẾN)
Chân GPIO của test_mic / test_speaker / test_oled là CHUẨN VÀNG.
Mọi code trong xiaozhi_like phải dùng đúng các chân này.
KHÔNG được đổi chân. Nếu config.h lệch → sửa config.h cho khớp test.

| Ngoại vi | Tín hiệu | Chân (Chuẩn) |
|---|---|---|
| Speaker (I2S0) | BCLK | GPIO15 |
| Speaker (I2S0) | WS | GPIO16 |
| Speaker (I2S0) | DOUT | GPIO7 |
| Mic (I2S1) | SCK/BCLK | GPIO5 |
| Mic (I2S1) | WS | GPIO4 |
| Mic (I2S1) | DIN | GPIO6 |
| OLED (I2C0) | SDA | GPIO8 |
| OLED (I2C0) | SCL | GPIO9 |

## Nguyên lý cốt lõi
Test "đạt" = 1 ngoại vi + 1 cấu hình + KHÔNG tương tác.
Tích hợp lộ ra: (a) config lệch phần cứng, (b) code viết lại thay vì copy,
(c) tương tác mới chưa từng test.

## 3 Root Cause (bằng chứng từ log COM6)

### RC-1: OLED sai chân I2C  →  `SSD1306 init failed: ESP_ERR_INVALID_STATE`
| | test_oled (ĐẠT) | xiaozhi_like (LỖI) |
|---|---|---|
| SDA | **GPIO8** ✅ | GPIO1 ❌ |
| SCL | **GPIO9** ✅ | GPIO2 ❌ |
- Log xác nhận: `I2C0 ready (SDA=1, SCL=2)` → không thiết bị nào ACK.
- File: `main/boards/esp32s3-wroom-1/config.h` dòng 41-42.
- ĐÃ SỬA: SDA 1→8, SCL 2→9 (2026-06-24).

### RC-2: OLED đổi driver esp_lcd → raw i2c
- test_oled dùng `esp_lcd_panel_ssd1306` (vendor-tested, auto-detect 0x3C/0x3D, scan bus).
- xiaozhi_like tự viết raw `i2c_master_transmit` → mất auto-detect, sai chân là chết.
- File: `main/boards/esp32s3-wroom-1/ssd1306.cc`.
- ĐÃ SỬA: port sang esp_lcd như test_oled (2026-06-24).

### RC-3: Loa rè — clamp thay vì scale 24→16 bit
- Log: `peak=32768` liên tục = clip mọi mẫu.
- Code clamp giá trị 24-bit (±8,388,607) xuống ±32767 thay vì `>>8` scale.
- File: `main/audio/esp32s3_audio_codec.cc` dòng 81-87.
- test_mic không lộ vì chỉ tính RMS; test_speaker phát sine có sẵn.
- ĐÃ SỬA: đổi clamp → `dest[i] = raw[i] >> 16` (2026-06-24).

### Phụ: double-enable I2S  →  `channel already enabled`
- `board.Initialize()` enable kênh, rồi `codec.EnableInput/Output()` enable lại.
- File: `main.cc` dòng 103-104 + `esp32s3_audio_codec.cc` dòng 43-61.
- ĐÃ SỬA: bỏ double-enable (2026-06-24).

## Bài học (chống lặp lỗi)
- KHÔNG viết lại code đã test — COPY nguyên văn rồi mới refactor.
- config.h phải khớp PINOUT.md + test = chuẩn vàng.
- Mỗi tương tác mới (loopback, shared bus) phải test riêng trước khi tin.

## Tiêu chí PASS cột mốc
- [ ] OLED hiện text "Mini-Xiaozhi / P3 OK" trên màn hình.
- [ ] Loa phát lại tiếng mic KHÔNG rè (peak < 32768, không clip).
- [ ] Không còn log lỗi `INVALID_STATE` / `already enabled`.
- [ ] Log I2C show SDA=8, SCL=9.

## Lịch sử fix
| Ngày | Người | Mô tả |
|---|---|---|
| 2026-06-24 | AI | RC1: config.h SDA 1→8, SCL 2→9 |
| 2026-06-24 | AI | RC2: port ssd1306.cc → esp_lcd |
| 2026-06-24 | AI | RC3: audio Read() clamp → shift 16 |
| 2026-06-24 | AI | Phụ: bỏ double-enable I2S |
