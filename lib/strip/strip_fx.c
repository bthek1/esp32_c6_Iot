#include "strip_fx.h"
#include "strip.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "strip_fx";

static volatile strip_fx_mode_t s_mode  = STRIP_FX_OFF;
static volatile uint8_t s_r = 255, s_g = 80, s_b = 0;   // base colour
static volatile uint8_t s_bright = 64;
static volatile int     s_speed  = 5;                   // 1..10
static volatile bool    s_dirty  = true;                // static modes redraw only when set

// custom : per-pixel colours for CUSTOM mode.
// view   : mirror of whatever was rendered last frame (for the UI preview).
// heat   : scratch per-pixel intensity for TWINKLE / FIRE.
static uint8_t (*s_custom)[3] = NULL;
static uint8_t (*s_view)[3]   = NULL;
static uint8_t  *s_heat       = NULL;
static int       s_count      = 0;

// ── helpers ─────────────────────────────────────────────────────────────────
static void render_pixel(int i, uint8_t r, uint8_t g, uint8_t b) {
    s_view[i][0] = r; s_view[i][1] = g; s_view[i][2] = b;
    strip_set_pixel(i, r, g, b);
}

// Scale the base colour by 0..255 and write it.
static void render_tinted(int i, int level) {
    if (level < 0)   level = 0;
    if (level > 255) level = 255;
    render_pixel(i, s_r * level / 255, s_g * level / 255, s_b * level / 255);
}

// Full-saturation HSV → RGB (h in 0–359).
static void hsv2rgb(int h, uint8_t *r, uint8_t *g, uint8_t *b) {
    int rem = (h % 60) * 255 / 60;
    uint8_t q = 255 - rem, t = rem;
    switch ((h / 60) % 6) {
        case 0:  *r = 255; *g = t;   *b = 0;   break;
        case 1:  *r = q;   *g = 255; *b = 0;   break;
        case 2:  *r = 0;   *g = 255; *b = t;   break;
        case 3:  *r = 0;   *g = q;   *b = 255; break;
        case 4:  *r = t;   *g = 0;   *b = 255; break;
        default: *r = 255; *g = 0;   *b = q;   break;
    }
}

// Heat (0..255) → fire palette: black → red → yellow → white.
static void heat_color(uint8_t h, uint8_t *r, uint8_t *g, uint8_t *b) {
    uint8_t t = (uint8_t)((unsigned)h * 191 / 255);   // 0..191
    uint8_t ramp = (t & 0x3f) << 2;                   // 0..252 within each band
    if      (t > 128) { *r = 255; *g = 255; *b = ramp; }
    else if (t > 64)  { *r = 255; *g = ramp; *b = 0;   }
    else              { *r = ramp; *g = 0;   *b = 0;   }
}

static uint32_t s_rng = 0x2545f491;
static uint32_t rng(void) {                            // xorshift32
    s_rng ^= s_rng << 13; s_rng ^= s_rng >> 17; s_rng ^= s_rng << 5;
    return s_rng;
}

// ── patterns (each renders one frame for counter t) ─────────────────────────
static void pat_rainbow(int t) {
    for (int i = 0; i < s_count; i++) {
        uint8_t r, g, b;
        hsv2rgb((i * 360 / (s_count ? s_count : 1) + t * 4) % 360, &r, &g, &b);
        render_pixel(i, r, g, b);
    }
}

static void pat_breathe(int t) {
    int x = (t * 4) % 512;
    int level = x < 256 ? x : 512 - x;                 // triangle 0..256
    for (int i = 0; i < s_count; i++) render_tinted(i, level);
}

static void pat_chase(int t) {                         // theatre chase: every 3rd lit
    for (int i = 0; i < s_count; i++)
        render_tinted(i, ((i + t) % 3 == 0) ? 255 : 0);
}

static void pat_wipe(int t) {                          // fill on, then wipe off, repeat
    int pos = t % (2 * s_count);
    for (int i = 0; i < s_count; i++) {
        bool on = pos < s_count ? (i < pos) : (i >= pos - s_count);
        render_tinted(i, on ? 255 : 0);
    }
}

static void pat_scanner(int t) {                       // comet bouncing with a fading tail
    int span = s_count > 1 ? s_count - 1 : 1;
    int x = t % (2 * span);
    int pos = x < span ? x : 2 * span - x;             // bounce
    for (int i = 0; i < s_count; i++) {
        int d = i - pos; if (d < 0) d = -d;
        render_tinted(i, 255 - d * 64);                // ~4-pixel tail
    }
}

static void pat_twinkle(int t) {                       // random sparkles that fade out
    (void)t;
    for (int i = 0; i < s_count; i++)
        s_heat[i] = s_heat[i] > 24 ? s_heat[i] - 24 : 0;
    for (int k = 0; k < s_count / 20 + 1; k++)
        s_heat[rng() % s_count] = 255;
    for (int i = 0; i < s_count; i++) render_tinted(i, s_heat[i]);
}

