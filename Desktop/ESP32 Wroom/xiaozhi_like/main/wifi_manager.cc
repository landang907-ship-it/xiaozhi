// wifi_manager.cc - P4.1: WiFi Manager with Auto-Scan
#include "wifi_manager.h"
#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include <esp_netif_types.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char* TAG = "WiFi";
static const char* NVS_NS = "wifi_creds";
static wifi_state_t s_state = WIFI_STATE_IDLE;
static char s_ip_str[16] = {0};
static EventGroupHandle_t s_wifi_event_group = NULL;
static int s_retry_count = 0;
static int s_max_retry = 5;  // Reduced retries for faster fallback

// WiFi event bits
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

// Known WiFi networks (SSID, Password)
typedef struct {
    const char* ssid;
    const char* password;
} known_wifi_t;

static const known_wifi_t s_known_networks[] = {
    {"Thanh", "long-2.4G-ext"},
    {"Lan-mini", "123456788"},
};

// Callback for WiFi connected event
static wifi_connected_cb_t s_connected_cb = NULL;

void wifi_on_connected(wifi_connected_cb_t cb) {
    s_connected_cb = cb;
}

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
        
        // Call the connected callback if registered
        if (s_connected_cb) {
            ESP_LOGI(TAG, "Calling WiFi connected callback");
            s_connected_cb();
        }
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

// Scan for available networks and return count
static int wifi_scan_available(char found_ssids[][33], int max_count) {
    wifi_scan_config_t scan_config = {};
    scan_config.scan_type = WIFI_SCAN_TYPE_PASSIVE;
    scan_config.scan_time.passive = 300;
    
    ESP_LOGI(TAG, "Scanning for WiFi networks...");
    esp_err_t err = esp_wifi_scan_start(&scan_config, true);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Scan failed: %s", esp_err_to_name(err));
        return 0;
    }
    
    wifi_ap_record_t* ap_records = (wifi_ap_record_t*)malloc(sizeof(wifi_ap_record_t) * 20);
    if (!ap_records) return 0;
    uint16_t ap_count = 20;
    
    err = esp_wifi_scan_get_ap_records(&ap_count, ap_records);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Get AP list failed");
        free(ap_records);
        return 0;
    }
    
    int found = 0;
    for (int i = 0; i < ap_count && found < max_count; i++) {
        // Check if SSID is not empty
        if (ap_records[i].ssid[0] != '\0') {
            bool already_found = false;
            for (int j = 0; j < found; j++) {
                if (strcmp(found_ssids[j], (char*)ap_records[i].ssid) == 0) {
                    already_found = true;
                    break;
                }
            }
            if (!already_found) {
                strncpy(found_ssids[found], (char*)ap_records[i].ssid, 32);
                found_ssids[found][32] = '\0';
                ESP_LOGI(TAG, "Found: %s (rssi: %d)", found_ssids[found], ap_records[i].rssi);
                found++;
            }
        }
    }
    
    free(ap_records);
    ESP_LOGI(TAG, "Scan complete: %d networks found", found);
    return found;
}

// Try to connect to a specific SSID with timeout
// Returns: ESP_OK if connected, ESP_FAIL if failed
static esp_err_t wifi_try_connect(const char* ssid, const char* password, int timeout_ms) {
    ESP_LOGI(TAG, "Attempting: %s", ssid);
    
    if (wifi_connect(ssid, password) != ESP_OK) {
        return ESP_FAIL;
    }
    
    esp_err_t err = wifi_wait_connected(timeout_ms);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Connected to %s!", ssid);
        return ESP_OK;
    }
    
    ESP_LOGW(TAG, "Failed to connect to %s", ssid);
    wifi_disconnect();
    vTaskDelay(pdMS_TO_TICKS(100));  // Brief delay before next attempt
    return ESP_FAIL;
}

esp_err_t wifi_connect_auto(void) {
    char available_ssids[20][33];
    char stored_ssid[33] = {0};
    char stored_pass[65] = {0};
    
    ESP_LOGI(TAG, "=== Auto WiFi Connect ===");
    
    // First, try stored credentials
    if (wifi_load_credentials(stored_ssid, sizeof(stored_ssid), 
                               stored_pass, sizeof(stored_pass)) == ESP_OK) {
        ESP_LOGI(TAG, "Trying stored network: %s", stored_ssid);
        if (wifi_try_connect(stored_ssid, stored_pass, 15000) == ESP_OK) {
            return ESP_OK;
        }
    }
    
    // Scan for available networks
    int scan_count = wifi_scan_available(available_ssids, 20);
    if (scan_count == 0) {
        ESP_LOGW(TAG, "No networks found in scan");
    }
    
    // Build known networks list (stored + hardcoded)
    const char* known_ssids[10];
    const char* known_pass[10];
    int known_count = 0;
    
    // Add stored network first if exists
    if (stored_ssid[0] != '\0' && stored_pass[0] != '\0') {
        known_ssids[known_count] = stored_ssid;
        known_pass[known_count] = stored_pass;
        known_count++;
    }
    
    // Add hardcoded networks
    for (int i = 0; i < 2; i++) {
        if (s_known_networks[i].ssid[0] != '\0') {
            known_ssids[known_count] = s_known_networks[i].ssid;
            known_pass[known_count] = s_known_networks[i].password;
            known_count++;
        }
    }
    
    // Try known networks that are available
    ESP_LOGI(TAG, "Trying %d known networks against %d available...", known_count, scan_count);
    
    for (int k = 0; k < known_count; k++) {
        for (int s = 0; s < scan_count; s++) {
            if (strcmp(known_ssids[k], available_ssids[s]) == 0) {
                ESP_LOGI(TAG, "Found known network in scan: %s", known_ssids[k]);
                
                if (wifi_try_connect(known_ssids[k], known_pass[k], 15000) == ESP_OK) {
                    // Save successful connection
                    wifi_save_credentials(known_ssids[k], known_pass[k]);
                    return ESP_OK;
                }
                break;  // Move to next known network
            }
        }
    }
    
    // If nothing matched, try all available networks with common passwords
    ESP_LOGI(TAG, "Trying all available networks with common passwords...");
    const char* common_passwords[] = {"", "12345678", "123456789", "password", "admin123"};
    
    for (int s = 0; s < scan_count; s++) {
        for (int p = 0; p < 5; p++) {
            if (wifi_try_connect(available_ssids[s], common_passwords[p], 15000) == ESP_OK) {
                // Try to save if it worked (might fail for open networks)
                ESP_LOGI(TAG, "Connected to %s!", available_ssids[s]);
                return ESP_OK;
            }
        }
    }
    
    ESP_LOGE(TAG, "Failed to connect to any network");
    return ESP_FAIL;
}

esp_err_t wifi_connect_stored(void) {
    // Use new auto-connect logic
    return wifi_connect_auto();
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
