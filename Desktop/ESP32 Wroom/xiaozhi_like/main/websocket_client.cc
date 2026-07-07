// websocket_client.cc - P5.1: WebSocket Client
#include "websocket_client.h"
#include <esp_log.h>
#include <esp_websocket_client.h>
#include <esp_crt_bundle.h>
#include <string.h>

static const char* TAG = "WS";

static esp_websocket_client_handle_t s_client = NULL;
static ws_state_t s_state = WS_STATE_IDLE;
static bool s_connected = false;

// Callbacks
static ws_connected_cb_t s_conn_cb = NULL;
static ws_disconnected_cb_t s_disc_cb = NULL;
static ws_error_cb_t s_err_cb = NULL;
static ws_data_cb_t s_data_cb = NULL;
static ws_text_cb_t s_text_cb = NULL;

// Ring buffer for outgoing audio
#define WS_SEND_BUF_SIZE (4096)
static uint8_t s_send_buf[WS_SEND_BUF_SIZE];
static size_t s_send_head = 0, s_send_tail = 0;

static size_t buf_avail(void) {
    if (s_send_head >= s_send_tail) return s_send_head - s_send_tail;
    return WS_SEND_BUF_SIZE - s_send_tail + s_send_head;
}

// WebSocket event handler
static void ws_event_handler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data) {
    esp_websocket_event_data_t* data = (esp_websocket_event_data_t*)event_data;
    
    switch (event_id) {
    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "WS Connected");
        s_state = WS_STATE_CONNECTED;
        s_connected = true;
        if (s_conn_cb) s_conn_cb();
        break;
        
    case WEBSOCKET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "WS Disconnected");
        s_state = WS_STATE_DISCONNECTED;
        s_connected = false;
        if (s_disc_cb) s_disc_cb();
        break;
        
    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGW(TAG, "WS Error");
        s_state = WS_STATE_ERROR;
        if (s_err_cb) s_err_cb("Connection error");
        break;
        
    case WEBSOCKET_EVENT_DATA:
        if (data->data_len > 0) {
            // Check opcode
            if (data->op_code == WS_TRANSPORT_OPCODES_TEXT) {
                // Null-terminate for text
                char* text = (char*)malloc(data->data_len + 1);
                if (text) {
                    memcpy(text, data->data_ptr, data->data_len);
                    text[data->data_len] = 0;
                    ESP_LOGD(TAG, "WS Text: %.*s", data->data_len > 64 ? 64 : data->data_len, text);
                    if (s_text_cb) s_text_cb(text);
                    free(text);
                }
            } else {
                // Binary/audio data
                ESP_LOGD(TAG, "WS Binary: %d bytes", data->data_len);
                if (s_data_cb) s_data_cb((const uint8_t*)data->data_ptr, data->data_len, WS_TYPE_BINARY);
            }
        }
        break;
        
    case WEBSOCKET_EVENT_BEGIN:
        break;
        
    case WEBSOCKET_EVENT_FINISH:
        break;
        
    default:
        break;
    }
}

// Public API
esp_err_t ws_init(void) {
    ESP_LOGI(TAG, "WS Client init");
    s_state = WS_STATE_IDLE;
    s_connected = false;
    s_send_head = s_send_tail = 0;
    
    // Attach ESP-IDF certificate bundle for TLS
    esp_err_t err = esp_crt_bundle_attach(NULL);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "esp_crt_bundle_attach: %s (TLS may not work)", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "CA bundle attached for TLS");
    }
    
    return ESP_OK;
}

esp_err_t ws_connect(const char* url) {
    if (s_client) {
        ws_disconnect();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    ESP_LOGI(TAG, "WS connecting to: %s", url);
    s_state = WS_STATE_CONNECTING;
    
    esp_websocket_client_config_t cfg = {};
    cfg.uri = url;
    cfg.reconnect_timeout_ms = 10000;
    cfg.disable_auto_reconnect = false;
    
    // Only attach TLS cert bundle for wss:// (secure WebSocket)
    // For plain ws://, do NOT attach TLS or connection will fail
    bool is_secure = (strncmp(url, "wss://", 6) == 0);
    if (is_secure) {
        cfg.crt_bundle_attach = esp_crt_bundle_attach;
        cfg.skip_cert_common_name_check = true;
        ESP_LOGI(TAG, "TLS: enabled (wss://)");
    } else {
        cfg.crt_bundle_attach = NULL;
        ESP_LOGI(TAG, "TLS: disabled (ws://)");
    }
    
    s_client = esp_websocket_client_init(&cfg);
    if (!s_client) {
        ESP_LOGE(TAG, "Failed to init WS client");
        s_state = WS_STATE_ERROR;
        return ESP_FAIL;
    }
    
    esp_websocket_register_events(s_client, WEBSOCKET_EVENT_ANY, ws_event_handler, NULL);
    
    esp_err_t err = esp_websocket_client_start(s_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "WS start failed: %s", esp_err_to_name(err));
        esp_websocket_client_destroy(s_client);
        s_client = NULL;
        s_state = WS_STATE_ERROR;
        return err;
    }
    
    return ESP_OK;
}

esp_err_t ws_disconnect(void) {
    if (!s_client) return ESP_ERR_INVALID_STATE;
    
    ESP_LOGI(TAG, "WS disconnecting...");
    esp_websocket_client_stop(s_client);
    esp_websocket_client_destroy(s_client);
    s_client = NULL;
    s_state = WS_STATE_IDLE;
    s_connected = false;
    return ESP_OK;
}

esp_err_t ws_send_binary(const uint8_t* data, size_t len) {
    if (!s_client || !s_connected) return ESP_ERR_INVALID_STATE;
    
    int wlen = esp_websocket_client_send_bin(s_client, (const char*)data, len, portMAX_DELAY);
    if (wlen < 0) {
        ESP_LOGE(TAG, "WS send failed");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t ws_send_text(const char* text) {
    if (!s_client || !s_connected) return ESP_ERR_INVALID_STATE;
    if (!text) return ESP_ERR_INVALID_ARG;
    
    int wlen = esp_websocket_client_send_text(s_client, text, strlen(text), portMAX_DELAY);
    if (wlen < 0) {
        ESP_LOGE(TAG, "WS send text failed");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t ws_send_audio(const int16_t* samples, int count) {
    // Convert stereo 16-bit to mono 8-bit and send
    if (!samples || count <= 0) return ESP_ERR_INVALID_ARG;
    
    uint8_t buf[1024];
    int out_count = count < 512 ? count : 512;
    
    for (int i = 0; i < out_count; i++) {
        // Average stereo channels if stereo, then convert to 8-bit
        int32_t s = samples[i];
        // Clip to 16-bit range
        if (s > 32767) s = 32767;
        if (s < -32768) s = -32768;
        // Convert to unsigned 8-bit: 0 = -32768, 128 = 0, 255 = 32767
        uint8_t u8 = (uint8_t)((s >> 8) + 128);
        buf[i] = u8;
    }
    
    return ws_send_binary(buf, out_count);
}

bool ws_is_connected(void) {
    return s_connected && s_client != NULL;
}

ws_state_t ws_get_state(void) {
    return s_state;
}

void ws_on_connected(ws_connected_cb_t cb) { s_conn_cb = cb; }
void ws_on_disconnected(ws_disconnected_cb_t cb) { s_disc_cb = cb; }
void ws_on_error(ws_error_cb_t cb) { s_err_cb = cb; }
void ws_on_data(ws_data_cb_t cb) { s_data_cb = cb; }
void ws_on_text(ws_text_cb_t cb) { s_text_cb = cb; }