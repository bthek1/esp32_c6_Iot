// lib/wifi — Wi-Fi station connect.
//
// This is a genuine integration test: it joins the AP from secrets.h, so it
// needs valid WIFI_SSID/WIFI_PASSWORD and the network in range. If the creds
// are still the placeholder, the connect case is ignored rather than failed.
//
// wifi_connect() initialises NVS/netif/event-loop and must run exactly once, so
// this suite is invoked last and the connect case is the only caller.

#include "unity.h"
#include "wifi.h"
#include "secrets.h"

#include <string.h>

static bool is_placeholder(const char *ssid) {
    return ssid[0] == '\0' || strcmp(ssid, "...") == 0;
}

// Must run before the connect case: nothing has connected yet.
static void test_wifi_initial_state(void) {
    TEST_ASSERT_FALSE(wifi_is_connected());
    TEST_ASSERT_EQUAL_STRING("0.0.0.0", wifi_get_ip());
}

static void test_wifi_connect(void) {
    if (is_placeholder(WIFI_SSID)) {
        TEST_IGNORE_MESSAGE("set real WIFI_SSID/WIFI_PASSWORD in secrets.h to run this");
    }

    bool ok = wifi_connect(WIFI_SSID, WIFI_PASSWORD);
    TEST_ASSERT_TRUE_MESSAGE(ok, "wifi_connect failed — check credentials / AP in range");
    TEST_ASSERT_TRUE(wifi_is_connected());
    TEST_ASSERT_TRUE_MESSAGE(strcmp(wifi_get_ip(), "0.0.0.0") != 0, "no IP after connect");
}

void run_wifi_tests(void) {
    RUN_TEST(test_wifi_initial_state);
    RUN_TEST(test_wifi_connect);
}
