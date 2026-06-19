# ============================================================================
# Top-level CMakeLists.txt cho project test_speaker
# ----------------------------------------------------------------------------
# Project ESP-IDF chạy file main/test_speaker.cc để test loa MAX98357A
# qua I2S_NUM_0 (BCLK=15, WS=16, DOUT=7).
# Project này HOÀN TOÀN ĐỘC LẬP với test_oled_project (không dùng chung
# build/, sdkconfig, hay flash).
# ============================================================================

cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS "")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(test_speaker)