// main/boards/esp32s3-wroom-1/config.h
// Source: PINOUT.md (verified 2026-06-17)
// Board: ESP32-S3-WROOM-1 (N16R8: 4MB flash, 2MB Octal PSRAM)

#pragma once

// ============================================================
// Speaker - MAX98357A on I2S_NUM_0
// ============================================================
#define AUDIO_I2S_SPK_GPIO_BCLK    GPIO_NUM_15
#define AUDIO_I2S_SPK_GPIO_WS      GPIO_NUM_16
#define AUDIO_I2S_SPK_GPIO_DOUT    GPIO_NUM_7

// ============================================================
// Microphone - INMP441 on I2S_NUM_1
// ============================================================
#define AUDIO_I2S_MIC_GPIO_SCK     GPIO_NUM_5
#define AUDIO_I2S_MIC_GPIO_WS      GPIO_NUM_4
#define AUDIO_I2S_MIC_GPIO_DIN     GPIO_NUM_6

// ============================================================
// Audio sample rates (Hz)
// ============================================================
#define AUDIO_INPUT_SAMPLE_RATE    24000
#define AUDIO_OUTPUT_SAMPLE_RATE   24000

// ============================================================
// Buttons
// ============================================================
// Wake button - TTP223 active-HIGH on GPIO47
#define BOOT_BUTTON_GPIO           GPIO_NUM_47  // Note: tên "BOOT" từ xiaozhi convention, GPIO47 = wake
#define WAKE_BUTTON_ACTIVE_HIGH    1

// Reset SSID - BOOT button on GPIO0 (active-LOW long-press 5s)
#define RESET_SSID_BUTTON_GPIO     GPIO_NUM_0
#define RESET_SSID_LONG_PRESS_MS   5000

// ============================================================
// OLED - SSD1306 128x64 on GPIO1/2 (safe, not strapping)
// ============================================================
#define DISPLAY_SDA_PIN            GPIO_NUM_8
#define DISPLAY_SCL_PIN            GPIO_NUM_9
#define DISPLAY_WIDTH              128
#define DISPLAY_HEIGHT             64
#define DISPLAY_MIRROR_X           true
#define DISPLAY_MIRROR_Y           true
#define DISPLAY_I2C_NUM            I2C_NUM_0

// ============================================================
// No status LED
// ============================================================
#define BUILTIN_LED_GPIO           GPIO_NUM_NC

// ============================================================
// Wake word off, dùng button
// ============================================================
#define HANDS_FREE_AUTO_LISTEN     1
#define CONFIG_WAKE_WORD_DISABLED  1

// ============================================================
// Opus frame (60ms @ 24kHz = 1440 samples)
// ============================================================
#define AUDIO_OPUS_FRAME_DURATION_MS   60
#define AUDIO_OPUS_FRAME_SIZE_SAMPLES  1440
