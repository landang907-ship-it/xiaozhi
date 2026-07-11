// main.cc - P8 Full Voice Assistant
// Integrates: wake word, WebSocket, audio capture/streaming, TTS playback
#include <esp_log.h>
#include <esp_err.h>
#include <esp_timer.h>
#include <esp_rom_uart.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cmath>
#include <vector>
#include <cstring>
#include <algorithm>
#include "esp32s3_wroom1_board.h"
#include "esp32s3_audio_codec.h"
#include "ssd1306.h"
#include "wifi_manager.h"
#include "wake_word.h"
#include "websocket_client.h"
#include "stream_player.h"

static const char* TAG = "Main";

// ===== Voice Assistant States =====
typedef enum {
    VA_STATE_IDLE = 0,
    VA_STATE_READY,
    VA_STATE_LISTENING,
    VA_STATE_THINKING,
    VA_STATE_SPEAKING,
    VA_STATE_ERROR
} va_state_t;

static const char* va_state_str(va_state_t s) {
    switch (s) {
        case VA_STATE_IDLE: return "IDLE";
        case VA_STATE_READY: return "READY";
        case VA_STATE_LISTENING: return "LISTENING";
        case VA_STATE_THINKING: return "THINKING";
        case VA_STATE_SPEAKING: return "SPEAKING";
        case VA_STATE_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

// ===== Global State =====
static va_state_t s_va_state = VA_STATE_IDLE;
static Esp32S3Wroom1Board* s_board = nullptr;
static Esp32S3AudioCodec* s_codec = nullptr;
static Ssd1306* s_oled = nullptr;
static TaskHandle_t s_mic_task = NULL;
static TaskHandle_t s_display_task = NULL;
static bool s_streaming_audio = false;
static bool s_speaking = false;
static uint64_t s_speech_start_time = 0;
static char s_ws_url[256] = {0};
static bool s_ws_display_connected = false;  // Display state for WS

// ===== WiFi Auto-Connect =====
#define NUM_KNOWN_NETWORKS 4
static struct {
    const char* ssid;
    const char* password;
} s_known_networks[NUM_KNOWN_NETWORKS] = {
    {"Lan-mini", "123456788"},
    {"Thanh  long-2.4G-ext", "17111976"},
    {"Thanh", "long-2.4G-ext"},
    {"", ""}  // Empty - end marker
};

// WiFi scan and auto-connect task
static void WifiAutoConnectTask(void* arg) {
    ESP_LOGI(TAG, "WiFi Auto-Connect task started");
    
    while (true) {
        // Only try to connect if not already connected
        if (!wifi_is_connected() && wifi_get_state() != WIFI_STATE_CONNECTING) {
            ESP_LOGI(TAG, "Scanning for known WiFi networks...");
            
            // Do WiFi scan
            wifi_scan_config_t scan_config = {};
            scan_config.scan_type = WIFI_SCAN_TYPE_PASSIVE;
            scan_config.scan_time.passive = 300;
            
            esp_err_t err = esp_wifi_scan_start(&scan_config, true);
            if (err == ESP_OK) {
                uint16_t ap_count = 0;
                esp_wifi_scan_get_ap_num(&ap_count);
                
                if (ap_count > 0) {
                    wifi_ap_record_t* ap_info = (wifi_ap_record_t*)malloc(sizeof(wifi_ap_record_t) * ap_count);
                    if (ap_info) {
                        esp_wifi_scan_get_ap_records(&ap_count, ap_info);
                        
                        // Check each scanned network against known networks
                        for (int i = 0; i < NUM_KNOWN_NETWORKS; i++) {
                            if (s_known_networks[i].ssid[0] == '\0') break;  // End marker
                            
                            for (int j = 0; j < ap_count; j++) {
                                if (strcmp((char*)ap_info[j].ssid, s_known_networks[i].ssid) == 0) {
                                    ESP_LOGI(TAG, "Found known network: %s (RSSI: %d)", 
                                             s_known_networks[i].ssid, ap_info[j].rssi);
                                    
                                    // Try to connect
                                    wifi_save_credentials(s_known_networks[i].ssid, s_known_networks[i].password);
                                    wifi_connect(s_known_networks[i].ssid, s_known_networks[i].password);
                                    
                                    if (wifi_wait_connected(20000) == ESP_OK) {
                                        ESP_LOGI(TAG, "Auto-connected to: %s, IP: %s", 
                                                 s_known_networks[i].ssid, wifi_get_ip_str());
                                        
                                        // Update OLED
                                        if (s_oled) {
                                            s_oled->Clear();
                                            s_oled->DrawRect(0, 0, 128, 64, 1);
                                            s_oled->DrawText(8, 6, "P8 Voice Asst", 1);
                                            s_oled->DrawText(8, 22, "WiFi CONNECTED!", 1);
                                            s_oled->DrawText(8, 34, s_known_networks[i].ssid, 1);
                                            s_oled->DrawText(8, 46, wifi_get_ip_str(), 1);
                                            s_oled->Display();
                                        }
                                        
                                        // Auto-connect WebSocket if URL was set
                                        if (s_ws_url[0] != 0) {
                                            vTaskDelay(pdMS_TO_TICKS(1000));
                                            ws_connect(s_ws_url);
                                        }
                                        
                                        free(ap_info);
                                        vTaskDelete(NULL);
                                        return;
                                    } else {
                                        ESP_LOGW(TAG, "Failed to connect to: %s", s_known_networks[i].ssid);
                                    }
                                    break;
                                }
                            }
                        }
                        free(ap_info);
                    }
                }
            }
        }
        
        // Wait before next scan (30 seconds)
        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}

static void load_ws_url(void) {
    nvs_handle_t nvs;
    if (nvs_open("ws_config", NVS_READONLY, &nvs) == ESP_OK) {
        size_t len = sizeof(s_ws_url);
        if (nvs_get_str(nvs, "ws_url", s_ws_url, &len) == ESP_OK) {
            ESP_LOGI(TAG, "Loaded stored WS URL: %s", s_ws_url);
        }
        nvs_close(nvs);
    }
}

static void save_ws_url(const char* url) {
    nvs_handle_t nvs;
    if (nvs_open("ws_config", NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_set_str(nvs, "ws_url", url);
        nvs_commit(nvs);
        nvs_close(nvs);
        ESP_LOGI(TAG, "Stored WS URL in NVS: %s", url);
    }
}

static void on_wake_detected(const char* wake_word, float confidence);

// ===== Console Task =====
static void ConsoleTask(void* arg) {
    char line[128];
    ESP_LOGI(TAG, "=== Commands: wifi add <ssid> <pass>, ws <url>, status, volume <n>, wake, reset ===");
    
    printf("\n> ");
    fflush(stdout);

    while (true) {
        if (fgets(line, sizeof(line), stdin) == NULL) { vTaskDelay(pdMS_TO_TICKS(50)); continue; }
        line[strcspn(line, "\r\n")] = 0;
        
        if (line[0] == '\0') {
            printf("\n> ");
            fflush(stdout);
            continue;
        }
        
        // WiFi add/connect command - add network to known list (for current session)
        if (strncmp(line, "wifi add ", 9) == 0 || strncmp(line, "wifi connect ", 13) == 0) {
            char ssid[65] = {0}, pass[65] = {0};
            const char* params = (strncmp(line, "wifi add ", 9) == 0) ? (line + 9) : (line + 13);
            
            // Parse with or without quotes to support spaces in SSID
            bool parsed = false;
            if (params[0] == '"') {
                // Format: "SSID with spaces" password
                const char* end_quote = strchr(params + 1, '"');
                if (end_quote) {
                    int ssid_len = end_quote - (params + 1);
                    if (ssid_len < sizeof(ssid)) {
                        strncpy(ssid, params + 1, ssid_len);
                        // Skip quote and space to get password
                        const char* pass_ptr = end_quote + 1;
                        while (*pass_ptr == ' ') pass_ptr++;
                        if (*pass_ptr) {
                            strncpy(pass, pass_ptr, sizeof(pass) - 1);
                            // Remove trailing quotes from password if any
                            if (pass[0] == '"' && pass[strlen(pass)-1] == '"') {
                                pass[strlen(pass)-1] = '\0';
                                memmove(pass, pass + 1, strlen(pass));
                            }
                            parsed = true;
                        }
                    }
                }
            } else {
                // Format: SSID password (no spaces in SSID)
                if (sscanf(params, "%64s %64s", ssid, pass) == 2) {
                    parsed = true;
                }
            }
            
            if (parsed) {
                // Connect immediately
                ESP_LOGI(TAG, "WiFi connecting to: '%s' with pass: '%s'", ssid, pass);
                wifi_save_credentials(ssid, pass);
                wifi_connect(ssid, pass);
                if (wifi_wait_connected(30000) == ESP_OK) {
                    ESP_LOGI(TAG, "WiFi connected: %s", wifi_get_ip_str());
                } else {
                    ESP_LOGE(TAG, "WiFi connection failed!");
                }
            } else {
                ESP_LOGI(TAG, "Usage: wifi connect \"<ssid>\" <password> OR wifi connect <ssid> <password>");
            }
        }
        else if (strcmp(line, "wifi status") == 0) {
            ESP_LOGI(TAG, "WiFi: state=%d, IP=%s", 
             wifi_get_state(), wifi_get_ip_str());
            if (wifi_is_connected()) {
                ESP_LOGI(TAG, "WiFi RSSI: %d dBm", wifi_get_rssi());
            }
        }
        else if (strncmp(line, "ws ", 3) == 0) {
            strncpy(s_ws_url, line + 3, sizeof(s_ws_url) - 1);
            ESP_LOGI(TAG, "WS URL set: %s", s_ws_url);
            save_ws_url(s_ws_url);
            if (wifi_is_connected()) {
                ws_connect(s_ws_url);
            } else {
                ESP_LOGW(TAG, "WiFi not connected!");
            }
        }
        else if (strcmp(line, "ws disconnect") == 0) {
            ws_disconnect();
        }
        else if (strcmp(line, "status") == 0) {
            ESP_LOGI(TAG, "VA: %s | WiFi: %s | WS: %s | Heap: %d",
                     va_state_str(s_va_state),
                     wifi_is_connected() ? wifi_get_ip_str() : "OFF",
                     ws_is_connected() ? "connected" : "disconnected",
                     esp_get_free_heap_size());
        }
        else if (strcmp(line, "wake") == 0) {
            ESP_LOGI(TAG, "Manual wake triggered");
            s_va_state = VA_STATE_READY;
            on_wake_detected("manual_wake", 1.0f);
        }
        else if (strncmp(line, "volume ", 7) == 0) {
            int vol = atoi(line + 7);
            if (vol < 0) vol = 0;
            if (vol > 100) vol = 100;
            s_codec->SetOutputVolume(vol);
            player_set_volume(vol);
            ESP_LOGI(TAG, "Volume: %d%%", vol);
        }
        else if (strcmp(line, "reset") == 0) {
            ESP_LOGI(TAG, "Resetting VA state");
            s_va_state = VA_STATE_READY;
            s_streaming_audio = false;
            s_speaking = false;
        }
        else if (strcmp(line, "help") == 0) {
            ESP_LOGI(TAG, "Commands:");
            ESP_LOGI(TAG, "  wifi add <ssid> <pass> - Connect WiFi");
            ESP_LOGI(TAG, "  wifi status            - WiFi status");
            ESP_LOGI(TAG, "  ws <url>              - Connect WebSocket");
            ESP_LOGI(TAG, "  ws disconnect          - Disconnect WS");
            ESP_LOGI(TAG, "  status                - All status");
            ESP_LOGI(TAG, "  volume <0-100>        - Volume");
            ESP_LOGI(TAG, "  wake                  - Manual wake");
            ESP_LOGI(TAG, "  reset                 - Reset state");
        }
        printf("\n> ");
        fflush(stdout);
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

static char s_asr_text[128] = "";

static void extract_json_value(const char* json, const char* key, char* dest, size_t dest_len) {
    dest[0] = '\0';
    const char* key_pos = strstr(json, key);
    if (!key_pos) return;
    
    // Find the colon after key
    const char* colon_pos = strchr(key_pos, ':');
    if (!colon_pos) return;
    
    // Find the opening quote of the value
    const char* val_start = strchr(colon_pos, '"');
    if (!val_start) return;
    val_start++; // Skip the quote
    
    // Find the closing quote
    const char* val_end = strchr(val_start, '"');
    if (!val_end) return;
    
    size_t len = val_end - val_start;
    if (len >= dest_len) len = dest_len - 1;
    
    memcpy(dest, val_start, len);
    dest[len] = '\0';
}

// ===== OLED Display Task =====
static void DisplayTask(void* arg) {
    char line[32];
    int dots = 0;
    
    while (true) {
        // Clear
        s_oled->Clear();
        s_oled->DrawRect(0, 0, 128, 64, 1);
        
        // Title
        s_oled->DrawText(2, 2, "P8 Voice Asst", 1);
        
        // State
        snprintf(line, sizeof(line), "State: %s", va_state_str(s_va_state));
        s_oled->DrawText(2, 14, line, 1);
        
        // If ASR text is available, display it; otherwise, display status info
        if (s_asr_text[0] != '\0') {
            s_oled->DrawText(2, 26, "You said:", 1);
            
            char line1[22] = "";
            char line2[22] = "";
            size_t text_len = strlen(s_asr_text);
            if (text_len <= 20) {
                snprintf(line1, sizeof(line1), "%s", s_asr_text);
            } else {
                snprintf(line1, sizeof(line1), "%.20s", s_asr_text);
                snprintf(line2, sizeof(line2), "%.20s", s_asr_text + 20);
            }
            s_oled->DrawText(2, 36, line1, 1);
            if (line2[0]) {
                s_oled->DrawText(2, 46, line2, 1);
            }
        } else {
            // WiFi status
            if (wifi_is_connected()) {
                s_oled->DrawText(2, 26, "WiFi: ON", 1);
                s_oled->DrawText(2, 36, wifi_get_ip_str(), 1);
            } else {
                s_oled->DrawText(2, 26, "WiFi: SCANNING...", 1);
            }
            
            // WS status
            if (s_ws_display_connected) {
                s_oled->DrawText(2, 46, "WS: CONNECTED", 1);
            } else {
                s_oled->DrawText(2, 46, "WS: DISCONNECT", 1);
            }
        }
        
        // Animation dots for LISTENING/THINKING
        if (s_va_state == VA_STATE_LISTENING || s_va_state == VA_STATE_THINKING) {
            dots = (dots + 1) % 4;
            for (int i = 0; i < dots; i++) {
                s_oled->SetPixel(100 + i * 6, 14, 1);
            }
        }
        
        s_oled->Display();
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

// ===== Mic Capture Task - Stream audio to WebSocket =====
static void MicCaptureTask(void* arg) {
    ESP_LOGI(TAG, "Mic capture task started");
    
    constexpr int CHUNK_SIZE = 480;  // ~20ms @ 24kHz
    int16_t samples[CHUNK_SIZE];
    
    int64_t start_time = esp_timer_get_time() / 1000;
    
    while (s_streaming_audio) {
        // Push-to-talk: stop capturing if button is released after 300ms debounce window
        if (esp_timer_get_time() / 1000 - start_time > 300) {
            if (gpio_get_level((gpio_num_t)BOOT_BUTTON_GPIO) == 0) {
                ESP_LOGI(TAG, "Button released, stopping capture");
                s_streaming_audio = false;
                s_va_state = VA_STATE_THINKING;
                PlayBeep(s_codec, 1200, 80, 6000);
                ws_send_text("{\"state\":\"stop_capture\"}");
                break;
            }
        }
        
        int n = s_codec->ReadSamples(samples, CHUNK_SIZE);
        if (n > 0 && ws_is_connected()) {
            if (ws_send_audio(samples, n) != ESP_OK) {
                ESP_LOGE(TAG, "Audio send failed, stopping mic capture");
                s_streaming_audio = false;
                break;
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(5));
        }
    }
    
    s_codec->EnableInput(false);
    ESP_LOGI(TAG, "Mic capture task stopped");
    s_mic_task = NULL;
    vTaskDelete(NULL);
}

// ===== Audio Playback Callback (for stream_player) =====
static int playback_write(void* codec, int16_t* samples, int count) {
    Esp32S3AudioCodec* c = (Esp32S3AudioCodec*)codec;
    c->WriteSamples(samples, count);
    return count;
}

// ===== Wake Word Callback =====
static void on_wake_detected(const char* wake_word, float confidence) {
    ESP_LOGI(TAG, "Wake detected: %s (%.2f)", wake_word, confidence);
    
    if (ws_is_connected()) {
        // If already recording, do nothing
        if (s_mic_task != NULL) {
            ESP_LOGI(TAG, "Mic task already running, ignoring wake");
            return;
        }
        
        // If speaking, interrupt and stop playback
        if (s_speaking || s_va_state == VA_STATE_SPEAKING) {
            ESP_LOGI(TAG, "Interrupting playback for new wake");
            s_speaking = false;
            player_stop();
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        
        s_va_state = VA_STATE_LISTENING;
        s_streaming_audio = true;
        s_speech_start_time = esp_timer_get_time() / 1000;
        
        // Play listening beep
        PlayBeep(s_codec, 1000, 100, 6000);
        
        // Start mic capture
        s_codec->EnableInput(true);
        if (xTaskCreatePinnedToCore(MicCaptureTask, "mic", 4096, NULL, 6, &s_mic_task, 1) != pdPASS) {
            ESP_LOGE(TAG, "Failed to start mic task");
            s_codec->EnableInput(false);
            s_streaming_audio = false;
            s_va_state = VA_STATE_ERROR;
        }
    } else {
        ESP_LOGW(TAG, "WS not connected, ignoring wake");
    }
}

// ===== WebSocket Text Callback =====
static void on_ws_text(const char* text) {
    ESP_LOGI(TAG, "WS Text: %s", text);
    
    // Parse JSON-like responses from server
    if (strstr(text, "\"action\":\"startSpeaking\"") || strstr(text, "\"action\":\"speak\"")) {
        // Server signals start of speech
        s_streaming_audio = false;
        s_speaking = true;
        s_va_state = VA_STATE_SPEAKING;
        
        // Stop mic if still running
        if (s_mic_task) {
            s_streaming_audio = false;
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        
        // Play beep
        PlayBeep(s_codec, 880, 80, 6000);
    }
    else if (strstr(text, "\"action\":\"stopSpeaking\"") || strstr(text, "\"action\":\"stop\"")) {
        // Server signals end of speech
        s_speaking = false;
        s_va_state = VA_STATE_READY;
        player_stop();
        
        // Play completion beep
        PlayBeep(s_codec, 1320, 80, 6000);
    }
    else if (strstr(text, "\"action\":\"listening\"")) {
        // Server ready for audio - clear previous text
        s_asr_text[0] = '\0';
        s_streaming_audio = true;
        s_va_state = VA_STATE_LISTENING;
    }
    else if (strstr(text, "\"action\":\"thinking\"")) {
        // Server processing
        s_streaming_audio = false;
        s_va_state = VA_STATE_THINKING;
    }
    else if (strstr(text, "\"action\":\"error\"")) {
        ESP_LOGW(TAG, "Server error action: %s", text);
        s_va_state = VA_STATE_ERROR;
        player_stop();
        vTaskDelay(pdMS_TO_TICKS(2000));
        s_va_state = VA_STATE_READY;
    }
    else if (strstr(text, "\"type\":\"asr_result\"")) {
        ESP_LOGI(TAG, "ASR result: %s", text);
        extract_json_value(text, "\"text\"", s_asr_text, sizeof(s_asr_text));
        if (s_va_state == VA_STATE_LISTENING) {
            s_streaming_audio = false;
            s_va_state = VA_STATE_THINKING;
        }
    }
    else if (strstr(text, "\"type\":\"llm_response\"")) {
        ESP_LOGI(TAG, "LLM result: %s", text);
        if (s_va_state == VA_STATE_THINKING) {
            s_va_state = VA_STATE_READY;
        }
    }
    else if (strncmp(text, "http://", 7) == 0 || strncmp(text, "https://", 8) == 0) {
        // Direct audio URL - play it
        ESP_LOGI(TAG, "Playing audio: %s", text);
        player_play(text);
    }
}

// ===== WebSocket Binary Callback (TTS audio) =====
static void on_ws_binary(const uint8_t* data, size_t len, ws_msg_type_t type) {
    if (type == WS_TYPE_BINARY && len > 0) {
        ESP_LOGD(TAG, "Binary data: %d bytes", len);
        player_write_audio(data, len);
    }
}

// ===== WebSocket Connected Callback =====
static void on_ws_connected(void) {
    ESP_LOGI(TAG, "WebSocket connected!");
    s_ws_display_connected = true;  // Update display state
    
    // Play connection beep
    PlayBeep(s_codec, 880, 100, 8000);
    vTaskDelay(pdMS_TO_TICKS(80));
    PlayBeep(s_codec, 1320, 100, 8000);
    
    if (s_va_state == VA_STATE_IDLE) {
        s_va_state = VA_STATE_READY;
    }
}

// ===== WebSocket Disconnected Callback =====
static void on_ws_disconnected(void) {
    ESP_LOGI(TAG, "WebSocket disconnected");
    s_ws_display_connected = false;  // Update display state
    
    s_streaming_audio = false;
    s_speaking = false;
    
    if (s_va_state != VA_STATE_IDLE) {
        s_va_state = VA_STATE_ERROR;
    }
}

// ===== WebSocket Error Callback =====
static void on_ws_error(const char* error) {
    ESP_LOGE(TAG, "WebSocket error: %s", error);
    s_streaming_audio = false;
    s_speaking = false;
    s_va_state = VA_STATE_ERROR;
}

// ===== Main =====
extern "C" void app_main(void) {
    esp_rom_printf("[BOOT] P8 Voice Assistant booting...\r\n");
    
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) { 
        nvs_flash_erase(); 
        nvs_flash_init(); 
    }
    
    // Load stored WebSocket URL from NVS
    load_ws_url();
    
    ESP_LOGI(TAG, "=== P8 Voice Assistant ===");
    
    // Initialize board (I2C, I2S)
    static Esp32S3Wroom1Board board;
    s_board = &board;
    board.Initialize();
    
    // Initialize OLED
    static Ssd1306 oled(board.GetI2CBus());
    s_oled = &oled;
    oled.Init();
    oled.Clear();
    oled.DrawRect(0, 0, 128, 64, 1);
    oled.DrawText(8, 6, "P8 Voice Asst", 1);
    oled.DrawText(8, 22, "Initializing...", 1);
    oled.Display();
    
    // Initialize audio codec
    static Esp32S3AudioCodec codec(board.GetSpeakerHandle(), board.GetMicHandle(), 24000, 24000);
    s_codec = &codec;
    codec.SetOutputVolume(70);
    codec.EnableInput(false);
    codec.EnableOutput(true);
    codec.Start();
    
    // Play startup beep
    PlayBeep(&codec, 880, 150, 8000);
    vTaskDelay(pdMS_TO_TICKS(130));
    PlayBeep(&codec, 1320, 150, 8000);
    
    // Initialize WiFi
    wifi_init();
    
    // Register WiFi connected callback to auto-reconnect WebSocket
    wifi_on_connected([]() {
        ESP_LOGI(TAG, "WiFi reconnected, checking for WebSocket auto-connect...");
        if (s_ws_url[0] != '\0') {
            ESP_LOGI(TAG, "Auto-reconnecting WebSocket to: %s", s_ws_url);
            ws_connect(s_ws_url);
        }
    });
    
    // Try stored credentials first
    if (wifi_connect_stored() == ESP_OK && wifi_wait_connected(10000) == ESP_OK) {
        ESP_LOGI(TAG, "WiFi (stored): %s", wifi_get_ip_str());
        oled.DrawText(8, 22, "WiFi (stored) OK!", 1);
        oled.Display();
    } else {
        ESP_LOGI(TAG, "WiFi not connected - starting auto-scan...");
        oled.DrawText(8, 22, "WiFi: Auto-Scan...", 1);
        oled.Display();
    }
    
    // Start WiFi auto-connect task (scans and auto-connects to known networks)
    xTaskCreatePinnedToCore(WifiAutoConnectTask, "wifi_auto", 4096, NULL, 2, NULL, 0);
    
    // Initialize wake word (button wake on GPIO47)
    ww_init(0);
    ww_on_detected(on_wake_detected);
    ww_start();
    
    // Initialize WebSocket
    ws_init();
    ws_on_connected(on_ws_connected);
    ws_on_disconnected(on_ws_disconnected);
    ws_on_error(on_ws_error);
    ws_on_text(on_ws_text);
    ws_on_data(on_ws_binary);
    
    // Initialize audio player
    player_init(&codec);
    player_set_volume(70);
    
    // Start display task
    xTaskCreatePinnedToCore(DisplayTask, "display", 4096, NULL, 2, &s_display_task, 0);
    
    // Start console task
    xTaskCreatePinnedToCore(ConsoleTask, "console", 4096, NULL, 3, NULL, 0);
    
    // Initial state
    s_va_state = VA_STATE_READY;
    
    ESP_LOGI(TAG, "=== Ready! Press button on GPIO47 ===");
    ESP_LOGI(TAG, "Auto-scanning for known networks: Lan-mini, Thanh long-2.4G-ext, etc.");
    
    // Update OLED
    oled.Clear();
    oled.DrawRect(0, 0, 128, 64, 1);
    oled.DrawText(8, 6, "P8 Voice Asst", 1);
    oled.DrawText(8, 22, "READY", 1);
    oled.DrawText(8, 34, "Press button", 1);
    oled.DrawText(8, 46, "GPIO47", 1);
    oled.Display();
}
