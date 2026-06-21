# esp32_c6_Iot — Claude Code Project Context

## Project

IoT firmware for the **ESP32-C6** built with **ESP-IDF** (CMake + `idf.py`), written in C.
Multi-target layout with shared libraries under `lib/`, modelled on the sibling `pico-servo` project.

## Board

**Seeed Studio XIAO ESP32-C6** (not an Espressif DevKitC-1).

- **MCU**: ESP32-C6 — single-core 32-bit RISC-V, Wi-Fi 6 (2.4 GHz), BLE 5, 802.15.4
- **Target id**: `esp32c6` (`CONFIG_IDF_TARGET="esp32c6"` in `sdkconfig.defaults`)
- **Console**: built-in **USB-Serial-JTAG** — enumerates as `/dev/ttyACM0`, no external UART bridge
- **Onboard LEDs**:
  - **User LED — plain on/off LED on GPIO15** (also MTDO/JTAG + a strapping pin; fine as an
    output after boot). Driven by `lib/led` (plain `gpio_set_level`). This is the one to blink.
  - **Charge LED (red)** — hardwired to the battery charger, **not** software-controllable.
  - There is **no** addressable WS2812 / RGB LED on this board. (The DevKitC-1's WS2812-on-GPIO8
    does not apply here — that earlier assumption was wrong.)

## Host Machine

The ESP32-C6 is connected to a **Raspberry Pi** (the flash/monitor machine).

- SSH access: `ssh pi` (username: `bthek1`)
- Serial port on Pi: `/dev/ttyACM0`
- **Compile runs locally** (this machine, ESP-IDF vendored at `lib/esp-idf`); flash and monitor go via `ssh pi`
- ESP-IDF's Python is provided by a **uv venv** at `~/.idf-uv` (keeps it off system / PlatformIO Python).
  Load the toolchain with `source ~/.idf-uv/bin/activate && . lib/esp-idf/export.sh`. Toolchains
  install to `~/.espressif` (machine-global); only the IDF source is vendored in-repo.
- The Pi does **not** need ESP-IDF — only `esptool` (`pip install esptool`) and the device plugged in
- Flashing copies a single merged `firmware.bin` to the Pi and runs `esptool write_flash 0x0`

All terminal commands run locally unless noted otherwise.

## Project Structure

```
esp32_c6_Iot/
├── compile.sh              ← build all, or one target: ./compile.sh [target]
├── flash.sh                ← merge image + flash via Pi (or local): ./flash.sh [--remote] [target]
├── justfile                ← just commands: deploy, compile, flash, monitor, test, test-device
├── sdkconfig.defaults      ← shared IDF config (target chip, console, flash)
├── secrets.h               ← Wi-Fi creds + DEFAULT_TARGET (gitignored)
├── secrets.h.example       ← template (committed)
├── lib/                    ← ESP-IDF + shared libraries (each is an IDF component)
│   ├── esp-idf/            ← ESP-IDF v5.3 (git submodule; IDF_PATH points here)
│   ├── led/                ← plain on/off GPIO LED (active-high or -low) + blink mode
│   ├── serial/             ← printf-over-console wrapper
│   ├── servo/              ← hobby-servo PWM via LEDC
│   ├── strip/              ← WS2812B strip: low-level RMT driver + strip_fx effect engine
│   ├── sysinfo/            ← on-chip temperature + uptime/heap stats JSON
│   ├── web/                ← esp_http_server helpers (body/form parsing, routes, responses)
│   └── wifi/               ← Wi-Fi station connect
├── test/                   ← host-side tests (run on the dev machine, no board)
│   └── host/               ← plain gcc + vendored Unity; lib/web parser tests
└── targets/                ← one standalone IDF *project* per target
    ├── blink/              ← LED blink demo (DEFAULT_TARGET)
    ├── webserver/          ← Wi-Fi + esp_http_server, embeds index.html (tabbed dashboard)
    └── unit_test/          ← on-device Unity test runner (led/strip/sysinfo/wifi)
                              (a `sweep` servo demo is planned; lib/servo exists, no target yet)
```

Each target is its own ESP-IDF project (its own `CMakeLists.txt` + `main/`). Build output
goes to `build/<target>/` (gitignored). There is **no** single root build — `compile.sh`
loops over `targets/`.

## Build & Flash

