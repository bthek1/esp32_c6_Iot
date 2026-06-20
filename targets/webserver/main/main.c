#include "wifi.h"
#include "serial.h"
#include "led.h"
#include "secrets.h"

#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_http_server.h"

// Seeed Studio XIAO ESP32-C6 user LED (plain, active-low) — see lib/led.
#define LED_PIN 15

// index.html is embedded into the firmware via EMBED_FILES (see CMakeLists.txt).
extern const char index_html_start[] asm("_binary_index_html_start");
extern const char index_html_end[]   asm("_binary_index_html_end");

// ── page ────────────────────────────────────────────────────────────────────
static esp_err_t root_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, index_html_start, index_html_end - index_html_start);
    return ESP_OK;
}

// ── LED ─────────────────────────────────────────────────────────────────────
// Returns an HTML fragment showing the current LED state (htmx swaps it in).
static esp_err_t send_led_status(httpd_req_t *req) {
    bool on = led_is_on();
    char buf[320];
    int n = snprintf(buf, sizeof(buf),
        "<span class=\"inline-block w-4 h-4 rounded-full %s\"></span>"
        "<span class=\"text-xl font-semibold\">%s</span>",
        on ? "bg-emerald-400 shadow-[0_0_12px_2px] shadow-emerald-400/70"
           : "bg-slate-600",
        on ? "ON" : "OFF");
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, buf, n);
    return ESP_OK;
}

static esp_err_t led_status_handler(httpd_req_t *req) { return send_led_status(req); }

static esp_err_t led_on_handler(httpd_req_t *req) {
    led_on();  serial_println("[web] led on");  return send_led_status(req);
}
static esp_err_t led_off_handler(httpd_req_t *req) {
    led_off(); serial_println("[web] led off"); return send_led_status(req);
}
static esp_err_t led_toggle_handler(httpd_req_t *req) {
    led_toggle();
    serial_println("[web] led toggle -> %s", led_is_on() ? "on" : "off");
    return send_led_status(req);
}

// ── serial console ──────────────────────────────────────────────────────────
// Send the console history, HTML-escaped, as the response body.
static esp_err_t send_serial_log(httpd_req_t *req) {
    static char raw[2048];
    static char esc[3072];
    serial_history(raw, sizeof(raw));

    size_t j = 0;
    for (size_t i = 0; raw[i] && j < sizeof(esc) - 6; i++) {
        char c = raw[i];
        if      (c == '&') { memcpy(esc + j, "&amp;", 5); j += 5; }
        else if (c == '<') { memcpy(esc + j, "&lt;",  4); j += 4; }
        else if (c == '>') { memcpy(esc + j, "&gt;",  4); j += 4; }
        else                 esc[j++] = c;
    }
    esc[j] = '\0';

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, esc, j);
    return ESP_OK;
}

static esp_err_t serial_get_handler(httpd_req_t *req) { return send_serial_log(req); }

static int hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// Accept a form-encoded "msg=..." body and echo it to the serial console.
static esp_err_t serial_post_handler(httpd_req_t *req) {
    char body[256];
    int total = req->content_len;
    if (total > (int)sizeof(body) - 1) total = sizeof(body) - 1;
    int recv = 0;
    while (recv < total) {
        int r = httpd_req_recv(req, body + recv, total - recv);
        if (r <= 0) return ESP_FAIL;
        recv += r;
    }
    body[recv] = '\0';

    // strip leading "msg=" field name if present
    const char *msg = body;
    char *eq = strchr(body, '=');
    if (eq) msg = eq + 1;

    // url-decode '+' and %xx into `decoded`
    char decoded[256];
    size_t di = 0;
    for (size_t i = 0; msg[i] && di < sizeof(decoded) - 1; i++) {
        if (msg[i] == '+') {
            decoded[di++] = ' ';
        } else if (msg[i] == '%' && msg[i + 1] && msg[i + 2]) {
            int hi = hexval(msg[i + 1]), lo = hexval(msg[i + 2]);
            if (hi >= 0 && lo >= 0) { decoded[di++] = (char)(hi * 16 + lo); i += 2; }
            else                      decoded[di++] = msg[i];
        } else {
            decoded[di++] = msg[i];
        }
    }
    decoded[di] = '\0';

    if (di) serial_println("[web] %s", decoded);
    return send_serial_log(req);
}

// ── server ──────────────────────────────────────────────────────────────────
static void register_get(httpd_handle_t s, const char *uri, esp_err_t (*h)(httpd_req_t *)) {
    httpd_uri_t u = { .uri = uri, .method = HTTP_GET,  .handler = h };
    httpd_register_uri_handler(s, &u);
}
static void register_post(httpd_handle_t s, const char *uri, esp_err_t (*h)(httpd_req_t *)) {
    httpd_uri_t u = { .uri = uri, .method = HTTP_POST, .handler = h };
    httpd_register_uri_handler(s, &u);
}

static void start_server(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 12;
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK) {
        register_get (server, "/",              root_handler);
        register_get (server, "/api/led",       led_status_handler);
        register_post(server, "/api/led/on",    led_on_handler);
        register_post(server, "/api/led/off",   led_off_handler);
        register_post(server, "/api/led/toggle",led_toggle_handler);
        register_get (server, "/api/serial",    serial_get_handler);
        register_post(server, "/api/serial",    serial_post_handler);
        serial_println("http server up");
    }
}

void app_main(void) {
    serial_init();
    led_init(LED_PIN, true);   // active-low user LED
    serial_println("webserver start");

    if (!wifi_connect(WIFI_SSID, WIFI_PASSWORD)) {
        serial_println("wifi failed");
        return;
    }
    serial_print("connected, ip = ");
    serial_println(wifi_get_ip());

    start_server();

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
