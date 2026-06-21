#include "strip.h"

#include <stdlib.h>
#include <string.h>
#include <sys/cdefs.h>          // __containerof
#include "driver/rmt_tx.h"
#include "driver/rmt_encoder.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "strip";

// 10 MHz RMT resolution → 0.1 us per tick. WS2812B bit timing:
//   '0' = 0.3 us high + 0.9 us low,  '1' = 0.9 us high + 0.3 us low.
#define RMT_RES_HZ 10000000

// ── WS2812 byte-stream encoder ──────────────────────────────────────────────
// Wraps a bytes-encoder (RGB → pulse timing) plus a copy-encoder that appends
// the >50 us low "reset" latch after the pixel data. Mirrors the ESP-IDF
// led_strip example encoder.
typedef struct {
    rmt_encoder_t base;
    rmt_encoder_t *bytes_encoder;
    rmt_encoder_t *copy_encoder;
    int state;
    rmt_symbol_word_t reset_code;
} ws_encoder_t;

static size_t ws_encode(rmt_encoder_t *encoder, rmt_channel_handle_t channel,
                        const void *data, size_t data_size,
                        rmt_encode_state_t *ret_state) {
    ws_encoder_t *enc = __containerof(encoder, ws_encoder_t, base);
    rmt_encoder_handle_t bytes = enc->bytes_encoder;
    rmt_encoder_handle_t copy  = enc->copy_encoder;
    rmt_encode_state_t session = RMT_ENCODING_RESET;
    rmt_encode_state_t state   = RMT_ENCODING_RESET;
    size_t encoded = 0;

    switch (enc->state) {
    case 0:  // pixel data
        encoded += bytes->encode(bytes, channel, data, data_size, &session);
        if (session & RMT_ENCODING_COMPLETE) enc->state = 1;
        if (session & RMT_ENCODING_MEM_FULL) { state |= RMT_ENCODING_MEM_FULL; goto out; }
    // fall-through
    case 1:  // reset latch
        encoded += copy->encode(copy, channel, &enc->reset_code, sizeof(enc->reset_code), &session);
        if (session & RMT_ENCODING_COMPLETE) { enc->state = 0; state |= RMT_ENCODING_COMPLETE; }
        if (session & RMT_ENCODING_MEM_FULL) { state |= RMT_ENCODING_MEM_FULL; goto out; }
    }
out:
    *ret_state = state;
    return encoded;
}

static esp_err_t ws_reset(rmt_encoder_t *encoder) {
    ws_encoder_t *enc = __containerof(encoder, ws_encoder_t, base);
    rmt_encoder_reset(enc->bytes_encoder);
    rmt_encoder_reset(enc->copy_encoder);
    enc->state = 0;
    return ESP_OK;
}

static esp_err_t ws_del(rmt_encoder_t *encoder) {
    ws_encoder_t *enc = __containerof(encoder, ws_encoder_t, base);
    rmt_del_encoder(enc->bytes_encoder);
    rmt_del_encoder(enc->copy_encoder);
    free(enc);
    return ESP_OK;
}

static esp_err_t ws_encoder_new(rmt_encoder_handle_t *out) {
    ws_encoder_t *enc = calloc(1, sizeof(ws_encoder_t));
    if (!enc) return ESP_ERR_NO_MEM;

    enc->base.encode = ws_encode;
    enc->base.reset  = ws_reset;
    enc->base.del    = ws_del;

    rmt_bytes_encoder_config_t bytes_cfg = {
        .bit0 = { .level0 = 1, .duration0 = 3, .level1 = 0, .duration1 = 9 },  // 0.3 us / 0.9 us
        .bit1 = { .level0 = 1, .duration0 = 9, .level1 = 0, .duration1 = 3 },  // 0.9 us / 0.3 us
        .flags.msb_first = 1,                                                  // WS2812 is MSB-first
    };
    if (rmt_new_bytes_encoder(&bytes_cfg, &enc->bytes_encoder) != ESP_OK) { free(enc); return ESP_FAIL; }

    rmt_copy_encoder_config_t copy_cfg = {};
    if (rmt_new_copy_encoder(&copy_cfg, &enc->copy_encoder) != ESP_OK) {
        rmt_del_encoder(enc->bytes_encoder); free(enc); return ESP_FAIL;
    }

    // >50 us low latch (250 + 250 ticks @ 0.1 us = 50 us, split into two symbols).
    enc->reset_code = (rmt_symbol_word_t){ .level0 = 0, .duration0 = 250, .level1 = 0, .duration1 = 250 };

    *out = &enc->base;
    return ESP_OK;
}

// ── strip state ─────────────────────────────────────────────────────────────
static rmt_channel_handle_t s_chan    = NULL;
static rmt_encoder_handle_t s_encoder = NULL;
static uint8_t             *s_buf     = NULL;   // GRB, 3 bytes per pixel
static int                  s_count   = 0;
static uint8_t             s_bright   = 255;

bool strip_init(int gpio, int count) {
    if (count <= 0) return false;
    s_buf = calloc(count, 3);
    if (!s_buf) { ESP_LOGE(TAG, "out of memory for %d pixels", count); return false; }
    s_count = count;

    rmt_tx_channel_config_t tx_cfg = {
        .clk_src           = RMT_CLK_SRC_DEFAULT,
        .gpio_num          = gpio,
        .mem_block_symbols = 64,
        .resolution_hz     = RMT_RES_HZ,
        .trans_queue_depth = 4,
    };
    if (rmt_new_tx_channel(&tx_cfg, &s_chan) != ESP_OK) { ESP_LOGE(TAG, "rmt channel failed"); return false; }
    if (ws_encoder_new(&s_encoder) != ESP_OK)           { ESP_LOGE(TAG, "encoder failed");     return false; }
    if (rmt_enable(s_chan) != ESP_OK)                   { ESP_LOGE(TAG, "rmt enable failed");  return false; }

    strip_show();   // latch all-off
    return true;
}

void strip_set_brightness(uint8_t b) { s_bright = b; }

void strip_set_pixel(int i, uint8_t r, uint8_t g, uint8_t b) {
    if (!s_buf || i < 0 || i >= s_count) return;
    r = (uint16_t)r * s_bright / 255;
    g = (uint16_t)g * s_bright / 255;
    b = (uint16_t)b * s_bright / 255;
    s_buf[i * 3 + 0] = g;   // WS2812 byte order is GRB
    s_buf[i * 3 + 1] = r;
    s_buf[i * 3 + 2] = b;
}

void strip_fill(uint8_t r, uint8_t g, uint8_t b) {
    for (int i = 0; i < s_count; i++) strip_set_pixel(i, r, g, b);
}

void strip_clear(void) {
    if (s_buf) memset(s_buf, 0, s_count * 3);
}

void strip_show(void) {
    if (!s_chan || !s_buf) return;
    rmt_transmit_config_t tx = { .loop_count = 0 };
    if (rmt_transmit(s_chan, s_encoder, s_buf, s_count * 3, &tx) == ESP_OK) {
        rmt_tx_wait_all_done(s_chan, portMAX_DELAY);
    }
}

int strip_count(void) { return s_count; }
