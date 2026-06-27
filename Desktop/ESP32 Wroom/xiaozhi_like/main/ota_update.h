// ota_update.h
// P4.1: OTA Update module using esp_ota_ops
// Supports: HTTP/HTTPS OTA update from URL

#pragma once

#include <esp_err.h>
#include <esp_ota_ops.h>
#include <esp_http_client.h>

#ifdef __cplusplus
extern "C" {
#endif

// OTA update handle
typedef struct ota_handle* ota_handle_t;

// OTA state
typedef enum {
    OTA_STATE_IDLE = 0,
    OTA_STATE_DOWNLOADING,
    OTA_STATE_VERIFYING,
    OTA_STATE_APPLYING,
    OTA_STATE_REBOOTING,
    OTA_STATE_FAILED
} ota_state_t;

// OTA progress callback
typedef void (*ota_progress_cb_t)(int progress_percent, size_t downloaded, size_t total);

// Initialize OTA module
esp_err_t ota_init(void);

// Start OTA update from URL
// url: HTTP/HTTPS URL to firmware binary
// progress_cb: optional callback for progress updates
esp_err_t ota_start_update(const char* url, ota_progress_cb_t progress_cb);

// Check if new firmware is pending (after reboot)
bool ota_is_pending(void);

// Get current running partition info
const esp_partition_t* ota_get_running_partition(void);

// Get next boot partition (OTA target)
const esp_partition_t* ota_get_update_partition(void);

// Mark current boot as valid (call after successful boot)
esp_err_t ota_mark_boot_success(void);

// Abort ongoing OTA
esp_err_t ota_abort(void);

// Get OTA state
ota_state_t ota_get_state(void);

// Get last error message
const char* ota_get_error_msg(void);

#ifdef __cplusplus
}
#endif