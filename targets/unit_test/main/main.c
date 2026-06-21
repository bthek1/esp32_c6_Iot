// On-device Unity test runner for the shared libraries.
//
// Flash this target to the ESP32-C6 (./flash.sh unit_test, or `just test-device`)
// and watch the console: Unity prints a PASS/FAIL line per case and a summary.
// Output is the standard Unity format, so pytest-embedded's dut.expect_unity_*()
// can drive it in CI.
//
// These are *integration* tests against real peripherals (GPIO/RMT/temp sensor)
// and — for wifi — a real AP, so some cases depend on the board's environment.

#include "unity.h"
#include "serial.h"
#include "tests.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Unity calls these around every test; nothing global to set up here.
void setUp(void)    {}
void tearDown(void) {}

void app_main(void) {
    serial_init();
    vTaskDelay(pdMS_TO_TICKS(500));   // let the USB-Serial-JTAG console settle

    serial_println("\n========== on-device unit tests ==========");

    UNITY_BEGIN();
    run_led_tests();
    run_strip_tests();
    run_sysinfo_tests();
    run_wifi_tests();                 // last: slow + one-shot netif init
    int failures = UNITY_END();

    serial_println("========== done: %d failure(s) ==========", failures);

    // Keep the task alive so the console output isn't cut off by a reboot.
    while (true) vTaskDelay(pdMS_TO_TICKS(1000));
}
