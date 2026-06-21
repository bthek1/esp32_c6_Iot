#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// High-level effect/state engine layered over the low-level strip driver
// (strip.h). Owns the current mode, base colour, global brightness, animation
// speed and the per-pixel "custom" buffer, and runs a background task that
// renders the selected mode to the strip. The application only sets state and
// reads it back for status / preview; it never touches pixels directly.
//
// Modes < RAINBOW are static (redrawn only when state changes); RAINBOW and
// later are animated (redrawn every frame at the current speed). Most animated
// patterns are tinted by the base colour (strip_fx_set_color); RAINBOW and FIRE
// generate their own colours.

typedef enum {
    STRIP_FX_OFF = 0,
    STRIP_FX_SOLID,
    STRIP_FX_CUSTOM,
    STRIP_FX_RAINBOW,        // animated from here on
    STRIP_FX_BREATHE,
    STRIP_FX_CHASE,
    STRIP_FX_WIPE,
    STRIP_FX_SCANNER,
    STRIP_FX_TWINKLE,
    STRIP_FX_FIRE,
    STRIP_FX_COUNT,
} strip_fx_mode_t;

void strip_fx_start(void);                       // launch the render task (after strip_init)

void            strip_fx_set_mode(strip_fx_mode_t mode);
strip_fx_mode_t strip_fx_mode(void);
const char     *strip_fx_mode_name(void);        // upper-case name of the current mode
strip_fx_mode_t strip_fx_mode_from_name(const char *s);  // "solid"/"rainbow"/… → mode (OFF if unknown)

void strip_fx_set_color(uint8_t r, uint8_t g, uint8_t b);   // base colour (does not change mode)
void strip_fx_get_color(uint8_t *r, uint8_t *g, uint8_t *b);

void    strip_fx_set_brightness(uint8_t b);      // 0–255 global scale
uint8_t strip_fx_brightness(void);

void strip_fx_set_speed(int speed);              // 1 (slow) – 10 (fast)
int  strip_fx_speed(void);

void strip_fx_set_pixel(int i, uint8_t r, uint8_t g, uint8_t b);  // one LED (also selects CUSTOM)
int  strip_fx_dump_hex(char *out, int cap);      // "rrggbb" per LED of the last frame → length

#ifdef __cplusplus
}
#endif
