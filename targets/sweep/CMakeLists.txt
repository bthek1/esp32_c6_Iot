cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../../lib/servo"
    "${CMAKE_CURRENT_LIST_DIR}/../../lib/serial")
set(SDKCONFIG_DEFAULTS   "${CMAKE_CURRENT_LIST_DIR}/../../sdkconfig.defaults")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(sweep)
