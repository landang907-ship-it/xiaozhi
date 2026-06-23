# Project State — Mini-Xiaozhi (`xiaozhi_like`)

> **File này là "bộ nhớ ngoài"** để AI sync context mỗi lần mở session mới.
> Đọc file này + `audit.ps1` + `git log --oneline -20` + `PINOUT.md` là đủ để hiểu dự án.
>
> **Cập nhật mỗi khi xong 1 phase** (chỉ mất 30 giây).

---

## 📌 Decisions (LOCKED — KHÔNG đổi)

### Approach: **Cách C — Hybrid**
- Copy 20 files .cc/.h từ [78/xiaozhi-esp32](https://github.com/78/xiaozhi-esp32) upstream vào `xiaozhi_src/`
- Tự viết board driver `esp32s3_wroom1_board.cc` (~150 dòng)
- Build system CỦA MÌNH (CMakeLists.txt), KHÔNG dùng Python script của upstream
- Lý do: balance giữa nhanh (5-7 ngày) + hiểu sâu (custom được)

### Board: **ESP32-S3-WROOM-1 (N16R8)**
- Flash: **4MB**
- PSRAM: **2MB Octal**
- USB CDC: enabled (giữ Serial Monitor trên GPIO19/20)

### GPIO Mapping (từ `PINOUT.md` — đã verify 2026-06-17)
| Peripheral | Pins | Bus |
|---|---|---|
| OLED SSD1306 128×64 | SDA=GPIO8, SCL=GPIO9, addr=0x3C | I2C_NUM_0 |
| Speaker MAX98357A | BCLK=GPIO15, WS=GPIO16, DOUT=GPIO7 | **I2S_NUM_0** |
| Mic INMP441 | SCK=GPIO5, WS=GPIO4, DIN=GPIO6 | **I2S_NUM_1** |
| Wake button (TTP223) | GPIO47 | active-HIGH |
| Reset SSID (BOOT) | GPIO0 | active-LOW long-press 5s |
| LED | NC | dùng `NoLed()` |

### Audio settings (LOCKED)
- Sample rate IN: **24000 Hz**, mono
- Sample rate OUT: **24000 Hz**, mono
- Opus frame: **60ms = 1440 samples** (chuẩn upstream)
- OLED rotate 180°: `DISPLAY_MIRROR_X=true, DISPLAY_MIRROR_Y=true`

### Build settings (LOCKED)
- Wake word: **DISABLED** (dùng GPIO47 button)
- USB CDC: enabled
- Partition: `partitions/v2/4m.csv`
- Flash mode: DIO @ 80 MHz

---

## 🔍 Audit workflow (BẮT BUỘC cho AI mỗi action)

### Tại sao có audit system?
- Trong session 2026-06-21, AI đã **5 lần** nghĩ STATE.md đã update nhưng file không đổi (do sai diff format).
- User yêu cầu giám sát chặt → kết hợp 3 cơ chế:
  - **A) Re-read verify**: Sau MỌI `replace_in_file` STATE.md → `read_file` lại để confirm content đã đổi.
  - **B) Git commit**: Sau mỗi phase → `git add . && git commit -m "Px: ..."` để có lịch sử + rollback.
  - **C) AUDIT_LOG.md**: Append-only log, mỗi action ghi 1 dòng có timestamp.

### Setup 1 lần (đã làm 2026-06-21 18:18):
```bash
cd xiaozhi_like && git init
git add STATE.md README.md CMakeLists.txt sdkconfig.defaults .gitignore audit.ps1 AUDIT_LOG.md
git add main/CMakeLists.txt main/boards/esp32s3-wroom-1/
git add partitions/v2/4m.csv
git add xiaozhi_src/main/   # freeze 44 files upstream version
# Skip tmp/ (reference clone, ~12MB, có thể re-clone)
git commit -m "P1+P1.5: skeleton + clone xiaozhi-esp32 (44 files) + audit system"
```

### Đầu session mới (~1 phút):
1. `powershell -NoProfile -ExecutionPolicy Bypass -File xiaozhi_like\audit.ps1` — xem tình trạng
2. Đọc `xiaozhi_like/AUDIT_LOG.md` 20 dòng cuối
3. Đọc `xiaozhi_like/STATE.md` (file này)
4. `cd xiaozhi_like && git log --oneline -20`
5. Đọc `PINOUT.md`

### Trong session — quy tắc vàng:
- ❌ **KHÔNG** nói "đã update X" nếu chưa `read_file` verify
- ✅ Sau MỌI `replace_in_file` STATE.md → `read_file` lại + append `STATE_UPDATE` entry vào AUDIT_LOG.md
- ✅ Cuối phase → append `PHASE_DONE` + `git commit`
- ❌ KHÔNG xóa dòng trong AUDIT_LOG.md (append-only)

