// lib/led — plain on/off GPIO LED + blink mode.
//
// Driven on the real XIAO ESP32-C6 user LED (GPIO15, active-low), so the LED
// visibly flickers while these run. Assertions are on the *logical* state
// (led_is_on) and the blink-task lifecycle, which is what the API guarantees.

#include "unity.h"
#include "led.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define LED_PIN 15   // XIAO ESP32-C6 user LED
#define ACTIVE_LOW true

static void test_led_init_starts_off(void) {
    led_init(LED_PIN, ACTIVE_LOW);
    TEST_ASSERT_FALSE(led_is_on());
}

static void test_led_on_off(void) {
    led_on();
    TEST_ASSERT_TRUE(led_is_on());
    led_off();
    TEST_ASSERT_FALSE(led_is_on());
}

static void test_led_toggle(void) {
    led_off();
    led_toggle();
    TEST_ASSERT_TRUE(led_is_on());
    led_toggle();
    TEST_ASSERT_FALSE(led_is_on());
}

static void test_led_blink_lifecycle(void) {
    TEST_ASSERT_FALSE(led_is_blinking());

    led_blink(100);
    TEST_ASSERT_TRUE(led_is_blinking());

    // Let the background task run a few toggles — just proving it doesn't crash.
    vTaskDelay(pdMS_TO_TICKS(350));

    led_blink_stop();
    TEST_ASSERT_FALSE(led_is_blinking());

    led_off();   // leave it in a known state for the next suite
}

void run_led_tests(void) {
    RUN_TEST(test_led_init_starts_off);
    RUN_TEST(test_led_on_off);
    RUN_TEST(test_led_toggle);
    RUN_TEST(test_led_blink_lifecycle);
}
