#pragma once

// Thin wrapper over the ESP-IDF console (USB-Serial-JTAG by default).
// Mirrors the pico-servo `serial` API so target code reads the same.

void serial_init(void);                    // unbuffered stdout; call once at start
void serial_print(const char *fmt, ...);   // printf, no trailing newline
void serial_println(const char *fmt, ...); // printf + "\n"
