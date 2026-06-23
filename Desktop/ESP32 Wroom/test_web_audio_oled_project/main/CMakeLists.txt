idf_component_register(SRCS "test_web_audio_oled.cc"
                       INCLUDE_DIRS "."
                       EMBED_TXTFILES "../components/web/index.html"
                       REQUIRES esp_wifi esp_http_server nvs_flash esp_event esp_netif
                       PRIV_REQUIRES driver esp_driver_i2s esp_driver_i2c esp_driver_gpio esp_lcd mbedtls json esp_timer)
