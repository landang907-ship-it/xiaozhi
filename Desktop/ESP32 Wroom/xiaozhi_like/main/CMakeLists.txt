# main/CMakeLists.txt
# Phase 2.3: +Ssd1306 raw-fb driver (I2C_NUM_0, GPIO8/9, 128x64, no LVGL)
# Phase 2.2: +Esp32S3AudioCodec (I2S bridge, I2S_NUM_0 spk + I2S_NUM_1 mic)
# Phase 2.1: HAL driver init only (I2C bus + I2S0 spk + I2S1 mic)
# xiaozhi_src/ integration deferred to Phase 2.4 (needs wifi/ota/ws/opus/etc deps)

idf_component_register(
    SRCS 
        "main.cc"
        "boards/esp32s3-wroom-1/esp32s3_wroom1_board.cc"
        "boards/esp32s3-wroom-1/ssd1306.cc"
        "audio/audio_codec.cc"
        "audio/esp32s3_audio_codec.cc"
    INCLUDE_DIRS 
        "."
        "boards/esp32s3-wroom-1"
        "audio"
    REQUIRES 
        "driver"
        "esp_lcd"
        "esp_timer"
        "nvs_flash"
        "freertos"
        "esp_event"
)
