#include "serial.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define HIST_LINES 40       // number of recent lines retained
#define LINE_MAX   160      // max chars per stored line

static char   s_lines[HIST_LINES][LINE_MAX];
static int    s_head  = 0;  // next slot to write
static int    s_count = 0;  // valid lines (<= HIST_LINES)
static char   s_cur[LINE_MAX];
static size_t s_cur_len = 0;
static SemaphoreHandle_t s_mtx;

static void lock(void)   { if (s_mtx) xSemaphoreTake(s_mtx, portMAX_DELAY); }
static void unlock(void) { if (s_mtx) xSemaphoreGive(s_mtx); }

static void push_line(void) {
    s_cur[s_cur_len] = '\0';
    memcpy(s_lines[s_head], s_cur, s_cur_len + 1);
    s_head = (s_head + 1) % HIST_LINES;
    if (s_count < HIST_LINES) s_count++;
    s_cur_len = 0;
}

// Accumulate printed text into the line buffer, flushing on each newline.
static void feed(const char *s) {
    for (; *s; s++) {
        if (*s == '\n') {
            push_line();
        } else if (*s != '\r') {
            if (s_cur_len < LINE_MAX - 1) s_cur[s_cur_len++] = *s;
        }
    }
}

void serial_init(void) {
    // Make output appear immediately rather than waiting on line buffering.
    setvbuf(stdout, NULL, _IONBF, 0);
    if (!s_mtx) s_mtx = xSemaphoreCreateMutex();
}

static void emit(const char *fmt, va_list ap, bool newline) {
    char buf[LINE_MAX];
    vsnprintf(buf, sizeof(buf), fmt, ap);
    fputs(buf, stdout);
    if (newline) putchar('\n');
    lock();
    feed(buf);
    if (newline) feed("\n");
    unlock();
}

void serial_print(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt); emit(fmt, ap, false); va_end(ap);
}

void serial_println(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt); emit(fmt, ap, true); va_end(ap);
}

size_t serial_history(char *out, size_t out_size) {
    if (out_size == 0) return 0;
    size_t n = 0;
    lock();
    int start = (s_head - s_count + HIST_LINES) % HIST_LINES;
    for (int i = 0; i < s_count; i++) {
        const char *line = s_lines[(start + i) % HIST_LINES];
        size_t ll = strlen(line);
        if (n + ll + 1 >= out_size) break;     // leave room for '\n' + NUL
        memcpy(out + n, line, ll);
        n += ll;
        out[n++] = '\n';
    }
    out[n] = '\0';
    unlock();
    return n;
}
