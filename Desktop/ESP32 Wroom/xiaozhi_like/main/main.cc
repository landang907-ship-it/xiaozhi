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
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esp32s3_wroom1_board.h"
#include "esp32s3_audio_codec.h"
#include "ssd1306.h"

static const char* TAG = "Main";

// 10 ms of 24 kHz mono PCM. Small enough to be low-latency, big enough
// that i2s_channel_read/write don't busy-spin on every call.
static constexpr int kLoopChunkSamples = 240;
static constexpr int kLoopTaskStack    = 4096;
static constexpr int kLoopTaskPrio     = 5;
static constexpr int kLoopTaskCore     = 1;  // APP CPU (Core 1)

static void LoopbackTask(void* arg) {
    auto* codec = static_cast<Esp32S3AudioCodec*>(arg);
    int16_t buf[kLoopChunkSamples];
    ESP_LOGI(TAG, "[loop] started on core %d, chunk=%d samples (%.1f ms)",
             xPortGetCoreID(), kLoopChunkSamples,
             (float)kLoopChunkSamples * 1000.0f / 24000.0f);

    while (true) {
        int n = codec->ReadSamples(buf, kLoopChunkSamples);
        if (n > 0) {
            int w = codec->WriteSamples(buf, n);
            if (w != n) {
                ESP_LOGW(TAG, "[loop] underrun: read=%d write=%d", n, w);
            }
        } else {
            // No input available; yield briefly so we don't busy-loop.
            vTaskDelay(pdMS_TO_TICKS(5));
        }
    }
}

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "=== Mini-Xiaozhi boot (P3 verify) ===");

    // 1) Board HAL: I2C0 + I2S0 (spk) + I2S1 (mic)
    static Esp32S3Wroom1Board board;
    board.Initialize();

    // 2) SSD1306 OLED on top of I2C bus from board.
    Ssd1306 oled(board.GetI2CBus(), board.OLED_I2C_ADDR, 128, 64);
    oled.Init();
    oled.Clear();
    oled.DrawRect(0, 0, 128, 64, 1);     // outer border
    oled.DrawText(8, 6,  "Mini-Xiaozhi", 1);
    oled.DrawText(8, 22, "P3 OK", 1);
    oled.DrawText(8, 38, "Loopback ON", 1);
    oled.DrawText(8, 54, "mic -> spk", 1);
    oled.Display();
    ESP_LOGI(TAG, "OLED test pattern flushed");

    // 3) Audio codec.
    Esp32S3AudioCodec codec(board.GetSpeakerHandle(), board.GetMicHandle(),
                            24000, 24000);
    codec.SetOutputVolume(40);   // conservative; mic+spk near each other can squeal
    codec.EnableInput(true);
    codec.EnableOutput(true);
    codec.Start();
    ESP_LOGI(TAG, "Audio codec ready (in=%d Hz, out=%d Hz, vol=%d)",
             codec.input_sample_rate(), codec.output_sample_rate(),
             codec.output_volume());

    // 4) Spawn loopback task on Core 1 (APP CPU keeps WiFi stack on Core 0).
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
