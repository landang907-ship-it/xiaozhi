// main.cc
// app_main entry point.
//
// Phase 3 verify (P2.x + P3):
//   1. Board HAL: I2C0 (SSD1306 OLED @ GPIO8/9, addr 0x3C)
//                 I2S0 (MAX98357A spk out, 16-bit slot, GPIO15/16/7)
//                 I2S1 (INMP441 mic in, 32-bit slot, GPIO4/5/6)
//   2. Audio codec (Esp32S3AudioCodec on top of HAL handles)
//   3. OLED: title + status line + "Loopback ON" indicator + flush
//   4. Audio loopback task: codec.Read(buf) -> codec.Write(buf) on Core 1
//
// Build chain (post P3 commit):
//   cmake .. -G Ninja -B build -DSDKCONFIG_DEFAULTS=... && ninja -C build flash

#include <esp_log.h>
#include <esp_err.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_rom_uart.h>
#include <cmath>
#include <vector>
#include "esp32s3_wroom1_board.h"
#include "esp32s3_audio_codec.h"
#include "ssd1306.h"

static const char* TAG = "Main";

// Audio loopback chunk size.
static constexpr int kLoopChunkSamples = 240;
static constexpr int kLoopTaskStack    = 4096;
static constexpr int kLoopTaskPrio    = 5;
static constexpr int kLoopTaskCore     = 1;  // APP CPU (Core 1)

// --- Boot beep helpers ---
// Play a sine-wave beep through the codec.
// freq_hz: tone frequency (e.g. 1000)
// duration_ms: how long the tone lasts
// amplitude: peak value 0..32767 (e.g. 8000 = ~25% of full scale)
static void PlayBeep(Esp32S3AudioCodec* codec, int freq_hz, int duration_ms, int amplitude) {
    constexpr int SAMPLE_RATE = 24000;
    int num_samples = (SAMPLE_RATE * duration_ms) / 1000;
    if (num_samples <= 0) return;

    // Generate sine wave with fade-in/out envelope (8 ms each side).
    std::vector<int16_t> buf(num_samples);
    const int fade_ms = 8;
    const int fade_samples = (SAMPLE_RATE * fade_ms) / 1000;
    const float TWO_PI = 6.28318530718f;
    const float phase_inc = TWO_PI * freq_hz / SAMPLE_RATE;
    float phase = 0;

    for (int i = 0; i < num_samples; i++) {
        float env = 1.0f;
        if (i < fade_samples) {
            env = (float)i / fade_samples;
        } else if (i > num_samples - fade_samples) {
            env = (float)(num_samples - i) / fade_samples;
        }
        float s = std::sin(phase) * amplitude * env;
        buf[i] = (int16_t)std::lround(s);
        phase += phase_inc;
        if (phase >= TWO_PI) phase -= TWO_PI;
    }

    codec->WriteSamples(buf.data(), num_samples);
}

// Play two beeps on boot to verify speaker is working.
// Disables loopback first to avoid mic noise feedback during beep playback.
static void PlayBootBeeps(Esp32S3AudioCodec* codec) {
    ESP_LOGI(TAG, "Playing boot beeps...");
    // Disable loopback to prevent mic noise feedback during beeps.
    codec->EnableInput(false);
    codec->EnableOutput(true);

    // Beep 1: 880 Hz, 150 ms
    PlayBeep(codec, 880, 150, 8000);
    // Silent gap ~130 ms
    vTaskDelay(pdMS_TO_TICKS(130));
    // Beep 2: 1320 Hz, 150 ms
    PlayBeep(codec, 1320, 150, 8000);

    // Re-enable mic input for loopback.
    codec->EnableInput(true);
    ESP_LOGI(TAG, "Boot beeps done.");
}

