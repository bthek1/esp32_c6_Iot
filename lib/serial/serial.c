#include "serial.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"

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

// Record a line typed on the physical console into the history (history only —
// the input task already echoes keystrokes to the terminal). Prefixed with
// "> " so console input is distinguishable from printed output on the web UI.
void serial_feed_input(const char *line) {
    lock();
    feed("> ");
    feed(line);
    feed("\n");
    unlock();
}

// Reads the USB-Serial-JTAG console byte-by-byte, echoes keystrokes back so the
// typist sees them, and on Enter pushes the completed line into the history ring
// so the webserver (or any serial_history() consumer) can show it.
static void input_task(void *arg) {
    (void)arg;
    char   line[LINE_MAX];
    size_t len = 0;
    uint8_t buf[64];

    for (;;) {
        int n = usb_serial_jtag_read_bytes(buf, sizeof(buf), pdMS_TO_TICKS(100));
        for (int i = 0; i < n; i++) {
            char c = (char)buf[i];
            if (c == '\r' || c == '\n') {
                usb_serial_jtag_write_bytes("\r\n", 2, pdMS_TO_TICKS(20));
                if (len) { line[len] = '\0'; serial_feed_input(line); len = 0; }
            } else if (c == 0x7f || c == 0x08) {       // DEL / backspace
                if (len) { len--; usb_serial_jtag_write_bytes("\b \b", 3, pdMS_TO_TICKS(20)); }
            } else if ((unsigned char)c >= 0x20 && len < sizeof(line) - 1) {
                line[len++] = c;
                usb_serial_jtag_write_bytes(&buf[i], 1, pdMS_TO_TICKS(20));
            }
        }
    }
}

void serial_start_input(void) {
    usb_serial_jtag_driver_config_t cfg = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    if (usb_serial_jtag_driver_install(&cfg) != ESP_OK) return;
    // Route stdout through the driver too, so printf output and the input task's
    // echo share one serialized TX path instead of fighting over the peripheral.
    usb_serial_jtag_vfs_use_driver();
    xTaskCreate(input_task, "serial_in", 4096, NULL, 5, NULL);
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
