# ============================================================================
# Top-level CMakeLists.txt cho project test_mic
# ----------------------------------------------------------------------------
# Project ESP-IDF chạy file main/test_mic.cc để test micro INMP441
# qua I2S_NUM_1 (SCK=5, WS=4, SD=6).
# Project này HOÀN TOÀN ĐỘC LẬP với test_oled_project và test_speaker_project
# (không dùng chung build/, sdkconfig, hay flash).
# ============================================================================

cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS "")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(test_mic)