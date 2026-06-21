#pragma once

// Board telemetry: on-chip temperature plus uptime / heap, packaged as the
// JSON the dashboard's chart polls. Hides the temperature-sensor driver and the
// esp_timer / esp_system calls from the application.

void  sysinfo_init(void);                       // install + enable the temp sensor
float sysinfo_temp_c(void);                      // last on-chip temperature, °C (0 if unavailable)
int   sysinfo_stats_json(char *out, int cap);    // {"uptime","heap","min_heap","temp"} → length

// Resource inventory + live usage (RAM heap, flash/app partition, task count),
// for the dashboard's "Resources" cards. All sizes in bytes.
// {"ram_total","ram_used","ram_free","ram_min_free",
//  "flash_total","app_part","app_used","tasks","cpu_mhz"} → length
int   sysinfo_resources_json(char *out, int cap);
