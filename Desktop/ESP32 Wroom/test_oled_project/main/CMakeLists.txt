idf_component_register(
    SRCS "test_oled.cc"
    INCLUDE_DIRS "."
    PRIV_REQUIRES
        driver          # I2C master, GPIO
        esp_timer       # esp_timer_get_time()
        console         # esp_console.h (REPL)
        esp_lcd         # esp_lcd_panel_io_i2c + esp_lcd_panel_ssd1306
        fatfs           # FATFS (wears/SD/MMC)
    )
