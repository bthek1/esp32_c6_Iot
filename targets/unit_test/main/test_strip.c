// lib/strip — WS2812B addressable strip over the RMT TX peripheral.
//
// No physical strip is required: strip_init() brings up an RMT channel + encoder
// and strip_show() clocks the buffer out the data pin regardless of what's
// attached. The strip API exposes no pixel read-back, so we verify what we can
// on-device: that the RMT channel initialises, the count is right, out-of-range
// writes are ignored, and — crucially — that strip_show() actually *completes*
// (it waits portMAX_DELAY for the transfer, so a broken RMT setup would hang;
// reaching the next line, within a time bound, proves the peripheral ran).
//
// strip has no deinit and the C6 has few RMT channels, so we init exactly once
// and share it across cases.

#include "unity.h"
#include "strip.h"

#include "esp_timer.h"

#define STRIP_PIN   2    // same data pin the webserver uses
#define STRIP_COUNT 8    // small test strip; real count is configured per app

static bool s_inited = false;

static void ensure_strip(void) {
    if (!s_inited) {
        TEST_ASSERT_TRUE_MESSAGE(strip_init(STRIP_PIN, STRIP_COUNT),
                                 "strip_init failed (RMT channel/encoder alloc)");
        s_inited = true;
    }
}

static void test_strip_init_count(void) {
    ensure_strip();
    TEST_ASSERT_EQUAL_INT(STRIP_COUNT, strip_count());
}

static void test_strip_init_rejects_bad_count(void) {
    // count <= 0 must fail without touching the live channel.
    TEST_ASSERT_FALSE(strip_init(STRIP_PIN, 0));
}

static void test_strip_show_completes(void) {
    ensure_strip();
    strip_set_brightness(64);
    strip_fill(10, 20, 30);

    int64_t t0 = esp_timer_get_time();
    strip_show();                         // blocks until the RMT transfer is done
    int64_t elapsed = esp_timer_get_time() - t0;

    // 8 px * 24 bits * ~1.2us + 50us reset is well under a millisecond; a 100ms
    // ceiling just proves it returned rather than hung. (Unity here is built
    // without 64-bit asserts; the value easily fits an int.)
    TEST_ASSERT_LESS_THAN_INT(100000, (int)elapsed);

    strip_clear();
    strip_show();
}

static void test_strip_pixel_bounds_ignored(void) {
    ensure_strip();
    // Out-of-range indices must be no-ops (no crash / no OOB write).
    strip_set_pixel(-1, 255, 255, 255);
    strip_set_pixel(STRIP_COUNT, 255, 255, 255);
    strip_set_pixel(STRIP_COUNT + 100, 255, 255, 255);
    strip_show();
    TEST_PASS();
}

void run_strip_tests(void) {
    RUN_TEST(test_strip_init_count);
    RUN_TEST(test_strip_init_rejects_bad_count);
    RUN_TEST(test_strip_show_completes);
    RUN_TEST(test_strip_pixel_bounds_ignored);
}
