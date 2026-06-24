// esp32s3_audio_codec.cc
// Read:  I2S_NUM_1 RX, 32-bit slot, 24-bit data left-justified.
//        Downshift by 8 to convert 24-bit to 16-bit PCM (full scale preserved).
// Write: I2S_NUM_0 TX, 16-bit slot, apply output_volume_ (0-100) before write.
// Phase 2.2.

#include "esp32s3_audio_codec.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <vector>
#include <cstring>

static const char* TAG = "Esp32S3AudioCodec";

Esp32S3AudioCodec::Esp32S3AudioCodec(i2s_chan_handle_t speaker_handle,
                                     i2s_chan_handle_t mic_handle,
                                     int input_sample_rate,
                                     int output_sample_rate)
    : AudioCodec(speaker_handle, mic_handle, input_sample_rate, output_sample_rate) {
    duplex_ = true;
    input_enabled_ = true;
    output_enabled_ = true;
    input_sample_rate_ = input_sample_rate;
    output_sample_rate_ = output_sample_rate;
    input_channels_ = 1;
    output_channels_ = 1;
    output_volume_ = 70;     // 0-100 scale
    input_gain_ = 1.0f;      // pass-through by default
    tx_handle_ = speaker_handle;
    rx_handle_ = mic_handle;
}

Esp32S3AudioCodec::~Esp32S3AudioCodec() {
    // I2S channels owned by Esp32S3Wroom1Board, do NOT delete here.
}

void Esp32S3AudioCodec::Start() {
    ESP_LOGI(TAG, "Audio codec start (in=%d Hz, out=%d Hz, vol=%d)",
        input_sample_rate_, output_sample_rate_, output_volume_);
    // Channels already enabled by board HAL Initialize().
}

void Esp32S3AudioCodec::EnableInput(bool enable) {
    input_enabled_ = enable;
    // Channels already enabled/disabled by board HAL in Initialize().
    // Codec layer only tracks enable flag, does NOT re-enable channels.
}

void Esp32S3AudioCodec::EnableOutput(bool enable) {
    output_enabled_ = enable;
    // Channels already enabled/disabled by board HAL in Initialize().
    // Codec layer only tracks enable flag, does NOT re-enable channels.
}

int Esp32S3AudioCodec::Read(int16_t* dest, int samples) {
    if (!input_enabled_ || rx_handle_ == nullptr) return 0;

    // INMP441 outputs 24-bit data in 32-bit slot (Philips format, left-justified).
    // We read as 32-bit and extract left channel only (step by 2 since it's stereo slot).
    std::vector<int32_t> raw(samples * 2);
    size_t bytes_read = 0;
    esp_err_t ret = i2s_channel_read(rx_handle_, raw.data(),
        raw.size() * sizeof(int32_t), &bytes_read, portMAX_DELAY);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s read failed: %s", esp_err_to_name(ret));
        return 0;
    }
    int words_read = bytes_read / sizeof(int32_t);
    int samples_read = 0;

    // Extract left channel (every 2 words) + convert 24-bit → 16-bit.
    // INMP441 outputs 24-bit left-justified in 32-bit slot. Take top 16 bits
    // (>>16) to scale ±8M range down to ±32K range. No clamping needed.
    for (int i = 0; i < words_read; i += 2) {
        dest[samples_read++] = (int16_t)(raw[i] >> 16);
    }

    // Apply input gain if non-unity.
    if (input_gain_ != 1.0f && samples_read > 0) {
        for (int i = 0; i < samples_read; i++) {
            int32_t v = (int32_t)((float)dest[i] * input_gain_);
            if (v > 32767) v = 32767;
            else if (v < -32768) v = -32768;
            dest[i] = (int16_t)v;
        }
    }
    return samples_read;
}

int Esp32S3AudioCodec::Write(const int16_t* data, int samples) {
    if (!output_enabled_ || tx_handle_ == nullptr) return 0;

    // Apply output volume (0-100).
    std::vector<int16_t> buf(samples);
    int vol = output_volume_;
    if (vol < 0) vol = 0;
    if (vol > 100) vol = 100;
    for (int i = 0; i < samples; i++) {
        int32_t v = ((int32_t)data[i] * vol) / 100;
        if (v > 32767) v = 32767;
        else if (v < -32768) v = -32768;
        buf[i] = (int16_t)v;
    }

    size_t bytes_written = 0;
    esp_err_t ret = i2s_channel_write(tx_handle_, buf.data(),
        samples * sizeof(int16_t), &bytes_written, portMAX_DELAY);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s write failed: %s", esp_err_to_name(ret));
        return 0;
    }
    return bytes_written / sizeof(int16_t);
}
