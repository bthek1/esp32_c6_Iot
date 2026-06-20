#include "serial.h"
#include <stdio.h>
#include <stdarg.h>

void serial_init(void) {
    // Make output appear immediately rather than waiting on line buffering.
    setvbuf(stdout, NULL, _IONBF, 0);
}

void serial_print(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

void serial_println(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    putchar('\n');
}