```bash
# Run locally. compile.sh/flash.sh auto-source the toolchain if idf.py/esptool
# aren't on PATH, so no manual `. export.sh` is needed (incl. via `just`).
./compile.sh                  # build all targets
./compile.sh webserver        # build one target
./compile.sh --clean webserver
./flash.sh --remote           # merge + scp + flash default target via Pi
./flash.sh --remote webserver # flash a specific target via Pi
./flash.sh webserver          # flash an ESP32-C6 plugged into THIS machine
```

Via justfile:

```bash
just deploy             # compile + flash default + wait for tty + monitor
just deploy webserver   # same, for webserver
just compile webserver  # compile one target
just flash webserver    # flash one target via Pi
just flash-local webserver # flash a target via this machine
just monitor          # ssh pi picocom -b 115200 /dev/ttyACM0
just test             # host-side unit tests (gcc + Unity, no board)
just test-device      # compile + flash + monitor the on-device Unity tests
```

Default target is **`blink`** (from `DEFAULT_TARGET` in `secrets.h`).

## How remote flashing works

ESP-IDF produces several images (bootloader @ 0x0, partition table @ 0x8000, app @ 0x10000)
plus a `build/<target>/flash_args` manifest. `flash.sh`:

1. runs `esptool merge_bin @flash_args` locally → one `firmware.bin` flashable at `0x0`
2. `scp`s it to `pi:~/esp-flash/<target>.bin`
3. `ssh pi esptool --chip esp32c6 -p /dev/ttyACM0 write_flash 0x0 <target>.bin`

This keeps the Pi dependency to just `esptool`. Override via env: `ESP_PI_HOST`, `ESP_PI_PORT`,
`ESP_CHIP`, `ESP_BAUD`, `ESP_PORT` (local).

## IDF & CMake Conventions

- Each target's root `CMakeLists.txt` sets `EXTRA_COMPONENT_DIRS` to the **specific** `lib/<name>`
  dirs it uses (e.g. blink → `../../lib/led ../../lib/serial`) and `SDKCONFIG_DEFAULTS` →
  `../../sdkconfig.defaults`, then `include(.../project.cmake)` + `project(<name>)`.
  Point at the individual lib dirs, **not** at `../../lib` — that parent also holds the `esp-idf`
  submodule, which must not be scanned as a component.
- Shared code is a **component**: `idf_component_register(SRCS ... INCLUDE_DIRS "." REQUIRES ...)`.
- A target's `main/CMakeLists.txt` lists only the components it uses in `REQUIRES`.
- `app_main(void)` is the entry point (not `main()`); it runs as a FreeRTOS task.
- Use `vTaskDelay(pdMS_TO_TICKS(ms))` for delays, not busy-waits.
- `sdkconfig`/`sdkconfig.old` are generated per project and **gitignored** — do not commit them.

## Shared Libraries (`lib/`)

Each is a normal IDF component; a target pulls one in via `EXTRA_COMPONENT_DIRS` + `REQUIRES`.

### `lib/led` — plain on/off GPIO LED (+ blink mode)
`led_init(int gpio, bool active_low)`, then `led_on` / `led_off` / `led_toggle` (no gpio arg —
the pin is stored). `REQUIRES driver`. Push-pull GPIO; pass `active_low=true` for the XIAO
ESP32-C6 user LED on **GPIO15**. Not for addressable LEDs — this board has none.
Blink mode: `led_blink(period_ms)` starts a lazily-created background task toggling the LED;
`led_blink_stop()` / `led_is_blinking()`. `led_on/off/toggle` do **not** stop blink (the task
itself drives `led_toggle`) — callers stop it explicitly.

### `lib/serial` — console
`serial_init()` (unbuffered stdout), `serial_print(fmt,...)`, `serial_println(fmt,...)`.
Thin wrapper over `printf` on the USB-Serial-JTAG console. Also keeps an in-memory ring
buffer of recent lines: `serial_history(out, size)` copies it (used by the webserver to show
the console remotely); `serial_start_input()`/`serial_feed_input(line)` record console input.

### `lib/strip` — WS2812B addressable LED strip (RMT) + effect engine
Two layers in one component:

