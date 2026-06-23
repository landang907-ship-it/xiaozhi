// esp32s3_wroom1_board.cc
// Pin map: see config.h (single source of truth)

#include "esp32s3_wroom1_board.h"
#include <esp_log.h>

static const char* TAG = "board";
static constexpr uint32_t I2C_FREQ_HZ = 400000;

Esp32S3Wroom1Board::Esp32S3Wroom1Board() {}

Esp32S3Wroom1Board::~Esp32S3Wroom1Board() {
    if (speaker_handle_) i2s_del_channel(speaker_handle_);
    if (mic_handle_) i2s_del_channel(mic_handle_);
    if (i2c_bus_) i2c_del_master_bus(i2c_bus_);
}

void Esp32S3Wroom1Board::Initialize() {
    ESP_LOGI(TAG, "==== ESP32-S3-WROOM-1 (N16R8) init ====");
    InitI2C();
    InitSpeakerI2S();
    InitMicI2S();
    ESP_LOGI(TAG, "Board HAL OK: I2C0 OLED@0x%02X, I2S0 spk, I2S1 mic", OLED_I2C_ADDR);
}

void Esp32S3Wroom1Board::InitI2C() {
    i2c_master_bus_config_t bus_cfg = {};
    bus_cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_cfg.i2c_port = DISPLAY_I2C_NUM;
    bus_cfg.sda_io_num = DISPLAY_SDA_PIN;
    bus_cfg.scl_io_num = DISPLAY_SCL_PIN;
    bus_cfg.glitch_ignore_cnt = 7;
    bus_cfg.flags.enable_internal_pullup = true;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &i2c_bus_));
    ESP_LOGI(TAG, "I2C0 ready (SDA=%d, SCL=%d, %d kHz)",
        (int)DISPLAY_SDA_PIN, (int)DISPLAY_SCL_PIN, (int)(I2C_FREQ_HZ / 1000));
}

void Esp32S3Wroom1Board::InitSpeakerI2S() {
    i2s_chan_config_t chan_cfg = {};
    chan_cfg.id = I2S_NUM_0;
    chan_cfg.role = I2S_ROLE_MASTER;
    chan_cfg.dma_desc_num = 8;
    chan_cfg.dma_frame_num = 240;  // 10ms @ 24kHz
    chan_cfg.auto_clear = true;
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &speaker_handle_, nullptr));

    i2s_std_config_t std_cfg = {};
    std_cfg.clk_cfg.sample_rate_hz = AUDIO_OUTPUT_SAMPLE_RATE;
    std_cfg.clk_cfg.clk_src = I2S_CLK_SRC_DEFAULT;
    std_cfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
    std_cfg.slot_cfg.data_bit_width = I2S_DATA_BIT_WIDTH_16BIT;
    std_cfg.slot_cfg.slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO;
    std_cfg.slot_cfg.slot_mode = I2S_SLOT_MODE_MONO;
    std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
    std_cfg.gpio_cfg.bclk = AUDIO_I2S_SPK_GPIO_BCLK;
    std_cfg.gpio_cfg.ws = AUDIO_I2S_SPK_GPIO_WS;
    std_cfg.gpio_cfg.dout = AUDIO_I2S_SPK_GPIO_DOUT;
    std_cfg.gpio_cfg.din = I2S_GPIO_UNUSED;
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(speaker_handle_, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(speaker_handle_));
    ESP_LOGI(TAG, "I2S0 spk ready (BCLK=%d, WS=%d, DOUT=%d, %d Hz mono)",
        (int)AUDIO_I2S_SPK_GPIO_BCLK, (int)AUDIO_I2S_SPK_GPIO_WS,
        (int)AUDIO_I2S_SPK_GPIO_DOUT, (int)AUDIO_OUTPUT_SAMPLE_RATE);
}

void Esp32S3Wroom1Board::InitMicI2S() {
    i2s_chan_config_t chan_cfg = {};
    chan_cfg.id = I2S_NUM_1;
    chan_cfg.role = I2S_ROLE_MASTER;
    chan_cfg.dma_desc_num = 8;
    chan_cfg.dma_frame_num = 240;  // 10ms @ 24kHz
    chan_cfg.auto_clear = true;
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, nullptr, &mic_handle_));

    i2s_std_config_t std_cfg = {};
    std_cfg.clk_cfg.sample_rate_hz = AUDIO_INPUT_SAMPLE_RATE;
    std_cfg.clk_cfg.clk_src = I2S_CLK_SRC_DEFAULT;
    std_cfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
    // INMP441 outputs 24-bit data left-justified in 32-bit slot
    std_cfg.slot_cfg.data_bit_width = I2S_DATA_BIT_WIDTH_32BIT;
    std_cfg.slot_cfg.slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO;
    std_cfg.slot_cfg.slot_mode = I2S_SLOT_MODE_MONO;
    std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
    std_cfg.gpio_cfg.bclk = AUDIO_I2S_MIC_GPIO_SCK;
    std_cfg.gpio_cfg.ws = AUDIO_I2S_MIC_GPIO_WS;
    std_cfg.gpio_cfg.dout = I2S_GPIO_UNUSED;
    std_cfg.gpio_cfg.din = AUDIO_I2S_MIC_GPIO_DIN;
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(mic_handle_, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(mic_handle_));
    ESP_LOGI(TAG, "I2S1 mic ready (SCK=%d, WS=%d, DIN=%d, %d Hz mono 32-bit)",
        (int)AUDIO_I2S_MIC_GPIO_SCK, (int)AUDIO_I2S_MIC_GPIO_WS,
        (int)AUDIO_I2S_MIC_GPIO_DIN, (int)AUDIO_INPUT_SAMPLE_RATE);
}
