#pragma once

#include <stdbool.h>

// Minimal Wi-Fi station helper for the ESP32-C6.
// Mirrors the pico-servo `wifi` API. Blocking connect with retry + timeout.

// Connect to an AP. Initialises NVS, netif, the event loop and the Wi-Fi
// driver internally. Blocks until connected (got IP) or the attempt fails.
// Returns true on success.
bool wifi_connect(const char *ssid, const char *password);

bool        wifi_is_connected(void);
const char *wifi_get_ip(void);   // "0.0.0.0" until connected
