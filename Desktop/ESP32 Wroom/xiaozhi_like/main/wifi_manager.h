// wifi_manager.h
// P4.1: WiFi Manager for OTA updates
// Simple WiFi connection with NVS storage for credentials

#pragma once

#include <esp_err.h>
#include <esp_netif_types.h>
#include <esp_wifi.h>

#ifdef __cplusplus
extern "C" {
#endif

// WiFi connection states
typedef enum {
    WIFI_STATE_IDLE = 0,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_DISCONNECTED,
    WIFI_STATE_FAILED
} wifi_state_t;

// Connection info callback
typedef void (*wifi_connect_cb_t)(wifi_state_t state, void* user_data);

// WiFi connected callback - called when WiFi successfully connects
typedef void (*wifi_connected_cb_t)(void);

// Initialize WiFi subsystem
esp_err_t wifi_init(void);

// Register callback for WiFi connected event
// The callback will be called each time WiFi connects successfully
void wifi_on_connected(wifi_connected_cb_t cb);

// Connect to WiFi network (stored in NVS)
// ssid: WiFi SSID (max 32 chars)
// password: WiFi password (max 64 chars)
esp_err_t wifi_connect(const char* ssid, const char* password);

// Auto-connect: scan available networks, try known credentials
// Tries stored credentials first, then scans and matches known networks
// 15s timeout per connection attempt
esp_err_t wifi_connect_auto(void);

// Connect using stored credentials from NVS (now uses auto-connect)
esp_err_t wifi_connect_stored(void);

// Disconnect from WiFi
esp_err_t wifi_disconnect(void);

// Check if connected
bool wifi_is_connected(void);

// Get current WiFi state
wifi_state_t wifi_get_state(void);

// Get local IP address (valid when connected)
char* wifi_get_ip_str(void);

// Save credentials to NVS
esp_err_t wifi_save_credentials(const char* ssid, const char* password);

// Load credentials from NVS
esp_err_t wifi_load_credentials(char* ssid_out, size_t ssid_len, char* pass_out, size_t pass_len);

// Clear stored credentials
esp_err_t wifi_clear_credentials(void);

// Wait for connection with timeout
// timeout_ms: maximum wait time in milliseconds
// Returns: ESP_OK if connected, ESP_ERR_TIMEOUT if timeout
esp_err_t wifi_wait_connected(int timeout_ms);

// Get signal strength (RSSI)
// Returns: RSSI value or 0 if not connected
int wifi_get_rssi(void);

#ifdef __cplusplus
}
#endif