### Các file audit:
- `xiaozhi_like/AUDIT_LOG.md` — append-only log
- `xiaozhi_like/audit.ps1` — auto-check script
- `.git/` — git history (sẽ tạo bằng `git init`)

---

## 🗂 Project structure

```
xiaozhi_like/                              ← PROJECT ROOT
├── STATE.md                               ← file này (READ FIRST mỗi session)
├── README.md
├── AUDIT_LOG.md                           ← append-only log mỗi action
├── audit.ps1                              ← auto-check script
├── CMakeLists.txt                         ← root (chỉ include project.cmake)
├── sdkconfig.defaults
├── .gitignore
├── partitions/v2/4m.csv
├── main/                                  ← CODE CỦA MÌNH
│   ├── CMakeLists.txt
│   ├── main.cc                            ← TỰ VIẾT app_main — TODO Phase 2
│   └── boards/esp32s3-wroom-1/
│       ├── config.h                       ← copy từ PINOUT.md ✅
│       ├── config.json                    ← copy từ PINOUT.md ✅
│       ├── esp32s3_wroom1_board.cc        ← TỰ VIẾT (~150 LOC) — TODO Phase 2
│       └── oled_ssd1306.cc                ← copy từ xiaozhi_src — TODO Phase 2
├── xiaozhi_src/                           ← COPY từ upstream [78/xiaozhi-esp32] ✅
│   └── main/                              ← 44 files: application/audio/display/led/protocols/boards/common/...
├── tmp/                                   ← reference clone (git) — không build
│   └── xiaozhi_src_repo/
└── components/                            ← custom (web debug UI) — TODO Phase 6
    └── web/index.html
```

---

## 📅 Phases (timeline 5-7 ngày)

| Phase | Công việc | Verify được | Status |
|---|---|---|---|
| **P1** | Skeleton + CMakeLists + sdkconfig + partitions | files tạo đúng cấu trúc plan | ✅ Done |
| **P1.5** | Clone xiaozhi-esp32 + copy 44 files to xiaozhi_src/ | `xiaozhi_src/main/` có files, `tmp/` có clone | ✅ Done |
| **Audit** | AUDIT_LOG.md + audit.ps1 + Audit workflow section in STATE.md | `audit.ps1` chạy OK, git init OK | ✅ Done |
| **P2** | Driver port (mic+spk+oled+button) + main.cc + CMakeLists wires | `idf.py build` pass | ✅ Done (P2.1 ✅ + P2.2 ✅ + P2.3 ✅ + P2.4 build PASS commit `1f03844` ✅) |
| **P3** | Audio pipeline: mic → spk loopback | Code done (LoopbackTask Core1, vol=40, OLED "Loopback ON"), **build PASS verified 2026-06-23 06:10:30** (xiaozhi_like.bin 0x3d700 bytes, 84% free), flash+verify on board pending | ✅ Done (code + build PASS) — ⚠️ hardware test (flash + nghe) pending user |
| **P4** | Display UI (5 states) + WebSocket connect | OLED đổi state, ping OK | ⏳ Pending |
| **P5** | Xiaozhi protocol + audio streaming | Nói → server → nghe | ⏳ Pending |
| **P6** | Wake button + reset + web debug UI | Bấm GPIO47, web xem được | ⏳ Pending |
| **P7** | README + commit + push | Done, lên GitHub | ⏳ Pending |

---

## ⚠️ Known risks

- **LVGL version mismatch** với `esp_lcd_panel_ssd1306` → pin version cụ thể trong `idf_component.yml`
- **Audio pipeline timing** = phần khó nhất (6 chỗ timing) → test loopback trước khi ghép opus/ws
- **esp-sr model .bin** (~2MB) cần flash vào OTA partition → đã disable, không dùng

---

## 🔧 Environment

- ESP-IDF: v5.x (đã cài)
- Python: 3.x
- Build dir: `xiaozhi_like/build/`
- Flash port: **COM6**
- WiFi SSID: từ nvs (long-press GPIO0 = 5s để reset)

---

## 📝 Last update

