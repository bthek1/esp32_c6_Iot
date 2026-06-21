#include "wifi.h"
#include "serial.h"
#include "led.h"
#include "strip.h"
#include "strip_fx.h"
#include "sysinfo.h"
#include "web.h"
#include "secrets.h"

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_http_server.h"

// Seeed Studio XIAO ESP32-C6 user LED (plain, active-low) — see lib/led.
#define LED_PIN     15

// External 5 V WS2812B strip: DIN on STRIP_PIN, STRIP_COUNT LEDs (5 m @ 60/m).
// All strip behaviour (modes, animation, per-pixel buffer) lives in lib/strip.
#define STRIP_PIN   2
#define STRIP_COUNT (60 * 5)

// index.html + logo.svg are embedded into the firmware (see CMakeLists.txt).
extern const char index_html_start[] asm("_binary_index_html_start");
extern const char index_html_end[]   asm("_binary_index_html_end");
extern const char logo_svg_start[]   asm("_binary_logo_svg_start");
extern const char logo_svg_end[]     asm("_binary_logo_svg_end");

// ── page ──────────────────────────────────────────────────────────────────────
static esp_err_t root_handler(httpd_req_t *req) {
    return web_send_html(req, index_html_start, index_html_end - index_html_start);
}

static esp_err_t favicon_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "image/svg+xml");
    return httpd_resp_send(req, logo_svg_start, logo_svg_end - logo_svg_start);
}

// ── user LED ───────────────────────────────────────────────────────────────────
static esp_err_t send_led_status(httpd_req_t *req) {
    bool on = led_is_on(), blinking = led_is_blinking();
    const char *label = blinking ? "BLINK" : (on ? "ON" : "OFF");
    const char *dot   = blinking
        ? "bg-amber-400 shadow-[0_0_12px_2px] shadow-amber-400/70 animate-pulse"
        : (on ? "bg-emerald-400 shadow-[0_0_12px_2px] shadow-emerald-400/70" : "bg-slate-600");
    char buf[320];
    int n = snprintf(buf, sizeof(buf),
        "<span class=\"inline-block w-4 h-4 rounded-full %s\"></span>"
        "<span class=\"text-xl font-semibold\">%s</span>", dot, label);
    return web_send_html(req, buf, n);
}

static esp_err_t led_status_handler(httpd_req_t *req) { return send_led_status(req); }

static esp_err_t led_on_handler(httpd_req_t *req) {
    led_blink_stop(); led_on();  serial_println("[web] led on");  return send_led_status(req);
}
static esp_err_t led_off_handler(httpd_req_t *req) {
    led_blink_stop(); led_off(); serial_println("[web] led off"); return send_led_status(req);
}
static esp_err_t led_toggle_handler(httpd_req_t *req) {
    led_blink_stop(); led_toggle();
    serial_println("[web] led toggle -> %s", led_is_on() ? "on" : "off");
    return send_led_status(req);
}
static esp_err_t led_blink_handler(httpd_req_t *req) {
    char body[64];
    web_recv_body(req, body, sizeof(body));
    int ms = web_form_int(body, "ms", 500);
    if (ms < 50)   ms = 50;
    if (ms > 5000) ms = 5000;
    led_blink(ms);
    serial_println("[web] led blink %d ms", ms);
    return send_led_status(req);
}

// ── WS2812B strip ──────────────────────────────────────────────────────────────
static esp_err_t send_strip_status(httpd_req_t *req) {
    uint8_t r, g, b;
    strip_fx_get_color(&r, &g, &b);
    char buf[320];
    int n = snprintf(buf, sizeof(buf),
        "<span class=\"inline-block w-4 h-4 rounded-full\" "
        "style=\"background:#%02x%02x%02x;box-shadow:0 0 12px 2px #%02x%02x%02x\"></span>"
        "<span class=\"text-xl font-semibold\">%s</span>",
        r, g, b, r, g, b, strip_fx_mode_name());
    return web_send_html(req, buf, n);
}

static esp_err_t strip_status_handler(httpd_req_t *req) { return send_strip_status(req); }

static esp_err_t strip_color_handler(httpd_req_t *req) {
    char body[64];
    web_recv_body(req, body, sizeof(body));
    long col = web_form_color(body, "c");
    if (col >= 0) {
        strip_fx_set_color((col >> 16) & 0xff, (col >> 8) & 0xff, col & 0xff);
        if (strip_fx_mode() == STRIP_FX_OFF) strip_fx_set_mode(STRIP_FX_SOLID);
        serial_println("[web] strip color #%06lx", col);
    }
    return send_strip_status(req);
}

static esp_err_t strip_speed_handler(httpd_req_t *req) {
    char body[32];
    web_recv_body(req, body, sizeof(body));
    int v = web_form_int(body, "v", strip_fx_speed());
    strip_fx_set_speed(v);
    serial_println("[web] strip speed %d", strip_fx_speed());
    return send_strip_status(req);
}

