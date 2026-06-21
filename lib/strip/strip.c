#include "strip.h"

#include <stdlib.h>
#include <string.h>
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "esp_log.h"

static const char *TAG = "strip";

// WS2812 over SPI. The C6's RMT has no DMA, so its bit-stream is corrupted by
// Wi-Fi/BLE interrupt load (e.g. the Matter target). The SPI peripheral clocks
// the data out via DMA — pure hardware timing, immune to interrupt latency.
//
// Each WS2812 bit is encoded as 3 SPI bits at 2.4 MHz (1 SPI bit ≈ 0.417 us):
//   '0' -> 100  (0.42 us high, 0.83 us low)
//   '1' -> 110  (0.83 us high, 0.42 us low)
// So one colour byte (8 bits) -> 24 SPI bits = 3 bytes, and one LED (GRB) -> 9.
#define SPI_HZ        2400000
#define SPI_BYTES_LED 9

static spi_device_handle_t s_spi   = NULL;
static uint8_t            *s_buf   = NULL;   // DMA TX buffer, 9 bytes/LED
static int                 s_count = 0;
static uint8_t             s_bright = 255;

// Expand one colour byte (MSB first) into 3 SPI bytes of 3-bit codes.
static void byte_to_spi(uint8_t v, uint8_t *out) {
    uint32_t bits = 0;
    for (int i = 7; i >= 0; i--) {
        bits <<= 3;
        bits |= (v & (1 << i)) ? 0b110 : 0b100;
    }
    out[0] = (bits >> 16) & 0xff;
    out[1] = (bits >> 8)  & 0xff;
    out[2] =  bits        & 0xff;
}

bool strip_init(int gpio, int count) {
    if (count <= 0) return false;
    s_buf = heap_caps_malloc((size_t)count * SPI_BYTES_LED, MALLOC_CAP_DMA);
    if (!s_buf) { ESP_LOGE(TAG, "out of DMA memory for %d pixels", count); return false; }
    memset(s_buf, 0, (size_t)count * SPI_BYTES_LED);
    s_count = count;

    spi_bus_config_t bus = {
        .mosi_io_num     = gpio,
        .miso_io_num     = -1,
        .sclk_io_num     = -1,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = count * SPI_BYTES_LED,
    };
    if (spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO) != ESP_OK) {
        ESP_LOGE(TAG, "spi bus init failed"); return false;
    }

    spi_device_interface_config_t dev = {
        .clock_speed_hz = SPI_HZ,
        .mode           = 0,
        .spics_io_num   = -1,
        .queue_size     = 1,
        .command_bits   = 0,
        .address_bits   = 0,
    };
    if (spi_bus_add_device(SPI2_HOST, &dev, &s_spi) != ESP_OK) {
        ESP_LOGE(TAG, "spi add device failed"); return false;
    }

    strip_show();   // latch all-off
    return true;
}

void strip_set_brightness(uint8_t b) { s_bright = b; }

void strip_set_pixel(int i, uint8_t r, uint8_t g, uint8_t b) {
    if (!s_buf || i < 0 || i >= s_count) return;
    r = (uint16_t)r * s_bright / 255;
    g = (uint16_t)g * s_bright / 255;
    b = (uint16_t)b * s_bright / 255;
    uint8_t *p = s_buf + (size_t)i * SPI_BYTES_LED;
    byte_to_spi(g, p);       // WS2812 byte order is GRB
    byte_to_spi(r, p + 3);
    byte_to_spi(b, p + 6);
}

void strip_fill(uint8_t r, uint8_t g, uint8_t b) {
    for (int i = 0; i < s_count; i++) strip_set_pixel(i, r, g, b);
}

void strip_clear(void) {
    for (int i = 0; i < s_count; i++) strip_set_pixel(i, 0, 0, 0);
}

void strip_show(void) {
    if (!s_spi || !s_buf) return;
    spi_transaction_t t = {
        .length    = (size_t)s_count * SPI_BYTES_LED * 8,   // in bits
        .tx_buffer = s_buf,
    };
    spi_device_transmit(s_spi, &t);   // blocks until DMA done; >50 us idle latches
}

int strip_count(void) { return s_count; }
