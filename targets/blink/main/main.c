#include "led.h"
#include "serial.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Seeed Studio XIAO ESP32-C6: the onboard user LED is a plain LED on GPIO15
// (the red LED is the charge indicator and is not software-controllable).
// Driven via the `led` library.
#define LED_PIN   15
#define BLINK_MS  500

void app_main(void) {
    serial_init();
    led_init(LED_PIN, true);   // active-low

    serial_println("blink start");

    uint32_t n = 0;
    while (true) {
        led_toggle();
        serial_println("blink %lu", (unsigned long)n++);
        vTaskDelay(pdMS_TO_TICKS(BLINK_MS));
    }
}
