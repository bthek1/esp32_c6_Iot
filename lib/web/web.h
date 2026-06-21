#pragma once

#include "esp_http_server.h"
#include <stdbool.h>

// Small helpers for the esp_http_server handlers: route registration, reading
// request bodies, pulling fields out of form-encoded bodies, and emitting
// common response types. Keeps the per-endpoint handlers in main.c short.

// Route registration shorthands.
void web_get (httpd_handle_t s, const char *uri, esp_err_t (*h)(httpd_req_t *));
void web_post(httpd_handle_t s, const char *uri, esp_err_t (*h)(httpd_req_t *));

// Read the whole request body into `buf` (NUL-terminated). Returns its length.
int  web_recv_body(httpd_req_t *req, char *buf, int cap);

// Form-field parsing on an "a=1&b=two" body.
//   web_form_int  — integer value of `key`, or `def` if absent.
//   web_form_str  — url-decoded value of `key` into `out` (handles '+'/%xx);
//                   returns false (and copies the whole body decoded) if absent.
//   web_form_color— "#rrggbb", "%23rrggbb" or bare "rrggbb" → 0xRRGGBB, -1 if absent.
int  web_form_int  (const char *body, const char *key, int def);
bool web_form_str  (const char *body, const char *key, char *out, int cap);
long web_form_color(const char *body, const char *key);

// HTML-escape `in` (&,<,>) into `out`; returns the escaped length.
int  web_html_escape(const char *in, char *out, int cap);

// Response shorthands (set Content-Type + send the body).
esp_err_t web_send_html(httpd_req_t *req, const char *body, int len);
esp_err_t web_send_text(httpd_req_t *req, const char *body, int len);
esp_err_t web_send_json(httpd_req_t *req, const char *body, int len);