- **`strip.h` (low level)** — `strip_init(int gpio, int count)` (allocates one RMT TX channel + a
  GRB pixel buffer, latches all-off), `strip_set_pixel(i, r, g, b)`, `strip_fill(r, g, b)`,
  `strip_clear()`, `strip_set_brightness(0–255)` (global scale applied on write), `strip_show()`
  (blocking flush), `strip_count()`. Self-contained WS2812 encoder (bytes-encoder for the 0.3/0.9 µs
  bit timing + copy-encoder for the >50 µs reset latch), 10 MHz RMT resolution, GRB byte order.
- **`strip_fx.h` (high level)** — owns mode/colour/brightness/speed state and a background render
  task. `strip_fx_start()` (after `strip_init`), `strip_fx_set_mode` / `strip_fx_mode_from_name`,
  `strip_fx_set_color` + `strip_fx_get_color`, `strip_fx_set_brightness`, `strip_fx_set_speed`
  (1–10), `strip_fx_set_pixel` (→ CUSTOM, seeded from the current look), `strip_fx_dump_hex`
  (per-LED colour dump for the UI preview), plus `strip_fx_mode_name`.
  Modes: **OFF, SOLID, CUSTOM** (static, redraw on change) and the animated patterns
  **RAINBOW, BREATHE, CHASE, WIPE, SCANNER, TWINKLE, FIRE** (redrawn each frame; per-frame delay
  derives from speed). Animated patterns are tinted by the base colour except RAINBOW and FIRE,
  which generate their own palette; `strip_fx_set_color` updates the tint live without changing
  mode. Allocates `custom`/`view`/`heat` (twinkle+fire scratch) buffers sized from `strip_count()`.
  The webserver target talks only to `strip_fx`, never to raw pixels.

`REQUIRES esp_driver_rmt`. This is for an **external** addressable strip — distinct from the onboard
plain LED, which must **not** use RMT/`led_strip`. Power a long 5 V strip from a separate supply
(≈60 mA/LED at full white) and share ground with the board.

### `lib/web` — esp_http_server helpers
Keeps the webserver's per-endpoint handlers short. `web_get`/`web_post` (route registration),
`web_recv_body` (NUL-terminated body read), `web_form_int` / `web_form_str` (url-decoded) /
`web_form_color` (`#`/`%23`/bare `rrggbb` → `0xRRGGBB`), `web_html_escape`, and
`web_send_html` / `web_send_text` / `web_send_json`. `REQUIRES esp_http_server`.

### `lib/sysinfo` — board telemetry
`sysinfo_init()` (installs + enables the C6 on-chip temperature sensor), `sysinfo_temp_c()`,
`sysinfo_stats_json(out, cap)` → `{"uptime","heap","min_heap","temp"}` (the JSON `/api/stats`
serves), and `sysinfo_resources_json(out, cap)` →
`{"ram_total","ram_used","ram_free","ram_min_free","flash_total","app_part","app_used","tasks","cpu_mhz"}`
(served by `/api/resources`; RAM from `heap_caps` internal-cap totals, flash size via
`esp_flash_get_size`, running app-image size via `esp_image_get_metadata`, task count via
`uxTaskGetNumberOfTasks`, CPU MHz from `CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ`). Hides
`driver/temperature_sensor.h`, `esp_timer`, `esp_system`, `esp_heap_caps`, `esp_flash` and the
OTA/image helpers from the app.
`REQUIRES esp_timer esp_driver_tsens app_update bootloader_support spi_flash`.

### `lib/servo` — hobby-servo PWM (LEDC)
`servo_init(int gpio)`, `servo_set_us(gpio, us)` (clamped 500–2500), `servo_set_deg(gpio, deg)` (0–180).
50 Hz, 14-bit, `LEDC_LOW_SPEED_MODE`, up to `SERVO_MAX_CH` (4) servos on one timer. `REQUIRES driver`.

### `lib/wifi` — Wi-Fi station
`wifi_connect(ssid, pwd)` (blocking, inits NVS/netif/event loop/driver, retries, returns bool),
`wifi_is_connected()`, `wifi_get_ip()`. WPA2-PSK. `REQUIRES esp_wifi esp_netif esp_event nvs_flash`.

## Webserver target

`targets/webserver` connects via `lib/wifi` and serves a single-page **tabbed dashboard**
(`main/index.html`, embedded with `EMBED_FILES`). `logo.svg` (repo root) is embedded too and
served as the favicon. The frontend pulls four libraries from CDNs (so the *viewing browser*
needs internet; the ESP32 only serves the page):

