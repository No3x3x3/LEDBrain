cmake_minimum_required(VERSION 3.16)
# Add only eppp_link component, not all esp-protocols components
set(EXTRA_COMPONENT_DIRS "components/esp-protocols/components/eppp_link")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ledbrain)

# Note: We only use OTA partitions (ota_0, ota_1), no factory partition exists.
# To flash firmware, use: idf.py flash -p COMx
# Or use esptool directly to flash to OTA partition at offset 0x20000