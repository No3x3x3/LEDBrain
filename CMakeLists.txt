cmake_minimum_required(VERSION 3.16)
# Add only eppp_link component, not all esp-protocols components
set(EXTRA_COMPONENT_DIRS "components/esp-protocols/components/eppp_link")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ledbrain)