static esp_err_t strip_brightness_handler(httpd_req_t *req) {
    char body[32];
    web_recv_body(req, body, sizeof(body));
    int v = web_form_int(body, "v", strip_fx_brightness());
    if (v < 0)   v = 0;
    if (v > 255) v = 255;
    strip_fx_set_brightness((uint8_t)v);
    serial_println("[web] strip brightness %d", v);
    return send_strip_status(req);
}

static esp_err_t strip_mode_handler(httpd_req_t *req) {
    char body[32], mode[16];
    web_recv_body(req, body, sizeof(body));
    web_form_str(body, "mode", mode, sizeof(mode));
    strip_fx_set_mode(strip_fx_mode_from_name(mode));
    serial_println("[web] strip mode %s", strip_fx_mode_name());
    return send_strip_status(req);
}

static esp_err_t strip_pixel_handler(httpd_req_t *req) {
    char body[64];
    web_recv_body(req, body, sizeof(body));
    int  i   = web_form_int(body, "i", -1);
    long col = web_form_color(body, "c");
    if (i < 0 || i >= STRIP_COUNT || col < 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad pixel");
        return ESP_FAIL;
    }
    strip_fx_set_pixel(i, (col >> 16) & 0xff, (col >> 8) & 0xff, col & 0xff);
    serial_println("[web] strip pixel %d #%06lx", i, col);
    return send_strip_status(req);
}

static esp_err_t strip_pixels_handler(httpd_req_t *req) {
    static char out[STRIP_COUNT * 6 + 1];
    int n = strip_fx_dump_hex(out, sizeof(out));
    return web_send_text(req, out, n);
}

// ── serial console ─────────────────────────────────────────────────────────────
static esp_err_t send_serial_log(httpd_req_t *req) {
    static char raw[2048];
    static char esc[3072];
    serial_history(raw, sizeof(raw));
    int n = web_html_escape(raw, esc, sizeof(esc));
    return web_send_html(req, esc, n);
}

static esp_err_t serial_get_handler(httpd_req_t *req) { return send_serial_log(req); }

static esp_err_t serial_post_handler(httpd_req_t *req) {
    char body[256], msg[256];
    web_recv_body(req, body, sizeof(body));
    web_form_str(body, "msg", msg, sizeof(msg));
    if (msg[0]) serial_println("[web] %s", msg);
    return send_serial_log(req);
}

// ── stats (JSON) ───────────────────────────────────────────────────────────────
static esp_err_t stats_handler(httpd_req_t *req) {
    char buf[192];
    int n = sysinfo_stats_json(buf, sizeof(buf));
    return web_send_json(req, buf, n);
}

// ── resources (JSON) ─────────────────────────────────────────────────────────────
static esp_err_t resources_handler(httpd_req_t *req) {
    char buf[256];
    int n = sysinfo_resources_json(buf, sizeof(buf));
    return web_send_json(req, buf, n);
}

// ── server ─────────────────────────────────────────────────────────────────────
static void start_server(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 24;
    httpd_handle_t s = NULL;
    if (httpd_start(&s, &config) != ESP_OK) return;

    web_get (s, "/",                    root_handler);
    web_get (s, "/favicon.ico",         favicon_handler);
    web_get (s, "/favicon.svg",         favicon_handler);

    web_get (s, "/api/led",             led_status_handler);
    web_post(s, "/api/led/on",          led_on_handler);
    web_post(s, "/api/led/off",         led_off_handler);
    web_post(s, "/api/led/toggle",      led_toggle_handler);
    web_post(s, "/api/led/blink",       led_blink_handler);

    web_get (s, "/api/strip",           strip_status_handler);
    web_post(s, "/api/strip/color",     strip_color_handler);
    web_post(s, "/api/strip/brightness",strip_brightness_handler);
    web_post(s, "/api/strip/mode",      strip_mode_handler);
    web_post(s, "/api/strip/speed",     strip_speed_handler);
    web_post(s, "/api/strip/pixel",     strip_pixel_handler);
    web_get (s, "/api/strip/pixels",    strip_pixels_handler);

    web_get (s, "/api/serial",          serial_get_handler);
    web_post(s, "/api/serial",          serial_post_handler);
    web_get (s, "/api/stats",           stats_handler);
    web_get (s, "/api/resources",       resources_handler);

    serial_println("http server up");
}

void app_main(void) {
    serial_init();
    serial_start_input();        // capture console input into history
    led_init(LED_PIN, true);     // active-low user LED
    sysinfo_init();              // on-chip temperature sensor

    if (strip_init(STRIP_PIN, STRIP_COUNT)) strip_fx_start();
    else                                    serial_println("strip init failed");

    serial_println("webserver start");

    if (!wifi_connect(WIFI_SSID, WIFI_PASSWORD)) {
        serial_println("wifi failed");
        return;
    }
    serial_print("connected, ip = ");
    serial_println(wifi_get_ip());

    start_server();

    while (true) vTaskDelay(pdMS_TO_TICKS(1000));
}
