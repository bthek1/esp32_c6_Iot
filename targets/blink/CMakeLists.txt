cmake_minimum_required(VERSION 3.16)

# Pull in the shared components/ tree two levels up, and the repo-wide
# sdkconfig defaults (CONFIG_IDF_TARGET, console, flash settings).
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../components")
set(SDKCONFIG_DEFAULTS   "${CMAKE_CURRENT_LIST_DIR}/../../sdkconfig.defaults")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(blink)
