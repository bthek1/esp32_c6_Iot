cmake_minimum_required(VERSION 3.16)

# On-device Unity test runner. Builds like any other target; flash it to the
# ESP32-C6 and the results print over the USB-Serial-JTAG console. Pulls in
# every library it exercises (led, strip, wifi, sysinfo + serial for output).
set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../../lib/serial"
    "${CMAKE_CURRENT_LIST_DIR}/../../lib/led"
    "${CMAKE_CURRENT_LIST_DIR}/../../lib/strip"
    "${CMAKE_CURRENT_LIST_DIR}/../../lib/wifi"
    "${CMAKE_CURRENT_LIST_DIR}/../../lib/sysinfo")
set(SDKCONFIG_DEFAULTS   "${CMAKE_CURRENT_LIST_DIR}/../../sdkconfig.defaults")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(unit_test)
