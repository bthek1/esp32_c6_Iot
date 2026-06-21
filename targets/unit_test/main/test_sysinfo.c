// lib/sysinfo — on-chip temperature + uptime/heap/resource JSON.
//
// Runs against the real C6 temperature sensor and live system counters.

#include "unity.h"
#include "sysinfo.h"

#include <string.h>

static bool s_inited = false;

static void ensure_init(void) {
    if (!s_inited) { sysinfo_init(); s_inited = true; }
}

static void test_temp_in_range(void) {
    ensure_init();
    float t = sysinfo_temp_c();
    // The sensor is configured for -10..80 C; allow margin. 0.0 means the read
    // failed (sensor unavailable), which we want to catch.
    TEST_ASSERT_TRUE_MESSAGE(t > 0.0f && t < 100.0f, "temperature out of plausible range");
}

static void test_stats_json_well_formed(void) {
    ensure_init();
    char buf[160];
    int n = sysinfo_stats_json(buf, sizeof(buf));

    TEST_ASSERT_GREATER_THAN_INT(0, n);
    TEST_ASSERT_LESS_THAN_INT((int)sizeof(buf), n);   // fit without truncation
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"uptime\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"heap\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"min_heap\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"temp\""));
    TEST_ASSERT_EQUAL_CHAR('{', buf[0]);
    TEST_ASSERT_EQUAL_CHAR('}', buf[n - 1]);
}

static void test_resources_json_well_formed(void) {
    ensure_init();
    char buf[256];
    int n = sysinfo_resources_json(buf, sizeof(buf));

    TEST_ASSERT_GREATER_THAN_INT(0, n);
    TEST_ASSERT_LESS_THAN_INT((int)sizeof(buf), n);
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"ram_total\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"ram_free\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"flash_total\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"tasks\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"cpu_mhz\""));
}

static void test_stats_json_truncation_safe(void) {
    // snprintf must never overflow a too-small buffer: the result stays
    // NUL-terminated within the buffer even though the full JSON won't fit.
    char small[10];
    sysinfo_stats_json(small, sizeof(small));
    // NUL-terminated strictly inside the buffer => no overflow.
    TEST_ASSERT_LESS_THAN_UINT(sizeof(small), strlen(small));
}

void run_sysinfo_tests(void) {
    RUN_TEST(test_temp_in_range);
    RUN_TEST(test_stats_json_well_formed);
    RUN_TEST(test_resources_json_well_formed);
    RUN_TEST(test_stats_json_truncation_safe);
}
