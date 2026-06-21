#include "led.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static int  s_gpio = -1;
static bool s_active_low = false;
static bool s_on = false;

static volatile int s_blink_ms   = 0;       // 0 = not blinking
static TaskHandle_t s_blink_task = NULL;

static void apply(void) {
    int level = s_on ? 1 : 0;
    if (s_active_low) level = !level;   // invert for active-low wiring
    gpio_set_level(s_gpio, level);
}

void led_init(int gpio, bool active_low) {
    s_gpio = gpio;
    s_active_low = active_low;
    s_on = false;
    gpio_reset_pin(gpio);
    gpio_set_direction(gpio, GPIO_MODE_OUTPUT);
    apply();                             // start off
}

void led_on(void)     { s_on = true;  apply(); }
void led_off(void)    { s_on = false; apply(); }
void led_toggle(void) { s_on = !s_on; apply(); }
bool led_is_on(void)  { return s_on; }

static void blink_task(void *arg) {
    while (true) {
        int ms = s_blink_ms;
        if (ms > 0) { led_toggle(); vTaskDelay(pdMS_TO_TICKS(ms)); }
        else          vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void led_blink(int period_ms) {
    s_blink_ms = period_ms;
    if (!s_blink_task) xTaskCreate(blink_task, "led_blink", 2048, NULL, 5, &s_blink_task);
}

void led_blink_stop(void)  { s_blink_ms = 0; }
bool led_is_blinking(void) { return s_blink_ms > 0; }
