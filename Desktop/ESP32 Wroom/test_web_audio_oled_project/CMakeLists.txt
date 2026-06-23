# ============================================================================
# CMakeLists.txt — test_web_audio_oled
# ============================================================================
# ESP32-S3-WROOM-1 project:
#   - INMP441 I2S Mic (GPIO4/5/6)
#   - SSD1306 OLED I2C (GPIO8/9)
#   - WiFi STA + HTTP server + WebSocket
#   - Stream real-time audio waveform/VU lên trình duyệt web và OLED
# ============================================================================

cmake_minimum_required(VERSION 3.16)
set(EXTRA_COMPONENT_DIRS "")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(test_web_audio_oled)
