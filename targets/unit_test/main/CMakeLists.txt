idf_component_register(
    SRCS "main.c" "test_led.c" "test_strip.c" "test_wifi.c" "test_sysinfo.c"
    INCLUDE_DIRS "."
    REQUIRES unity serial led strip wifi sysinfo esp_timer
)

# secrets.h (WIFI_SSID / WIFI_PASSWORD) lives at the repo root, two levels up.
target_include_directories(${COMPONENT_LIB} PRIVATE "${CMAKE_SOURCE_DIR}/../..")
