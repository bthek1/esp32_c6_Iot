#include "led.h"
#include "driver/gpio.h"

static int  s_gpio = -1;
static bool s_active_low = false;
static bool s_on = false;

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
