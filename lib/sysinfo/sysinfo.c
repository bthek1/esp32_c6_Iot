#include "sysinfo.h"

#include <stdio.h>
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_flash.h"
#include "esp_ota_ops.h"
#include "esp_image_format.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/temperature_sensor.h"

static temperature_sensor_handle_t s_temp = NULL;

void sysinfo_init(void) {
    temperature_sensor_config_t cfg = TEMPERATURE_SENSOR_CONFIG_DEFAULT(-10, 80);
    if (temperature_sensor_install(&cfg, &s_temp) == ESP_OK)
        temperature_sensor_enable(s_temp);
}

float sysinfo_temp_c(void) {
    float t = 0.0f;
    if (s_temp) temperature_sensor_get_celsius(s_temp, &t);
    return t;
}

int sysinfo_stats_json(char *out, int cap) {
    long long uptime  = esp_timer_get_time() / 1000000;       // seconds
    unsigned  heap    = (unsigned)esp_get_free_heap_size();
    unsigned  minheap = (unsigned)esp_get_minimum_free_heap_size();
    return snprintf(out, cap,
        "{\"uptime\":%lld,\"heap\":%u,\"min_heap\":%u,\"temp\":%.1f}",
        uptime, heap, minheap, sysinfo_temp_c());
}

// Actual size of the running app image in flash (header + segments), read from
// the image metadata. 0 if it can't be determined (e.g. unverifiable image).
static uint32_t app_image_size(void) {
    const esp_partition_t *p = esp_ota_get_running_partition();
    if (!p) return 0;
    esp_image_metadata_t meta = { 0 };
    const esp_partition_pos_t pos = { .offset = p->address, .size = p->size };
    if (esp_image_get_metadata(&pos, &meta) != ESP_OK) return 0;
    return meta.image_len;
}

int sysinfo_resources_json(char *out, int cap) {
    // RAM: the internal-capable heap region (usable DRAM after static .data/.bss).
    unsigned ram_total = (unsigned)heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
    unsigned ram_free  = (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    unsigned ram_min   = (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
    unsigned ram_used  = ram_total - ram_free;

    // Flash: whole chip, plus the running app partition and how full it is.
    uint32_t flash_total = 0;
    esp_flash_get_size(NULL, &flash_total);
    const esp_partition_t *app = esp_ota_get_running_partition();
    unsigned app_part = app ? app->size : 0;
    unsigned app_used = app_image_size();

    unsigned tasks   = (unsigned)uxTaskGetNumberOfTasks();
    unsigned cpu_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ;

    return snprintf(out, cap,
        "{\"ram_total\":%u,\"ram_used\":%u,\"ram_free\":%u,\"ram_min_free\":%u,"
        "\"flash_total\":%u,\"app_part\":%u,\"app_used\":%u,"
        "\"tasks\":%u,\"cpu_mhz\":%u}",
        ram_total, ram_used, ram_free, ram_min,
        (unsigned)flash_total, app_part, app_used, tasks, cpu_mhz);
}
