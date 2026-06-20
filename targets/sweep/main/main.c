#include "servo.h"
#include "serial.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define SERVO_PIN 4
#define STEP_DEG  2
#define STEP_MS   20

void app_main(void) {
    serial_init();
    servo_init(SERVO_PIN);

    serial_println("sweep start");

    float deg = 0.0f;
    int dir = STEP_DEG;

    while (true) {
        servo_set_deg(SERVO_PIN, deg);
        deg += dir;
        if (deg >= 180.0f || deg <= 0.0f) dir = -dir;
        vTaskDelay(pdMS_TO_TICKS(STEP_MS));
    }
}