static void pat_fire(int t) {                          // Fire2012-style flicker
    (void)t;
    for (int i = 0; i < s_count; i++) {                // cool every cell a little
        int cool = rng() % (((55 * 10) / (s_count ? s_count : 1)) + 2);
        s_heat[i] = s_heat[i] > cool ? s_heat[i] - cool : 0;
    }
    for (int i = s_count - 1; i >= 2; i--)             // drift heat upward
        s_heat[i] = (s_heat[i - 1] + s_heat[i - 2] * 2) / 3;
    if (rng() % 255 < 120) {                           // spark near the base
        int y = rng() % 7;
        if (y < s_count) { int v = s_heat[y] + 160 + rng() % 96; s_heat[y] = v > 255 ? 255 : v; }
    }
    for (int i = 0; i < s_count; i++) {
        uint8_t r, g, b; heat_color(s_heat[i], &r, &g, &b);
        render_pixel(i, r, g, b);
    }
}

// ── render task ─────────────────────────────────────────────────────────────
static bool is_animated(strip_fx_mode_t m) { return m >= STRIP_FX_RAINBOW; }

static void render_animated(strip_fx_mode_t m, int t) {
    switch (m) {
        case STRIP_FX_RAINBOW: pat_rainbow(t); break;
        case STRIP_FX_BREATHE: pat_breathe(t); break;
        case STRIP_FX_CHASE:   pat_chase(t);   break;
        case STRIP_FX_WIPE:    pat_wipe(t);    break;
        case STRIP_FX_SCANNER: pat_scanner(t); break;
        case STRIP_FX_TWINKLE: pat_twinkle(t); break;
        case STRIP_FX_FIRE:    pat_fire(t);    break;
        default: break;
    }
}

static void render_static(strip_fx_mode_t m) {
    for (int i = 0; i < s_count; i++) {
        if (m == STRIP_FX_SOLID)       render_pixel(i, s_r, s_g, s_b);
        else if (m == STRIP_FX_CUSTOM) render_pixel(i, s_custom[i][0], s_custom[i][1], s_custom[i][2]);
        else                           render_pixel(i, 0, 0, 0);
    }
}

static void strip_fx_task(void *arg) {
    int t = 0;
    while (true) {
        strip_set_brightness(s_bright);
        strip_fx_mode_t m = s_mode;
        if (is_animated(m)) {
            render_animated(m, t++);
            strip_show();
            vTaskDelay(pdMS_TO_TICKS(70 - s_speed * 6));   // speed 1→64ms … 10→10ms
        } else {
            if (s_dirty) { render_static(m); strip_show(); s_dirty = false; }
            vTaskDelay(pdMS_TO_TICKS(40));
        }
    }
}

void strip_fx_start(void) {
    s_count  = strip_count();
    s_custom = calloc(s_count, 3);
    s_view   = calloc(s_count, 3);
    s_heat   = calloc(s_count, 1);
    if (!s_custom || !s_view || !s_heat) { ESP_LOGE(TAG, "out of memory for %d pixels", s_count); return; }
    xTaskCreate(strip_fx_task, "strip_fx", 4096, NULL, 5, NULL);
}

// ── state accessors ─────────────────────────────────────────────────────────
static const char *const NAMES[STRIP_FX_COUNT] = {
    "off", "solid", "custom", "rainbow", "breathe", "chase", "wipe", "scanner", "twinkle", "fire",
};
static const char *const NAMES_UC[STRIP_FX_COUNT] = {
    "OFF", "SOLID", "CUSTOM", "RAINBOW", "BREATHE", "CHASE", "WIPE", "SCANNER", "TWINKLE", "FIRE",
};

void strip_fx_set_mode(strip_fx_mode_t mode) {
    if (mode >= 0 && mode < STRIP_FX_COUNT) { s_mode = mode; s_dirty = true; }
}
strip_fx_mode_t strip_fx_mode(void)  { return s_mode; }
const char *strip_fx_mode_name(void) { return NAMES_UC[s_mode]; }

strip_fx_mode_t strip_fx_mode_from_name(const char *s) {
    for (int m = 0; m < STRIP_FX_COUNT; m++)
        if (!strcmp(s, NAMES[m])) return (strip_fx_mode_t)m;
    return STRIP_FX_OFF;
}

void strip_fx_set_color(uint8_t r, uint8_t g, uint8_t b) {
    s_r = r; s_g = g; s_b = b; s_dirty = true;         // animated modes pick it up next frame
}
void strip_fx_get_color(uint8_t *r, uint8_t *g, uint8_t *b) {
    if (r) *r = s_r;
    if (g) *g = s_g;
    if (b) *b = s_b;
}

void strip_fx_set_brightness(uint8_t b) { s_bright = b; s_dirty = true; }
uint8_t strip_fx_brightness(void)       { return s_bright; }

void strip_fx_set_speed(int speed) {
    if (speed < 1)  speed = 1;
    if (speed > 10) speed = 10;
    s_speed = speed;
}
int strip_fx_speed(void) { return s_speed; }

void strip_fx_set_pixel(int i, uint8_t r, uint8_t g, uint8_t b) {
    if (!s_custom || i < 0 || i >= s_count) return;
    if (s_mode != STRIP_FX_CUSTOM) {
        memcpy(s_custom, s_view, (size_t)s_count * 3);  // keep the current look
        s_mode = STRIP_FX_CUSTOM;
    }
    s_custom[i][0] = r; s_custom[i][1] = g; s_custom[i][2] = b;
    s_dirty = true;
}

int strip_fx_dump_hex(char *out, int cap) {
    char *p = out;
    char *end = out + cap - 7;                          // room for one "rrggbb" + NUL
    for (int i = 0; i < s_count && p < end; i++)
        p += sprintf(p, "%02x%02x%02x", s_view[i][0], s_view[i][1], s_view[i][2]);
    return (int)(p - out);
}
