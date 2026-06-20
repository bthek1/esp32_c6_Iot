#pragma once

// Simple GPIO LED helper.
//
// NOTE: the ESP32-C6-DevKitC-1 onboard LED is an *addressable* WS2812 RGB LED
// on GPIO8 — a plain gpio_set_level() will not drive it. For that board, wire a
// plain LED to the chosen GPIO, or swap this component for one built on the
// `led_strip` managed component. These helpers drive a standard on/off GPIO LED.

void led_init(int gpio);
void led_on(int gpio);
void led_off(int gpio);
void led_toggle(int gpio);
