#include "led.h"
#include "serial.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define LED_PIN  8      // ESP32-C6-DevKitC-1: see note in led.h
#define BLINK_MS 500

void app_main(void) {
    serial_init();
    led_init(LED_PIN);

    serial_println("blink start");

    while (true) {
        led_toggle(LED_PIN);
        vTaskDelay(pdMS_TO_TICKS(BLINK_MS));
    }
}
