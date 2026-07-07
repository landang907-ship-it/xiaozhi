// wake_word.cc - Simple Wake Word Detection for ESP32-S3
// Note: esp_sr (WakeNet) not available on ESP32-S3
// Using button wake instead (GPIO47)
#include "wake_word.h"
#include "boards/esp32s3-wroom-1/config.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

static const char* TAG = "WakeWord";
static ww_state_t s_state = WW_STATE_IDLE;
static ww_detected_cb_t s_callback = NULL;

static SemaphoreHandle_t s_btn_sem = NULL;
static TaskHandle_t s_btn_task = NULL;

static void IRAM_ATTR wake_button_isr_handler(void* arg) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (s_btn_sem) {
        xSemaphoreGiveFromISR(s_btn_sem, &xHigherPriorityTaskWoken);
        if (xHigherPriorityTaskWoken) {
            portYIELD_FROM_ISR();
        }
    }
}

static void wake_button_task(void* arg) {
    ESP_LOGI(TAG, "Wake button task started");
    while (true) {
        if (xSemaphoreTake(s_btn_sem, portMAX_DELAY) == pdTRUE) {
            // Debounce delay
            vTaskDelay(pdMS_TO_TICKS(50));
            // Read pin state (must be HIGH since TTP223 is active HIGH)
            if (gpio_get_level((gpio_num_t)BOOT_BUTTON_GPIO) == 1) {
                ESP_LOGI(TAG, "Wake button pressed (debounced)!");
                s_state = WW_STATE_DETECTED;
                if (s_callback) {
                    s_callback("button_wake", 1.0f);
                }
            }
        }
    }
}

esp_err_t ww_init(int mode) {
    ESP_LOGI(TAG, "Wake word init (mode=%d) - Using button wake on GPIO47", mode);
    
    // Create semaphore if not exists
    if (!s_btn_sem) {
        s_btn_sem = xSemaphoreCreateBinary();
    }
    
    // Create button task if not exists
    if (!s_btn_task) {
        xTaskCreatePinnedToCore(wake_button_task, "wake_btn", 3072, NULL, 5, &s_btn_task, 1);
    }
    
    // Setup wake button (GPIO47 active HIGH)
    gpio_config_t btn_conf = {
        .pin_bit_mask = (1ULL << BOOT_BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_POSEDGE
    };
    gpio_config(&btn_conf);
    
    // Install GPIO ISR service if not already installed
    esp_err_t err = gpio_install_isr_service(0);
    if (err == ESP_ERR_INVALID_STATE) {
        ESP_LOGI(TAG, "GPIO ISR service already installed");
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install GPIO ISR service: %s", esp_err_to_name(err));
        return err;
    }
    
    gpio_isr_handler_add((gpio_num_t)BOOT_BUTTON_GPIO, wake_button_isr_handler, NULL);
    
    s_state = WW_STATE_IDLE;
    return ESP_OK;
}

esp_err_t ww_start(void) {
    ESP_LOGI(TAG, "Wake word listening started (button wake)");
    s_state = WW_STATE_LISTENING;
    return ESP_OK;
}

esp_err_t ww_stop(void) {
    ESP_LOGI(TAG, "Wake word stopped");
    s_state = WW_STATE_IDLE;
    return ESP_OK;
}

esp_err_t ww_feed(const int16_t* samples, int count) {
    // Not used - button wake only
    return ESP_OK;
}

ww_state_t ww_get_state(void) { 
    return s_state; 
}

esp_err_t ww_reset(void) {
    s_state = WW_STATE_LISTENING;
    return ESP_OK;
}

bool ww_is_detected(void) { 
    return s_state == WW_STATE_DETECTED; 
}

void ww_on_detected(ww_detected_cb_t cb) { 
    s_callback = cb; 
}

float ww_get_audio_level(void) { 
    return 0.0f; 
}

void ww_set_threshold(float threshold) {
    ESP_LOGI(TAG, "Threshold set: %.0f (not used in button wake)", threshold);
}