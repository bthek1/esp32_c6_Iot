#pragma once

#include <stdbool.h>

// Plain on/off GPIO LED driver.
//
// Board note: the Seeed Studio XIAO ESP32-C6 user LED is on GPIO15 and is
// *active-low* — driving the pin LOW turns it on. Pass active_low=true so
// led_on()/led_off() mean what they say regardless of wiring polarity.

void led_init(int gpio, bool active_low);  // configure as output, LED off
void led_on(void);
void led_off(void);
void led_toggle(void);
bool led_is_on(void);                      // logical state (on == lit)

// Blink mode: a background task toggles the LED every `period_ms`. Starting it
// is non-blocking; the task is created on first use. led_on/off/toggle do *not*
// stop it — call led_blink_stop() (the task itself drives led_toggle()).
void led_blink(int period_ms);             // start/retune blink at this half-period
void led_blink_stop(void);
bool led_is_blinking(void);
