#include "servo.h"
#include "driver/ledc.h"

#define SERVO_FREQ_HZ   50
#define SERVO_RES       LEDC_TIMER_14_BIT
#define SERVO_MAX_DUTY  ((1 << 14) - 1)
#define SERVO_PERIOD_US 20000          // 50 Hz → 20 ms
#define SERVO_MIN_US    500
#define SERVO_MAX_US    2500

// ESP32-C6 only has the low-speed LEDC mode.
#define SERVO_MODE      LEDC_LOW_SPEED_MODE
#define SERVO_TIMER     LEDC_TIMER_0

static int  s_gpio[SERVO_MAX_CH];
static int  s_count = 0;
static bool s_timer_ready = false;

static int channel_of(int gpio) {
    for (int i = 0; i < s_count; i++) {
        if (s_gpio[i] == gpio) return i;
    }
    return -1;
}

void servo_init(int gpio) {
    if (channel_of(gpio) >= 0) return;            // already attached
    if (s_count >= SERVO_MAX_CH) return;          // out of channels

    if (!s_timer_ready) {
        ledc_timer_config_t timer = {
            .speed_mode      = SERVO_MODE,
            .duty_resolution = SERVO_RES,
            .timer_num       = SERVO_TIMER,
            .freq_hz         = SERVO_FREQ_HZ,
            .clk_cfg         = LEDC_AUTO_CLK,
        };
        ledc_timer_config(&timer);
        s_timer_ready = true;
    }

    int ch = s_count++;
    s_gpio[ch] = gpio;

    ledc_channel_config_t channel = {
        .gpio_num   = gpio,
        .speed_mode = SERVO_MODE,
        .channel    = ch,
        .timer_sel  = SERVO_TIMER,
        .duty       = 0,
        .hpoint     = 0,
    };
    ledc_channel_config(&channel);
}

void servo_set_us(int gpio, uint32_t us) {
    int ch = channel_of(gpio);
    if (ch < 0) return;

    if (us < SERVO_MIN_US) us = SERVO_MIN_US;
    if (us > SERVO_MAX_US) us = SERVO_MAX_US;

    uint32_t duty = (uint32_t)((uint64_t)us * SERVO_MAX_DUTY / SERVO_PERIOD_US);
    ledc_set_duty(SERVO_MODE, ch, duty);
    ledc_update_duty(SERVO_MODE, ch);
}

void servo_set_deg(int gpio, float degrees) {
    if (degrees < 0.0f)   degrees = 0.0f;
    if (degrees > 180.0f) degrees = 180.0f;
    servo_set_us(gpio, SERVO_MIN_US + (uint32_t)(degrees / 180.0f * (SERVO_MAX_US - SERVO_MIN_US)));
}
