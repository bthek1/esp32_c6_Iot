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
├── justfile                ← just commands: deploy, compile, flash, monitor
├── sdkconfig.defaults      ← shared IDF config (target chip, console, flash)
├── secrets.h               ← Wi-Fi creds + DEFAULT_TARGET (gitignored)
├── secrets.h.example       ← template (committed)
├── lib/                    ← ESP-IDF + shared libraries (each is an IDF component)
│   ├── esp-idf/            ← ESP-IDF v5.3 (git submodule; IDF_PATH points here)
│   ├── led/                ← plain on/off GPIO LED (active-high or -low)
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
# Run locally. compile.sh/flash.sh auto-source the toolchain if idf.py/esptool
# aren't on PATH, so no manual `. export.sh` is needed (incl. via `just`).
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

### `lib/led` — plain on/off GPIO LED
`led_init(int gpio, bool active_low)`, then `led_on` / `led_off` / `led_toggle` (no gpio arg —
the pin is stored). `REQUIRES driver`. Push-pull GPIO; pass `active_low=true` for the XIAO
ESP32-C6 user LED on **GPIO15**. Not for addressable LEDs — this board has none.

### `lib/serial` — console
`serial_init()` (unbuffered stdout), `serial_print(fmt,...)`, `serial_println(fmt,...)`.
Thin wrapper over `printf` on the USB-Serial-JTAG console.

### `lib/servo` — hobby-servo PWM (LEDC)
`servo_init(int gpio)`, `servo_set_us(gpio, us)` (clamped 500–2500), `servo_set_deg(gpio, deg)` (0–180).
50 Hz, 14-bit, `LEDC_LOW_SPEED_MODE`, up to `SERVO_MAX_CH` (4) servos on one timer. `REQUIRES driver`.

### `lib/wifi` — Wi-Fi station
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
- Assuming a WS2812/RGB onboard LED — this board (XIAO ESP32-C6) has a **plain** LED on GPIO15.
  Do not pull in `led_strip`/RMT for it. (That only applies to the Espressif DevKitC-1.)
- Pointing `EXTRA_COMPONENT_DIRS` at `../../lib` — it contains the `esp-idf` submodule; list the
  specific `lib/<name>` component dirs instead.
- Assuming the bootloader is at 0x1000 — on the C6 (RISC-V) it is at **0x0**; the merged image
  is flashed at `0x0`.

## Status

✅ `blink` builds, flashes via the Pi, and runs on hardware (verified on a **Seeed Studio
XIAO ESP32-C6**). It toggles the **plain user LED on GPIO15** (active-low) via `lib/led`;
the LED visibly blinks. The red charge LED is hardwired to the charger and not controllable.
`sweep` and `webserver` build clean but are not yet hardware-verified.

`compile.sh`/`flash.sh` auto-source the ESP-IDF toolchain
(`~/.idf-uv/bin/activate` + `lib/esp-idf/export.sh`) when `idf.py`/`esptool`
aren't on `PATH`, so `just deploy` works from a cold shell. Opt out with
`NO_IDF_AUTOSOURCE=1`.
