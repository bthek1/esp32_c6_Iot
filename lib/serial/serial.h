#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Thin wrapper over the ESP-IDF console (USB-Serial-JTAG by default).
// Mirrors the pico-servo `serial` API so target code reads the same.
// Besides printing, it keeps an in-memory ring buffer of recent lines so a UI
// (e.g. the webserver) can show the console remotely.

void serial_init(void);                    // unbuffered stdout; call once at start
void serial_print(const char *fmt, ...);   // printf, no trailing newline
void serial_println(const char *fmt, ...); // printf + "\n"

// Start a background task that reads the USB-Serial-JTAG console and records
// each line typed there into the history (so a remote UI sees console input).
// Installs the USB-Serial-JTAG driver; call once after serial_init().
void serial_start_input(void);

// Record a single line of console input into the history (history only, no echo).
void serial_feed_input(const char *line);

// Copy the recent console history (oldest first, newline-separated) into `out`.
// Always NUL-terminates; returns bytes written (excluding the terminator).
// Safe to call from another task, e.g. an HTTP handler.
size_t serial_history(char *out, size_t out_size);

#ifdef __cplusplus
}
#endif
