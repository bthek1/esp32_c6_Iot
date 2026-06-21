#include "web.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

void web_get(httpd_handle_t s, const char *uri, esp_err_t (*h)(httpd_req_t *)) {
    httpd_uri_t u = { .uri = uri, .method = HTTP_GET, .handler = h };
    httpd_register_uri_handler(s, &u);
}

void web_post(httpd_handle_t s, const char *uri, esp_err_t (*h)(httpd_req_t *)) {
    httpd_uri_t u = { .uri = uri, .method = HTTP_POST, .handler = h };
    httpd_register_uri_handler(s, &u);
}

int web_recv_body(httpd_req_t *req, char *buf, int cap) {
    int total = req->content_len;
    if (total > cap - 1) total = cap - 1;
    int recv = 0;
    while (recv < total) {
        int r = httpd_req_recv(req, buf + recv, total - recv);
        if (r <= 0) break;
        recv += r;
    }
    if (recv < 0) recv = 0;
    buf[recv] = '\0';
    return recv;
}

static int hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// Locate the value of "key=" in a form body, or NULL.
static const char *field(const char *body, const char *key) {
    char pat[24];
    int n = snprintf(pat, sizeof(pat), "%s=", key);
    const char *p = strstr(body, pat);
    return p ? p + n : NULL;
}

int web_form_int(const char *body, const char *key, int def) {
    const char *v = field(body, key);
    return v ? atoi(v) : def;
}

bool web_form_str(const char *body, const char *key, char *out, int cap) {
    const char *v = field(body, key);
    bool found = v != NULL;
    if (!v) v = body;                         // fall back to the whole body
    int di = 0;
    for (int i = 0; v[i] && v[i] != '&' && di < cap - 1; i++) {
        char c = v[i];
        if (c == '+') {
            out[di++] = ' ';
        } else if (c == '%' && v[i + 1] && v[i + 2]) {
            int hi = hexval(v[i + 1]), lo = hexval(v[i + 2]);
            if (hi >= 0 && lo >= 0) { out[di++] = (char)(hi * 16 + lo); i += 2; }
            else                      out[di++] = c;
        } else {
            out[di++] = c;
        }
    }
    out[di] = '\0';
    return found;
}

long web_form_color(const char *body, const char *key) {
    const char *v = field(body, key);
    if (!v) return -1;
    if      (!strncmp(v, "%23", 3)) v += 3;   // url-encoded '#'
    else if (*v == '#')             v += 1;
    char *end;
    long col = strtol(v, &end, 16);
    return end == v ? -1 : (col & 0xffffff);
}

int web_html_escape(const char *in, char *out, int cap) {
    int j = 0;
    for (int i = 0; in[i] && j < cap - 6; i++) {
        char c = in[i];
        if      (c == '&') { memcpy(out + j, "&amp;", 5); j += 5; }
        else if (c == '<') { memcpy(out + j, "&lt;",  4); j += 4; }
        else if (c == '>') { memcpy(out + j, "&gt;",  4); j += 4; }
        else                 out[j++] = c;
    }
    out[j] = '\0';
    return j;
}

static esp_err_t send(httpd_req_t *req, const char *type, const char *body, int len) {
    httpd_resp_set_type(req, type);
    return httpd_resp_send(req, body, len);
}

esp_err_t web_send_html(httpd_req_t *req, const char *body, int len) { return send(req, "text/html",        body, len); }
esp_err_t web_send_text(httpd_req_t *req, const char *body, int len) { return send(req, "text/plain",       body, len); }
esp_err_t web_send_json(httpd_req_t *req, const char *body, int len) { return send(req, "application/json", body, len); }
