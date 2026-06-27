// main.cc - Mini-Xiaozhi P4.1 (OTA Update + WiFi)
#include <esp_log.h>
#include <esp_err.h>
#include <esp_timer.h>
#include <esp_rom_uart.h>
#include <nvs_flash.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cmath>
#include <vector>
#include <cstring>
#include "esp32s3_wroom1_board.h"
#include "esp32s3_audio_codec.h"
#include "ssd1306.h"
#include "wifi_manager.h"
#include "ota_update.h"

static const char* TAG = "Main";
static constexpr int kLoopChunkSamples = 240;
static constexpr int kLoopTaskStack    = 4096;
static constexpr int kLoopTaskPrio     = 5;
static constexpr int kLoopTaskCore     = 1;

// ===== Console Task =====
static void ConsoleTask(void* arg) {
    char line[128];
    Esp32S3AudioCodec* codec = (Esp32S3AudioCodec*)arg;
    ESP_LOGI(TAG, "=== Commands: wifi connect|status|clear, ota <url>|status, status, volume <n> ===");
    
    while (true) {
        printf("\n> ");
        fflush(stdout);
        if (fgets(line, sizeof(line), stdin) == NULL) { vTaskDelay(pdMS_TO_TICKS(100)); continue; }
        line[strcspn(line, "\r\n")] = 0;
        
        if (strncmp(line, "wifi connect ", 13) == 0) {
            char ssid[33] = {0}, pass[65] = {0};
            if (sscanf(line + 13, "%32s %64s", ssid, pass) == 2) {
                ESP_LOGI(TAG, "WiFi: connecting to %s", ssid);
                wifi_save_credentials(ssid, pass);
                wifi_connect(ssid, pass);
                if (wifi_wait_connected(30000) == ESP_OK) ESP_LOGI(TAG, "WiFi OK! IP: %s", wifi_get_ip_str());
                else ESP_LOGE(TAG, "WiFi failed");
            }
        }
        else if (strcmp(line, "wifi status") == 0) {
            ESP_LOGI(TAG, "WiFi: state=%d, IP=%s", wifi_get_state(), wifi_get_ip_str());
        }
        else if (strcmp(line, "wifi clear") == 0) { wifi_clear_credentials(); ESP_LOGI(TAG, "Cleared"); }
        else if (strncmp(line, "ota ", 4) == 0) {
            if (!wifi_is_connected()) ESP_LOGE(TAG, "WiFi not connected!");
            else { ESP_LOGI(TAG, "OTA start: %s", line + 4); ota_start_update(line + 4, NULL); }
        }
        else if (strcmp(line, "ota status") == 0) {
            ESP_LOGI(TAG, "OTA: state=%d, part=%s", ota_get_state(), ota_get_running_partition()->label);
        }
        else if (strcmp(line, "status") == 0) {
            const esp_partition_t* p = ota_get_running_partition();
            ESP_LOGI(TAG, "P4.1 OTA | Part: %s | WiFi: %s | Heap: %d",
                     p->label, wifi_is_connected()?"ON":"OFF", esp_get_free_heap_size());
        }
        else if (strncmp(line, "volume ", 7) == 0) { codec->SetOutputVolume(atoi(line + 7)); ESP_LOGI(TAG, "Vol: %s", line + 7); }
        else if (strcmp(line, "help") == 0) { ESP_LOGI(TAG, "wifi connect|status|clear, ota <url>|status, status, volume <n>"); }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ===== Beep =====
static void PlayBeep(Esp32S3AudioCodec* c, int freq, int dur, int amp) {
    int n = (24000 * dur) / 1000; if (n <= 0) return;
    std::vector<int16_t> buf(n);
    int fade = (24000 * 8) / 1000;
    const float PI2 = 6.28318530718f;
    float phase = 0, inc = PI2 * freq / 24000;
    for (int i = 0; i < n; i++) {
        float env = (i < fade) ? (float)i / fade : (i > n - fade) ? (float)(n - i) / fade : 1.0f;
        buf[i] = (int16_t)std::lround(std::sin(phase) * amp * env);
        phase += inc; if (phase >= PI2) phase -= PI2;
    }
    c->WriteSamples(buf.data(), n);
}

// ===== Loopback =====
static void LoopbackTask(void* arg) {
    auto* codec = static_cast<Esp32S3AudioCodec*>(arg);
    int16_t buf[kLoopChunkSamples];
    while (true) {
        int n = codec->ReadSamples(buf, kLoopChunkSamples);
        if (n > 0) codec->WriteSamples(buf, n);
        else vTaskDelay(pdMS_TO_TICKS(5));
    }
}

// ===== Main =====
extern "C" void app_main(void) {
    esp_rom_printf("[BOOT] P4.1 booting...\r\n");
    
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) { nvs_flash_erase(); nvs_flash_init(); }
    
    ESP_LOGI(TAG, "=== P4.1 OTA+WiFi ===");
    static Esp32S3Wroom1Board board; board.Initialize();

    Ssd1306 oled(board.GetI2CBus()); oled.Init(); oled.Clear();
    oled.DrawRect(0, 0, 128, 64, 1);
    oled.DrawText(8, 6, "Mini-Xiaozhi", 1);
    oled.DrawText(8, 22, "P4.1 OTA", 1);
    oled.DrawText(8, 38, "WiFi+OTA", 1);
    oled.Display();

    Esp32S3AudioCodec codec(board.GetSpeakerHandle(), board.GetMicHandle(), 24000, 24000);
    codec.SetOutputVolume(5); codec.EnableInput(true); codec.EnableOutput(true); codec.Start();
    
    codec.EnableInput(false); codec.EnableOutput(true);
    PlayBeep(&codec, 880, 150, 8000); vTaskDelay(pdMS_TO_TICKS(130));
    PlayBeep(&codec, 1320, 150, 8000); codec.EnableInput(true);

    wifi_init();
    if (wifi_connect_stored() == ESP_OK && wifi_wait_connected(15000) == ESP_OK) {
        ESP_LOGI(TAG, "Auto-WiFi: %s", wifi_get_ip_str());
    }
    
    ota_init();
    if (ota_is_pending()) { ESP_LOGI(TAG, "OTA pending - marking OK"); ota_mark_boot_success(); }

    xTaskCreatePinnedToCore(LoopbackTask, "loop", kLoopTaskStack, &codec, kLoopTaskPrio, nullptr, kLoopTaskCore);
    xTaskCreatePinnedToCore(ConsoleTask, "console", 4096, &codec, 3, nullptr, 1);
    
    ESP_LOGI(TAG, "=== Ready ===");
}