// ota_update.cc - P4.1: OTA Update (ESP-IDF 5.5)
#include "ota_update.h"
#include <esp_log.h>
#include <esp_http_client.h>
#include <esp_https_ota.h>
#include <esp_app_format.h>
#include <esp_ota_ops.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

static const char* TAG = "OTA";
static ota_state_t s_state = OTA_STATE_IDLE;
static char s_error_msg[256] = {0};
static TaskHandle_t s_ota_task = NULL;
static ota_progress_cb_t s_progress_cb = NULL;
static int s_total_len = 0, s_progress = 0;

static esp_err_t http_event_handler(esp_http_client_event_t* evt) {
    switch (evt->event_id) {
    case HTTP_EVENT_ON_CONNECTED:
        s_total_len = s_progress = 0;
        break;
    case HTTP_EVENT_ON_HEADER:
        if (strcmp(evt->header_key, "Content-Length") == 0) {
            s_total_len = atoi(evt->header_value);
        }
        break;
    case HTTP_EVENT_ON_DATA:
        if (s_total_len > 0) {
            s_progress += evt->data_len;
            if (s_progress_cb) s_progress_cb((s_progress*100)/s_total_len, s_progress, s_total_len);
        }
        break;
    default: break;
    }
    return ESP_OK;
}

static void ota_task(void* param) {
    char* url = (char*)param;
    ESP_LOGI(TAG, "OTA from: %s", url);
    s_state = OTA_STATE_DOWNLOADING;
    
    esp_http_client_config_t http_cfg = {};
    http_cfg.url = url;
    http_cfg.event_handler = http_event_handler;
    http_cfg.timeout_ms = 30000;
    if (strncmp(url, "https://", 8) == 0) http_cfg.skip_cert_common_name_check = true;
    
    esp_https_ota_config_t ota_cfg = {};
    ota_cfg.http_config = &http_cfg;
    s_state = OTA_STATE_VERIFYING;
    
    esp_https_ota_handle_t handle = NULL;
    esp_err_t err = esp_https_ota_begin(&ota_cfg, &handle);
    if (err != ESP_OK) { 
        snprintf(s_error_msg,256,"Begin failed: %s",esp_err_to_name(err)); 
        s_state = OTA_STATE_FAILED;
        free(url);
        vTaskDelete(NULL);
        return;
    }
    
    const esp_app_desc_t* desc = esp_ota_get_app_description();
    ESP_LOGI(TAG, "New fw: %s", desc->version);
    
    while (1) {
        err = esp_https_ota_perform(handle);
        if (esp_https_ota_is_complete_data_received(handle)) break;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    // ESP-IDF 5.5: Use esp_ota_check_rollback_is_possible instead of validate_firmware_slot
    if (esp_ota_check_rollback_is_possible() != ESP_OK) {
        strcpy(s_error_msg, "Validation failed");
        esp_https_ota_abort(handle);
        s_state = OTA_STATE_FAILED;
        free(url);
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "Validated - rebooting...");
    esp_https_ota_abort(handle);
    s_state = OTA_STATE_REBOOTING;
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();
    
    // Should not reach here
    s_state = OTA_STATE_FAILED;
    free(url);
    vTaskDelete(NULL);
}

// Public API
esp_err_t ota_init(void) {
    ESP_LOGI(TAG, "OTA init");
    s_state = OTA_STATE_IDLE;
    return ESP_OK;
}

esp_err_t ota_start_update(const char* url, ota_progress_cb_t cb) {
    if (s_state != OTA_STATE_IDLE && s_state != OTA_STATE_FAILED) return ESP_ERR_INVALID_STATE;
    if (!url || !url[0]) return ESP_ERR_INVALID_ARG;
    s_progress_cb = cb;
    char* u = strdup(url);
    if (!u) return ESP_ERR_NO_MEM;
    if (xTaskCreatePinnedToCore(ota_task, "ota", 8192, u, 5, &s_ota_task, 0) != pdPASS) { free(u); return ESP_FAIL; }
    return ESP_OK;
}

bool ota_is_pending(void) {
    esp_ota_img_states_t st;
    return esp_ota_get_state_partition(esp_ota_get_running_partition(), &st) == ESP_OK && st == ESP_OTA_IMG_PENDING_VERIFY;
}

const esp_partition_t* ota_get_running_partition(void) { return esp_ota_get_running_partition(); }
const esp_partition_t* ota_get_update_partition(void) { return esp_ota_get_next_update_partition(NULL); }

esp_err_t ota_mark_boot_success(void) {
    return esp_ota_mark_app_valid_cancel_rollback();
}

esp_err_t ota_abort(void) {
    if (s_ota_task) { vTaskDelete(s_ota_task); s_ota_task = NULL; }
    s_state = OTA_STATE_IDLE;
    return ESP_OK;
}

ota_state_t ota_get_state(void) { return s_state; }
const char* ota_get_error_msg(void) { return s_error_msg; }