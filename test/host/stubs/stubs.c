// No-op definitions for the esp_http_server symbols referenced by web.c's
// route/response helpers. The host tests never call these — they exist only so
// web.c links. See stubs/esp_http_server.h.

#include "esp_http_server.h"

int       httpd_req_recv           (httpd_req_t *r, char *buf, size_t len)      { (void)r; (void)buf; (void)len; return 0; }
esp_err_t httpd_register_uri_handler(httpd_handle_t h, const httpd_uri_t *u)    { (void)h; (void)u; return ESP_OK; }
esp_err_t httpd_resp_set_type      (httpd_req_t *r, const char *type)           { (void)r; (void)type; return ESP_OK; }
esp_err_t httpd_resp_send          (httpd_req_t *r, const char *buf, ssize_t l) { (void)r; (void)buf; (void)l; return ESP_OK; }
