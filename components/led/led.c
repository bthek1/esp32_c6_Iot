#include "led.h"
#include "driver/gpio.h"

static int s_level = 0;

void led_init(int gpio) {
    gpio_reset_pin(gpio);
    gpio_set_direction(gpio, GPIO_MODE_OUTPUT);
    gpio_set_level(gpio, 0);
    s_level = 0;
}

void led_on(int gpio) {
    gpio_set_level(gpio, 1);
    s_level = 1;
}

void led_off(int gpio) {
    gpio_set_level(gpio, 0);
    s_level = 0;
}

void led_toggle(int gpio) {
    s_level = !s_level;
    gpio_set_level(gpio, s_level);
}