| Layer | Library | Used for |
|---|---|---|
| Styling | Tailwind CSS | `cdn.tailwindcss.com` |
| Polling / requests | htmx 2 | LED buttons, console, blink-slider POSTs |
| Reactive UI | Alpine.js 3 | tabs, live stat cards, slider state |
| Charts | uPlot 1 (+CSS) | live heap + temperature time-series |

The UI has four tabs: **Dashboard** (stat cards + a **Resources** panel — RAM-heap and
firmware/flash usage bars with %, fed by `/api/resources` — + uPlot chart fed by `/api/stats`),
**Control** (LED on/off/toggle + a blink-interval slider), **Strip** (WS2812B colour/brightness/
effect), **Console** (serial log + send).

HTTP endpoints (`main/main.c`, `max_uri_handlers = 24`):

- `GET /` page · `GET /favicon.svg|.ico` logo
- `GET /api/led`, `POST /api/led/{on,off,toggle}` — return an HTML status fragment
- `POST /api/led/blink` (`ms=NNN`, clamped 50–5000) — starts blink mode; on/off/toggle stop it.
  A background `blink_task` toggles the LED at `blink_ms` while `blink_on` is set.
- `GET /api/strip` status fragment · `POST /api/strip/color` (`c=%23rrggbb`, base colour) ·
  `POST /api/strip/brightness` (`v=0–255`) · `POST /api/strip/speed` (`v=1–10`) ·
  `POST /api/strip/mode` (`mode=solid|rainbow|breathe|chase|wipe|scanner|twinkle|fire|off`) ·
  `POST /api/strip/pixel` (`i=N&c=%23rrggbb`, sets one LED → **custom** mode) ·
  `GET /api/strip/pixels` (compact `rrggbb`-per-LED dump of the last-rendered colours, used by
  the UI's live preview). All rendering is in `lib/strip`'s `strip_fx` engine; the handlers just
  set state. Strip data pin is **GPIO2** (`STRIP_PIN`); LED count is `STRIP_COUNT` (`60*5` = 300
  for the 5 m strip).
- `GET /api/serial` (HTML-escaped history) · `POST /api/serial` (`msg=...`, url-decoded, echoed)
- `GET /api/stats` → JSON `{uptime, heap, min_heap, temp}` — uptime via `esp_timer`, heap via
  `esp_system`, `temp` from the C6 on-chip sensor (`driver/temperature_sensor.h`).
- `GET /api/resources` → JSON `{ram_total, ram_used, ram_free, ram_min_free, flash_total,
  app_part, app_used, tasks, cpu_mhz}` — internal-heap totals, flash chip + app-partition fill
  (actual running image size), and task count, for the Dashboard's Resources usage bars.

`main/main.c` is kept **thin**: each handler parses its request and calls a library
(`strip_fx_*`, `led_*`, `sysinfo_*`) via the `lib/web` helpers, then returns a small HTML/JSON
fragment. The low-level work lives in the components. `main/CMakeLists.txt` `REQUIRES` is
`wifi serial led strip web sysinfo esp_http_server` (the `esp_timer` / `esp_driver_tsens` deps
moved into `lib/sysinfo`).

## Secrets

`secrets.h` (gitignored) at the repo root defines:

```c
#define WIFI_SSID      "..."
#define WIFI_PASSWORD  "..."
#define DEFAULT_TARGET "blink"
```

`compile.sh`/`flash.sh` read `DEFAULT_TARGET` from it. A target that needs creds adds the repo
root to its include path (see `targets/webserver/main/CMakeLists.txt`) and `#include "secrets.h"`.

## Testing

Two layers, both using **Unity** (vendored in `lib/esp-idf/components/unity`).

### Host tests — `test/host/` (`just test`)
Pure-logic library code compiled and run **on the dev machine** with plain `gcc` +
Unity. No ESP-IDF, no board — fast feedback. Currently covers `lib/web`'s form
parsers (`web_form_color` / `web_form_int` / `web_form_str` / `web_html_escape`).

`lib/web` declares `REQUIRES esp_http_server`, but those parsers never call the
server, so `test/host/stubs/esp_http_server.h` (+ `stubs.c`) is a minimal compile/link
stand-in — **not** a behavioural mock. Build is a hand-written `Makefile`; add a suite
by dropping `test_<name>.c` and extending its `TESTS`. Unity needs `setUp`/`tearDown`
defined (empty is fine) and 64-bit asserts are off on the host build too — match the
device. See `test/host/README.md`.

### On-device tests — `targets/unit_test/` (`just test-device`)
A normal IDF target that exercises **real peripherals** (GPIO, RMT, temp sensor) and a
live Wi-Fi connect, printing standard Unity output (CI-drivable via `pytest-embedded`).
One suite file per lib under `main/` (`test_led/strip/sysinfo/wifi.c`), each exposing a
`run_<lib>_tests()` that `main.c` calls between `UNITY_BEGIN()`/`UNITY_END()`. Gotchas
baked into the current suites:

- **64-bit Unity asserts are disabled** in this IDF build — use `TEST_ASSERT_*_INT`,
  not `*_INT64` (or you get `Unity 64-bit Support Disabled`).
- **`lib/strip`** has no deinit and the C6 has few RMT channels → init the strip **once**
  and share it; the show-completes test times the blocking flush to prove RMT ran.
- **`lib/wifi`** `wifi_connect()` inits NVS/netif/event-loop and aborts if run twice → the
  wifi suite runs **last** and connects once (ignored, not failed, if `secrets.h` still
  has placeholder creds). Needs `secrets.h` on the include path, like `webserver`.
- The runner **idles** after `UNITY_END()` so results aren't lost to a reboot loop.

All 14 on-device cases pass on hardware. See `targets/unit_test/README.md`.

> ⚠️ `just test-device` ends in `picocom`, which holds `/dev/ttyACM0`. A later `flash.sh`
> then fails with "port busy" until you exit picocom (`C-a C-x`) or `ssh pi fuser -k /dev/ttyACM0`.

## What to Avoid

- Committing `sdkconfig`, `sdkconfig.old`, `build/`, `firmware.bin`, or `secrets.h`.
- A single root `CMakeLists.txt` building all targets — ESP-IDF projects build one app each.
- `main()` — the entry point is `app_main(void)`.
- Busy-wait loops — use `vTaskDelay`.
- Assuming a WS2812/RGB *onboard* LED — this board (XIAO ESP32-C6) has a **plain** LED on GPIO15.
  Do not pull in `led_strip`/RMT for it. (That only applies to the Espressif DevKitC-1.) An
  **external** WS2812B strip is a different story — it does use RMT, via `lib/strip` on GPIO2.
- Pointing `EXTRA_COMPONENT_DIRS` at `../../lib` — it contains the `esp-idf` submodule; list the
  specific `lib/<name>` component dirs instead.
- Assuming the bootloader is at 0x1000 — on the C6 (RISC-V) it is at **0x0**; the merged image
  is flashed at `0x0`.

## Status

✅ `blink` builds, flashes via the Pi, and runs on hardware (verified on a **Seeed Studio
XIAO ESP32-C6**). It toggles the **plain user LED on GPIO15** (active-low) via `lib/led`;
the LED visibly blinks. The red charge LED is hardwired to the charger and not controllable.

`webserver` builds clean (~11% app-partition free) and serves the tabbed dashboard (Tailwind +
htmx + Alpine.js + uPlot) with LED control, an adjustable blink mode, a **WS2812B strip** tab
(9 patterns — solid/rainbow/breathe/chase/wipe/scanner/twinkle/fire/off — plus base colour,
brightness, speed, a live per-LED preview, and single-LED editing via `lib/strip` on GPIO2),
the serial console, and a live `/api/stats` telemetry chart (heap + on-chip temperature).
It runs on hardware and drives a real **5 m WS2812B** strip on GPIO2 (LEDs light up; `STRIP_COUNT`
is `60*5` = 300). The animated patterns/single-LED editing are wired end-to-end but not every
pattern has been eyeballed on the strip yet. No `sweep` target exists yet (only `lib/servo`).

`compile.sh`/`flash.sh` auto-source the ESP-IDF toolchain
(`~/.idf-uv/bin/activate` + `lib/esp-idf/export.sh`) when `idf.py`/`esptool`
aren't on `PATH`, so `just deploy` works from a cold shell. Opt out with
`NO_IDF_AUTOSOURCE=1`.

Testing is in place (see the **Testing** section). Host tests (`just test`, `lib/web`
parsers, 16 cases) pass on the dev machine; the on-device `unit_test` target
(`just test-device`, `led`/`strip`/`sysinfo`/`wifi`, 14 cases) builds (~18% app-partition
free) and **all 14 pass on hardware** — including a live Wi-Fi connect.
