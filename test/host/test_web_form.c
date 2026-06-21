// Host-side unit tests for lib/web's form-parsing helpers.
//
// These run on the build machine (plain gcc + Unity) with no ESP32 — see
// test/host/Makefile. They cover pure string logic only; the route/response
// helpers that need a live HTTP server are not exercised here.

#include "unity.h"
#include "web.h"

#include <string.h>

void setUp(void)    {}   // required by Unity (run before each test)
void tearDown(void) {}   // required by Unity (run after each test)

// ── web_form_color ─────────────────────────────────────────────────────────

void test_color_hash_prefix(void) {
    TEST_ASSERT_EQUAL_HEX32(0xff8800, web_form_color("c=#ff8800", "c"));
}

void test_color_url_encoded_hash(void) {
    TEST_ASSERT_EQUAL_HEX32(0x00ff00, web_form_color("c=%2300ff00", "c"));
}

void test_color_bare_hex(void) {
    TEST_ASSERT_EQUAL_HEX32(0x123abc, web_form_color("c=123abc", "c"));
}

void test_color_masks_to_24_bits(void) {
    // Anything above 0xffffff is masked down to the low 24 bits.
    TEST_ASSERT_EQUAL_HEX32(0xffffff, web_form_color("c=ffffffff", "c"));
}

void test_color_missing_key_returns_neg1(void) {
    TEST_ASSERT_EQUAL_INT(-1, web_form_color("x=ff0000", "c"));
}

void test_color_non_hex_returns_neg1(void) {
    TEST_ASSERT_EQUAL_INT(-1, web_form_color("c=#zzzzzz", "c"));
}

void test_color_picks_right_field(void) {
    TEST_ASSERT_EQUAL_HEX32(0x0000ff, web_form_color("a=ff0000&c=0000ff", "c"));
}

// ── web_form_int ───────────────────────────────────────────────────────────

void test_int_basic(void) {
    TEST_ASSERT_EQUAL_INT(255, web_form_int("v=255", "v", 0));
}

void test_int_default_when_absent(void) {
    TEST_ASSERT_EQUAL_INT(42, web_form_int("x=1", "v", 42));
}

void test_int_amongst_fields(void) {
    TEST_ASSERT_EQUAL_INT(7, web_form_int("mode=fire&v=7&speed=3", "v", 0));
}

// ── web_form_str (url-decoding) ────────────────────────────────────────────

void test_str_plus_is_space(void) {
    char out[32];
    TEST_ASSERT_TRUE(web_form_str("msg=hello+world", "msg", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("hello world", out);
}

void test_str_percent_escape(void) {
    char out[32];
    web_form_str("msg=a%2Bb", "msg", out, sizeof(out));   // %2B -> '+'
    TEST_ASSERT_EQUAL_STRING("a+b", out);
}

void test_str_stops_at_ampersand(void) {
    char out[32];
    web_form_str("msg=hi&next=x", "msg", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("hi", out);
}

void test_str_missing_returns_false(void) {
    char out[32];
    TEST_ASSERT_FALSE(web_form_str("other=1", "msg", out, sizeof(out)));
}

// ── web_html_escape ────────────────────────────────────────────────────────

void test_escape_entities(void) {
    char out[64];
    int n = web_html_escape("<a> & </a>", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("&lt;a&gt; &amp; &lt;/a&gt;", out);
    TEST_ASSERT_EQUAL_INT((int)strlen(out), n);
}

void test_escape_plain_passthrough(void) {
    char out[64];
    web_html_escape("no entities here", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("no entities here", out);
}

// ── runner ─────────────────────────────────────────────────────────────────

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_color_hash_prefix);
    RUN_TEST(test_color_url_encoded_hash);
    RUN_TEST(test_color_bare_hex);
    RUN_TEST(test_color_masks_to_24_bits);
    RUN_TEST(test_color_missing_key_returns_neg1);
    RUN_TEST(test_color_non_hex_returns_neg1);
    RUN_TEST(test_color_picks_right_field);

    RUN_TEST(test_int_basic);
    RUN_TEST(test_int_default_when_absent);
    RUN_TEST(test_int_amongst_fields);

    RUN_TEST(test_str_plus_is_space);
    RUN_TEST(test_str_percent_escape);
    RUN_TEST(test_str_stops_at_ampersand);
    RUN_TEST(test_str_missing_returns_false);

    RUN_TEST(test_escape_entities);
    RUN_TEST(test_escape_plain_passthrough);

    return UNITY_END();
}