static void LoopbackTask(void* arg) {
    auto* codec = static_cast<Esp32S3AudioCodec*>(arg);
    int16_t buf[kLoopChunkSamples];
    ESP_LOGI(TAG, "[loop] started on core %d, chunk=%d samples (%.1f ms)",
             xPortGetCoreID(), kLoopChunkSamples,
             (float)kLoopChunkSamples * 1000.0f / 24000.0f);

    int64_t last_stats_us = 0;
    int64_t total_read = 0;
    int64_t total_write = 0;

    while (true) {
        int n = codec->ReadSamples(buf, kLoopChunkSamples);
        if (n > 0) {
            total_read += n;
            int w = codec->WriteSamples(buf, n);
            total_write += w;
            if (w != n) {
                ESP_LOGW(TAG, "[loop] underrun: read=%d write=%d", n, w);
            }

            // Log peak/RMS every ~1 second to diagnose mic input level.
            int64_t now_us = esp_timer_get_time();
            if (now_us - last_stats_us > 1'000'000) {
                int32_t peak = 0;
                int64_t sum_sq = 0;
                for (int i = 0; i < n; i++) {
                    int32_t absv = std::abs((int32_t)buf[i]);
                    if (absv > peak) peak = absv;
                    sum_sq += (int64_t)buf[i] * buf[i];
                }
                float rms = sqrtf((float)sum_sq / n);
                ESP_LOGI(TAG, "[loop stats] read=%lld write=%lld peak=%" PRId32 " rms=%.1f",
                         total_read, total_write, peak, rms);
                last_stats_us = now_us;
            }
        } else {
            // No input available; yield briefly so we don't busy-loop.
            vTaskDelay(pdMS_TO_TICKS(5));
        }
    }
}

extern "C" void app_main(void) {
    // esp_rom_printf bypasses esp_log buffer — always goes to UART immediately.
    esp_rom_printf("[BOOT] app_main running, version P3-verify\r\n");
    fflush(stdout);
    ESP_LOGI(TAG, "=== Mini-Xiaozhi boot (P3 verify) ===");

    // 1) Board HAL: I2C0 + I2S0 (spk) + I2S1 (mic)
    static Esp32S3Wroom1Board board;
    board.Initialize();

    // 2) SSD1306 OLED on top of I2C bus from board.
    Ssd1306 oled(board.GetI2CBus());
    oled.Init();
    oled.Clear();
    oled.DrawRect(0, 0, 128, 64, 1);     // outer border
    oled.DrawText(8, 6,  "Mini-Xiaozhi", 1);
    oled.DrawText(8, 22, "P3 OK", 1);
    oled.DrawText(8, 38, "Loopback ON", 1);
    oled.DrawText(8, 54, "B2-fixed", 1);
    oled.Display();
    ESP_LOGI(TAG, "OLED test pattern flushed");

    // 3) Audio codec.
    Esp32S3AudioCodec codec(board.GetSpeakerHandle(), board.GetMicHandle(),
                            24000, 24000);
    codec.SetOutputVolume(5);  // Very low volume to eliminate feedback noise
    codec.EnableInput(true);
    codec.EnableOutput(true);
    codec.Start();
    ESP_LOGI(TAG, "Audio codec ready (in=%d Hz, out=%d Hz, vol=%d)",
             codec.input_sample_rate(), codec.output_sample_rate(),
             codec.output_volume());

    // 3b) Play two beeps to verify speaker works on boot/reset.
    // Loopback is disabled during beep to prevent mic noise feedback.
    PlayBootBeeps(&codec);

    // 4) [B2] Loopback re-enabled with fixed codec (MONO Read + >>8 shift).
    ESP_LOGI(TAG, "[B2] Loopback ENABLED (fixed codec)");
    BaseType_t ok = xTaskCreatePinnedToCore(
        LoopbackTask, "audio_loop",
        kLoopTaskStack, &codec,
        kLoopTaskPrio, nullptr, kLoopTaskCore);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "Failed to spawn loopback task");
    } else {
        ESP_LOGI(TAG, "Loopback task spawned on core %d", kLoopTaskCore);
    }

    // 5) Idle loop so user can read serial monitor + see OLED pattern.
    ESP_LOGI(TAG, "=== Idle loop (monitor serial, speak near mic) ===");
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
