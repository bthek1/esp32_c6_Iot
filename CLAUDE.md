# esp32_c6_Iot — Claude Code Project Context

## Project

IoT firmware for the **ESP32-C6** built with **ESP-IDF** (CMake + `idf.py`), written in C.
Multi-target layout with shared `components/`, modelled on the sibling `pico-servo` project.

## Board

- **MCU**: ESP32-C6 — single-core 32-bit RISC-V, Wi-Fi 6 (2.4 GHz), BLE 5, 802.15.4
- **Target id**: `esp32c6` (`CONFIG_IDF_TARGET="esp32c6"` in `sdkconfig.defaults`)
- **Console**: built-in **USB-Serial-JTAG** — enumerates as `/dev/ttyACM0`, no external UART bridge
- **Onboard LED** (DevKitC-1): addressable WS2812 RGB on GPIO8 — **not** a plain GPIO LED.
  `components/led` drives a *plain* on/off GPIO; for the onboard RGB use the `led_strip` component.

## Host Machine

The ESP32-C6 is connected to a **Raspberry Pi** (the flash/monitor machine).

- SSH access: `ssh pi` (username: `bthek1`)
- Serial port on Pi: `/dev/ttyACM0`
- **Compile runs locally** (this machine, needs ESP-IDF); flash and serial monitor go via `ssh pi`
- The Pi does **not** need ESP-IDF — only `esptool` (`pip install esptool`) and the device plugged in
- Flashing copies a single merged `firmware.bin` to the Pi and runs `esptool write_flash 0x0`

All terminal commands run locally unless noted otherwise.

## Project Structure

```
esp32_c6_Iot/
├── compile.sh              ← build all, or one target: ./compile.sh [target]
├── flash.sh                ← merge image + flash via Pi (or local): ./flash.sh [--remote] [target]
├── justfile                ← just commands: deploy, compile, flash, monitor
├── sdkconfig.defaults      ← shared IDF config (target chip, console, flash)
├── secrets.h               ← Wi-Fi creds + DEFAULT_TARGET (gitignored)
├── secrets.h.example       ← template (committed)
├── components/             ← shared IDF components (reused across targets)
│   ├── led/                ← GPIO LED
│   ├── serial/             ← printf-over-console wrapper
│   ├── servo/              ← hobby-servo PWM via LEDC
│   └── wifi/               ← Wi-Fi station connect
└── targets/                ← one standalone IDF *project* per target
    ├── blink/              ← LED blink demo (DEFAULT_TARGET)
    ├── sweep/              ← servo sweep demo
    └── webserver/          ← Wi-Fi + esp_http_server, embeds index.html
```

Each target is its own ESP-IDF project (its own `CMakeLists.txt` + `main/`). Build output
goes to `build/<target>/` (gitignored). There is **no** single root build — `compile.sh`
loops over `targets/`.

## Build & Flash

```bash
# Run locally — first: . $IDF_PATH/export.sh   (puts idf.py on PATH)
./compile.sh              # build all targets
./compile.sh sweep        # build one target
./compile.sh --clean sweep
./flash.sh --remote       # merge + scp + flash default target via Pi
./flash.sh --remote sweep # flash a specific target via Pi
./flash.sh sweep          # flash an ESP32-C6 plugged into THIS machine
```

Via justfile:

```bash
just deploy           # compile + flash default + wait for tty + monitor
just deploy sweep     # same, for sweep
just compile sweep    # compile one target
just flash sweep      # flash one target via Pi
just flash-local sweep# flash a target via this machine
just monitor          # ssh pi picocom -b 115200 /dev/ttyACM0
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

- Each target's root `CMakeLists.txt` sets `EXTRA_COMPONENT_DIRS` → `../../components` and
  `SDKCONFIG_DEFAULTS` → `../../sdkconfig.defaults`, then `include(.../project.cmake)` + `project(<name>)`.
- Shared code is a **component**: `idf_component_register(SRCS ... INCLUDE_DIRS "." REQUIRES ...)`.
- A target's `main/CMakeLists.txt` lists only the components it uses in `REQUIRES`.
- `app_main(void)` is the entry point (not `main()`); it runs as a FreeRTOS task.
- Use `vTaskDelay(pdMS_TO_TICKS(ms))` for delays, not busy-waits.
- `sdkconfig`/`sdkconfig.old` are generated per project and **gitignored** — do not commit them.

## Shared Components

### `components/led` — GPIO LED
`led_init(int gpio)` / `led_on` / `led_off` / `led_toggle`. `REQUIRES driver`.
Plain push-pull GPIO. The DevKitC-1 onboard LED is addressable RGB — wire an external LED, or
swap in `led_strip`.

### `components/serial` — console
`serial_init()` (unbuffered stdout), `serial_print(fmt,...)`, `serial_println(fmt,...)`.
Thin wrapper over `printf` on the USB-Serial-JTAG console.

### `components/servo` — hobby-servo PWM (LEDC)
`servo_init(int gpio)`, `servo_set_us(gpio, us)` (clamped 500–2500), `servo_set_deg(gpio, deg)` (0–180).
50 Hz, 14-bit, `LEDC_LOW_SPEED_MODE`, up to `SERVO_MAX_CH` (4) servos on one timer. `REQUIRES driver`.

### `components/wifi` — Wi-Fi station
`wifi_connect(ssid, pwd)` (blocking, inits NVS/netif/event loop/driver, retries, returns bool),
`wifi_is_connected()`, `wifi_get_ip()`. WPA2-PSK. `REQUIRES esp_wifi esp_netif esp_event nvs_flash`.

## Secrets

`secrets.h` (gitignored) at the repo root defines:

```c
#define WIFI_SSID      "..."
#define WIFI_PASSWORD  "..."
#define DEFAULT_TARGET "blink"
```

`compile.sh`/`flash.sh` read `DEFAULT_TARGET` from it. A target that needs creds adds the repo
root to its include path (see `targets/webserver/main/CMakeLists.txt`) and `#include "secrets.h"`.

## What to Avoid

- Committing `sdkconfig`, `sdkconfig.old`, `build/`, `firmware.bin`, or `secrets.h`.
- A single root `CMakeLists.txt` building all targets — ESP-IDF projects build one app each.
- `main()` — the entry point is `app_main(void)`.
- Busy-wait loops — use `vTaskDelay`.
- Driving the onboard WS2812 RGB with plain `gpio_set_level` — it needs `led_strip`.
- Assuming the bootloader is at 0x1000 — on the C6 (RISC-V) it is at **0x0**; the merged image
  is flashed at `0x0`.

## Status

⚠️ This tree was scaffolded as a template and has **not yet been compiled or flashed**
(no local ESP-IDF / device at setup time). Run `./compile.sh blink` once a toolchain is
available and fix any first-build issues (component `REQUIRES`, pin choices, board LED type).
