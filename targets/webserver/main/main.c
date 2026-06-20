#include "wifi.h"
#include "serial.h"
#include "secrets.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_http_server.h"

// index.html is embedded into the firmware via EMBED_FILES (see CMakeLists.txt).
extern const char index_html_start[] asm("_binary_index_html_start");
extern const char index_html_end[]   asm("_binary_index_html_end");

static esp_err_t root_handler(httpd_req_t *req) {
    const size_t len = index_html_end - index_html_start;
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, index_html_start, len);
    return ESP_OK;
}

static void start_server(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t root = {
            .uri      = "/",
            .method   = HTTP_GET,
            .handler  = root_handler,
        };
        httpd_register_uri_handler(server, &root);
        serial_println("http server up");
    }
}

void app_main(void) {
    serial_init();
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
