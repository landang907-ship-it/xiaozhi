# ============================================================================
# Top-level CMakeLists.txt cho project test_oled
# ----------------------------------------------------------------------------
# Project ESP-IDF chạy file main/test_oled.cc để test màn hình OLED SSD1306.
# ============================================================================

cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS "")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(test_oled)