- **Date**: 2026-06-22 06:40 (Asia/Bangkok) — session mới sau ~11h im lặng
- **By**: AI (Cline, MiniMax-M3)
- **Phases done**: P1 ✅ + P1.5 ✅ + Audit ✅ + P2.1 ✅ + P2.2 ✅ + P2.3 ✅ + **P2.4 build PASS `1f03844`** ✅ + **P3 code COMMITTED `ca72a3b`** ✅ + **P3 post-commit sync `52825aa`** ✅ (flash+verify pending user)
- **Commit history (11 commits, verify bằng `git log`)**:
  - `52825aa` (2026-06-22 06:40) **P3 post-commit sync**: AUDIT_LOG + PHASE_STATUS.json (working tree clean)
  - `ca72a3b` (2026-06-22 06:36) **P3 code**: LoopbackTask + SetOutputVolume + 5 build scripts + state sync
  - `1f03844` (2026-06-21 23:12) **P2.4 build PASS**: audio_codec.cc + FreeRTOS include + i2c dev handle + format fixes
  - `e2fc571` (2026-06-21 ~22:xx) **P2.4 sub-fix**: kconfgen em-dash → ASCII `--` in sdkconfig.defaults + CMakeLists + config.h
  - `ebe7910` (2026-06-21 19:30) P2.3 AUDIT_LOG: append 19:18-19:30 entries
  - `89f2c5e` (2026-06-21 19:27) P2.3 metadata: post-commit AUDIT_LOG + STATE Last update
  - `4323356` (2026-06-21 19:27) P2.2 fix + P2.3: audio_codec.h base + SSD1306 OLED + main wire
  - `bf875be` P2.2: Esp32S3AudioCodec
  - `508cb5a` Setup auto-advance policy (auto_full)
  - `d8ba69c` P2.1: HAL driver
  - `2e0655d` audit.ps1 v2 ASCII-only
  - `c6332fb` P1+P1.5+Audit system
- **P3 changes (NOW COMMITTED in `ca72a3b`)**:
  - `main/audio/audio_codec.h`: thêm `SetOutputVolume(int)` virtual + inline `ReadSamples`/`WriteSamples` forwarders (để loopback task gọi từ ngoài class)
  - `main/audio/audio_codec.cc`: definition cho `SetOutputVolume` (clamp 0..100, store vào `output_volume_`)
  - `main/main.cc`: thêm `LoopbackTask` (Core 1, chunk 240 samples = 10 ms @ 24 kHz, vol=40 conservative, EnableInput/Output true); OLED đổi sang "P3 OK" + "Loopback ON" + "mic -> spk"
- **5 build scripts (NOW COMMITTED in `ca72a3b`)**:
  - `do_p2_4_build.bat` — wrapper cho `idf.py build` với `PYTHONUTF8=1` (fix kconfgen charmap crash trên sdkconfig.defaults có UTF-8)
  - `do_p2_4d_build.bat` — bootloader-only build
  - `do_p2_4d_boot.bat` — direct ninja trên `build/bootloader`
  - `do_p2_4d_app.bat` — direct ninja trên `build/` (app)
  - `do_p2_4e_build_app.bat` — `idf.py -B build app` sau khi fix format warnings
- **Working tree**: **CLEAN** (sau commit `52825aa`, `git status --short` returns nothing)
- **Mode**: `auto-advance` (policy=`auto_full`) — **STOP_CONDITION #3 (Design Decision)** triggered for P4 (Display UI 5 states layout), AI MUST ask user before advancing
- **Next step**: ASK USER — (a) confirm advance to P4 (Display UI design), or (b) flash + verify P3 hardware test first, or (c) STOP/PAUSE/MANUAL

---

## 🆕 2026-06-23 session — FRESH BUILD PASS (xiaozhi_src wired)

- **Date**: 2026-06-23 06:30 (Asia/Bangkok) — session mới sau ~24h im lặng (sau 06:42 22/06)
- **By**: AI (Cline, MiniMax-M3)
- **Trigger**: User yêu cầu "đọc lại dự án" → AI phát hiện phases table ở P3 vẫn ở trạng thái "code done — hw test pending" NHƯNG build artifacts có thể cũ; user xác nhận cần "build PASS thật" (không phải artifacts cũ).

### A1 — Cleanup build cũ
- `run_a1_cleanup.ps1`: xóa `build/` + `sdkconfig` + `dependencies.lock` (nếu có)
- Sau cleanup: working tree clean, build dir gone

### A2 — Chạy `do_p2_4_build.bat` (build sạch)
- Bắt đầu: 06:03:30 (sau A1)
- Build wrapper: `PYTHONUTF8=1` (fix kconfgen charmap crash trên UTF-8 sdkconfig.defaults)
- App version (từ app_description): `xiaozhi_like 2.0 / db2d04c` (= HEAD)
- Toolchain: ESP-IDF v5.4 + xtensa-esp32s3 + GCC 14.2
- Log: `xiaozhi_like/p2_4_build.log` (1357 dòng, 1061 ninja steps + bootloader rebuild)

### A3 — Build result
- **1061/1061 steps completed** (bootloader + app)
- **BUILD SUCCESS** at 06:10:30 (4 phút 30 giây total build time)
- App bin: `xiaozhi_like.bin 0x3d700 bytes (251648 bytes), 84% free` (app partition 0x180000)
- Bootloader bin: `bootloader.bin 0x52d0 bytes (21200 bytes), 35% free`
- **No compile errors, no warnings (apart from normal format-truncation)**

