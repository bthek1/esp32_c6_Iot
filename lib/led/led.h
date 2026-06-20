#pragma once

#include <stdbool.h>

// Plain on/off GPIO LED driver.
//
// Board note: the Seeed Studio XIAO ESP32-C6 user LED is on GPIO2 and is
// *active-low* — driving the pin LOW turns it on. Pass active_low=true so
// led_on()/led_off() mean what they say regardless of wiring polarity.

void led_init(int gpio, bool active_low);  // configure as output, LED off
void led_on(void);
void led_off(void);
void led_toggle(void);
