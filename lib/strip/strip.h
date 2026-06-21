#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// WS2812B (NeoPixel) addressable LED strip driven by the ESP32-C6 RMT TX
// peripheral. One RMT channel feeds the whole strip from an in-RAM GRB pixel
// buffer; call strip_show() to flush the buffer out the data line.
//
// Wiring note: a long 5 V WS2812B strip can pull many amps at full white
// (≈60 mA/LED). Power it from a separate 5 V supply sized for the strip, share
// ground with the board, and feed the strip's DIN from the data GPIO. Keep the
// global brightness modest unless the supply can take it.

bool strip_init(int gpio, int count);                       // alloc RMT + buffer, all off
void strip_set_pixel(int i, uint8_t r, uint8_t g, uint8_t b);  // buffer one pixel (brightness-scaled)
void strip_fill(uint8_t r, uint8_t g, uint8_t b);           // set every pixel to one colour
void strip_clear(void);                                     // buffer all pixels off
void strip_set_brightness(uint8_t b);                       // 0–255 global scale, applied on write
void strip_show(void);                                      // blocking flush of the buffer to the strip
int  strip_count(void);                                     // configured LED count

#ifdef __cplusplus
}
#endif
