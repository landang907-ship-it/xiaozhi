// esp32s3_wroom1_board.h
// Board driver cho ESP32-S3-WROOM-1 (N16R8: 4MB flash, 2MB Octal PSRAM)
// Phase 2.1: HAL init only (I2C + I2S in/out). Wire Application in Phase 2.2.

#ifndef ESP32S3_WROOM1_BOARD_H
#define ESP32S3_WROOM1_BOARD_H

#include <driver/i2c_master.h>
#include <driver/i2s_std.h>
#include <driver/gpio.h>
#include "config.h"

class Esp32S3Wroom1Board {
public:
    Esp32S3Wroom1Board();
    ~Esp32S3Wroom1Board();

    // Initialize all hardware (I2C bus for OLED, I2S0 spk out, I2S1 mic in)
    void Initialize();

    // HAL handles
    i2c_master_bus_handle_t GetI2CBus() const { return i2c_bus_; }
    i2s_chan_handle_t GetSpeakerHandle() const { return speaker_handle_; }
    i2s_chan_handle_t GetMicHandle() const { return mic_handle_; }

    // Constants
    static constexpr uint8_t OLED_I2C_ADDR = 0x3C;

private:
    void InitI2C();
    void InitSpeakerI2S();
    void InitMicI2S();

    i2c_master_bus_handle_t i2c_bus_ = nullptr;
    i2s_chan_handle_t speaker_handle_ = nullptr;
    i2s_chan_handle_t mic_handle_ = nullptr;
};

#endif // ESP32S3_WROOM1_BOARD_H
