#pragma once

#include <stdint.h>

// Hobby-servo PWM driven by the ESP32-C6 LEDC peripheral at 50 Hz.
// Up to SERVO_MAX_CH servos share one LEDC timer, one channel each.
// Pulse range is clamped to 500–2500 us (≈ 0–180°).

#define SERVO_MAX_CH 4

void servo_init(int gpio);                    // attach a servo on this GPIO
void servo_set_us(int gpio, uint32_t us);     // raw pulse width, clamped 500–2500
void servo_set_deg(int gpio, float degrees);  // 0–180° → 500–2500 us
