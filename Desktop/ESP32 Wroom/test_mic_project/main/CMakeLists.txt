idf_component_register(
    SRCS "test_mic.cc"
    INCLUDE_DIRS "."
    PRIV_REQUIRES
        driver          # I2S std RX (i2s_channel_new, i2s_channel_init_std_mode)
        esp_timer       # esp_timer_get_time()
        console         # esp_console.h (REPL menu)
)