### A4 — Verify bin timestamp mới
- `xiaozhi_like.bin`: `2026-06-23 06:10:29` (FRESH, just-built)
- `bootloader.bin`: `2026-06-23 06:10:24` (FRESH)
- `partition-table.bin`: `2026-06-23 06:05:11` (regenerated by cmake configure step)
- ⇒ xác nhận build PASS THẬT, không phải artifacts cũ

### xiaozhi_src wired (built objects thấy trong log)
- `main/audio/audio_codec.cc.obj` (P2.4)
- `main/audio/esp32s3_audio_codec.cc.obj` (P2.2)
- `main/audio/es8311_audio_codec.cc.obj` (xiaozhi_src upstream)
- `main/audio/es8379_audio_codec.cc.obj` (xiaozhi_src upstream)
- `main/audio/es8389_audio_codec.cc.obj` (xiaozhi_src upstream)
- `main/display/ssd1306.cc.obj` (xiaozhi_src upstream)
- `main/boards/esp32s3-wroom-1/oled_ssd1306.cc.obj` (P2.3)
- `main/protocols/websocket.cc.obj` (xiaozhi_src upstream, ready cho P4)
- `xiaozhi_src_lib_main.cc.obj` (xiaozhi_src upstream)

### Phases table — cập nhật
- **P3 row**: chuyển từ "✅ Done (code) — ⚠️ hw test pending" → **"✅ Done (code + build PASS verified 2026-06-23 06:10:30)"**
- **P4 row**: vẫn "⏳ Pending" (Design Decision trigger — AI sẽ ASK USER trước khi start)

### A5 — State sync (this commit)
- Update STATE.md (section này + phases table)
- Append AUDIT_LOG.md (BUILD_RESULT PASS + PHASE_DONE P3 BUILD_PASS_REAL + VERIFY_OK timestamp)
- Update PHASE_STATUS.json (current_phase_done=true, finished=2026-06-23 06:10, next_phase=P4)
- Commit hash: pending (will be set after commit)

### B — Flash (USER ACTION)
- User cần: (1) cắm board vào COM6, (2) `cd xiaozhi_like && idf.py -p COM6 flash`, (3) reset board, (4) nói vào mic, (5) nghe loopback audio từ speaker MAX98357A
- Sau flash thành công + nghe rõ → có thể advance P3 → P4 (sau design decision ASK)

---

## 💡 Cách AI sync context mỗi session mới

```bash
# 1. powershell xiaozhi_like\audit.ps1        — 10 giây (xem tổng quan)
# 2. Đọc xiaozhi_like/AUDIT_LOG.md 20 dòng cuối — 30 giây (xem AI đã làm gì)
# 3. Đọc xiaozhi_like/STATE.md (file này)     — 30 giây
# 4. cd xiaozhi_like && git log --oneline -20 — 10 giây
# 5. cat PINOUT.md                             — 20 giây
# Tổng: ~1.5 phút là đồng bộ xong
```

---


## 🤖 Auto-advance policy (P2.2+)

### Quy tắc
- AI chạy `audit.ps1` SAU MỖI phase. Nếu all `[OK]` + working tree clean → AUTO advance sang phase kế tiếp.
- KHÔNG hỏi user trừ khi thuộc **stop_conditions** (xem `PHASE_STATUS.json`).

### Stop conditions (AI BẮT BUỘC dừng)
1. **Audit fail**: Bất kỳ `[NG]` hoặc `[!]` trong audit report
2. **Git dirty**: Working tree có uncommitted changes sau khi commit
3. **Design decision**: Phase có nhiều cách làm (vd: chọn thư viện, chọn kiến trúc)
4. **User override**: User gõ `STOP` / `PAUSE` / `MANUAL` / `SKIP`

### User controls
| Command | Effect |
|---|---|
| (không gõ gì) | Continue auto-advance |
| `STOP` | Dừng ngay, save state |
| `PAUSE` | Dừng nhưng giữ current_phase, không advance |
| `MANUAL` | Tắt auto-advance, hỏi user mỗi phase |
| `SKIP` | Nhảy phase (vd `next_phase=P3`) |

### Files liên quan
- `PHASE_STATUS.json` — current_phase, next_phase, policy
- `audit.ps1` check #7 — validate PHASE_STATUS.json
- `AUDIT_LOG.md` — mỗi advance ghi 1 entry
- `STATE.md` section này — giải thích policy

## 🛠 P2.3 hardware test pattern (P2.4 verify)
- OLED hiển thị border + "Mini-Xiaozhi" (8,10) + "P2.3 OK" (20,26) + "I2C+I2S ready" (4,46)
- P2.4 sẽ: `cd xiaozhi_like && idf.py build` (or cmake + ninja) → fix compile errors if any → `idf.py -p COM6 flash` → reset board → xem OLED hiện pattern + serial log
```
