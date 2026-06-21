#pragma once

// Each suite lives in its own file and exposes one entry point that RUN_TEST()s
// its cases. main.c calls them between UNITY_BEGIN()/UNITY_END().
//
// Order matters: run_wifi_tests() goes last — it connects to the AP (slow) and
// inits the netif/event loop, which must not happen twice.

void run_led_tests(void);
void run_strip_tests(void);
void run_sysinfo_tests(void);
void run_wifi_tests(void);
