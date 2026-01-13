cmake_minimum_required(VERSION 3.16)
# Add only eppp_link component, not all esp-protocols components
set(EXTRA_COMPONENT_DIRS "components/esp-protocols/components/eppp_link")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ledbrain)

# Note: We only use OTA partitions (ota_0, ota_1), no factory partition exists.
# With CONFIG_PARTITION_TABLE_CUSTOM=y, idf.py flash will automatically
# detect ota_0 as the first app partition and flash to offset 0x20000.
# To flash firmware, use: idf.py flash -p COMx