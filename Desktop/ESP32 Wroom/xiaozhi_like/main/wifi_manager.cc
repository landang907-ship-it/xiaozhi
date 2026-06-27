// wifi_manager.cc - P4.1: WiFi Manager
#include "wifi_manager.h"
#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include <esp_netif_types.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <string.h>

static const char* TAG = "WiFi";
static const char* NVS_NS = "wifi_creds";
static wifi_state_t s_state = WIFI_STATE_IDLE;
static char s_ip_str[16] = {0};
static EventGroupHandle_t s_wifi_event_group = NULL;
static int s_retry_count = 0;
static int s_max_retry = 10;

// WiFi event bits
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_count < s_max_retry) {
            esp_wifi_connect();
            s_retry_count++;
            ESP_LOGI(TAG, "Retry %d/%d", s_retry_count, s_max_retry);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            s_state = WIFI_STATE_FAILED;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
        esp_ip4addr_ntoa(&event->ip_info.ip, s_ip_str, sizeof(s_ip_str));
        ESP_LOGI(TAG, "Got IP: %s", s_ip_str);
        s_retry_count = 0;
        s_state = WIFI_STATE_CONNECTED;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

esp_err_t wifi_init(void) {
    if (s_wifi_event_group == NULL) {
        s_wifi_event_group = xEventGroupCreate();
    }
    
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    esp_event_handler_instance_t instance_any_id, instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
        &wifi_event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
        &wifi_event_handler, NULL, &instance_got_ip));
    
    s_state = WIFI_STATE_IDLE;
    ESP_LOGI(TAG, "WiFi initialized");
    return ESP_OK;
}

esp_err_t wifi_connect(const char* ssid, const char* password) {
    if (!ssid || !password) return ESP_ERR_INVALID_ARG;
    
    s_state = WIFI_STATE_CONNECTING;
    s_retry_count = 0;
    
    wifi_config_t wifi_config = {};
    strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "WiFi connect started to %s", ssid);
    return ESP_OK;
}

esp_err_t wifi_connect_stored(void) {
    char ssid[33] = {0}, pass[65] = {0};
    esp_err_t err = wifi_load_credentials(ssid, sizeof(ssid), pass, sizeof(pass));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "No stored credentials");
        return err;
    }
    return wifi_connect(ssid, pass);
}

esp_err_t wifi_disconnect(void) {
    s_state = WIFI_STATE_DISCONNECTED;
    return esp_wifi_disconnect();
}

bool wifi_is_connected(void) {
    return s_state == WIFI_STATE_CONNECTED;
}

wifi_state_t wifi_get_state(void) {
    return s_state;
}

char* wifi_get_ip_str(void) {
    return s_ip_str;
}

esp_err_t wifi_save_credentials(const char* ssid, const char* password) {
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &nvs);
    if (err != ESP_OK) return err;
    
    err = nvs_set_str(nvs, "ssid", ssid);
    if (err == ESP_OK) err = nvs_set_str(nvs, "password", password);
    if (err == ESP_OK) err = nvs_commit(nvs);
    nvs_close(nvs);
    
    ESP_LOGI(TAG, "Credentials saved");
    return err;
}

esp_err_t wifi_load_credentials(char* ssid_out, size_t ssid_len, char* pass_out, size_t pass_len) {
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NS, NVS_READONLY, &nvs);
    if (err != ESP_OK) return err;
    
    err = nvs_get_str(nvs, "ssid", ssid_out, &ssid_len);
    if (err == ESP_OK) {
        size_t pass_len_tmp = pass_len;
        err = nvs_get_str(nvs, "password", pass_out, &pass_len_tmp);
    }
    nvs_close(nvs);
    return err;
}

esp_err_t wifi_clear_credentials(void) {
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &nvs);
    if (err != ESP_OK) return err;
    err = nvs_erase_all(nvs);
    if (err == ESP_OK) err = nvs_commit(nvs);
    nvs_close(nvs);
    return err;
}

esp_err_t wifi_wait_connected(int timeout_ms) {
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, pdMS_TO_TICKS(timeout_ms));
    
    if (bits & WIFI_CONNECTED_BIT) {
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        return ESP_FAIL;
    }
    return ESP_ERR_TIMEOUT;
}

int wifi_get_rssi(void) {
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        return ap_info.rssi;
    }
    return 0;
}