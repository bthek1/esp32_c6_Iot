#pragma once

// Minimal host-test stub of esp_http_server.h.
//
// lib/web/web.h pulls in esp_http_server.h for the route/response helpers, but
// the *parsing* functions we unit-test on the host (web_form_int / web_form_str
// / web_form_color / web_html_escape) never touch the HTTP server. This stub
// supplies just enough of the API surface for web.c to compile and link with a
// plain host gcc — the httpd_* functions are defined (as no-ops) in stubs.c so
// the unused response/route helpers still resolve at link time.
//
// It is NOT a behavioural mock: nothing here is exercised by the tests.

#include <stddef.h>
#include <sys/types.h>   // ssize_t

typedef int   esp_err_t;
typedef void *httpd_handle_t;

#define ESP_OK 0

typedef enum { HTTP_GET = 1, HTTP_POST = 3 } httpd_method_t;

typedef struct httpd_req {
    size_t content_len;   // the only field web.c reads (web_recv_body)
} httpd_req_t;

typedef struct httpd_uri {
    const char     *uri;
    httpd_method_t  method;
    esp_err_t     (*handler)(httpd_req_t *r);
    void           *user_ctx;
} httpd_uri_t;

int       httpd_req_recv           (httpd_req_t *r, char *buf, size_t len);
esp_err_t httpd_register_uri_handler(httpd_handle_t h, const httpd_uri_t *u);
esp_err_t httpd_resp_set_type      (httpd_req_t *r, const char *type);
esp_err_t httpd_resp_send          (httpd_req_t *r, const char *buf, ssize_t len);
