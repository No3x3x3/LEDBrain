# ESP32-C6 WiFi Coprocessor Firmware for LEDBrain
# This firmware runs on ESP32-C6 and provides WiFi connectivity to ESP32-P4 via PPP

cmake_minimum_required(VERSION 3.16)

# Add only eppp_link component, not all esp-protocols components
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/../components/esp-protocols/components/eppp_link")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ledbrain_c6_coprocessor)

