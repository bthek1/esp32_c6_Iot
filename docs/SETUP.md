# ESP32-C6 — Project Setup Guide

Setting up an ESP32-C6 / ESP-IDF development environment on Linux, building this
multi-target project, and flashing it through a Raspberry Pi.

---

## Target Board

| | |
|---|---|
| **Board** | Seeed Studio XIAO ESP32-C6 |
| **MCU** | ESP32-C6 (single-core RISC-V) |
| **Target id** | `esp32c6` |
| **Console** | built-in USB-Serial-JTAG → `/dev/ttyACM0` |
| **User LED** | plain LED on **GPIO15**, active-low (driven by `lib/led`) |
| **Charge LED** (red) | hardwired to the charger — **not** software-controllable |
| **Onboard RGB** | none (no WS2812 on this board) |
| **Bootloader offset** | `0x0` (RISC-V parts), app at `0x10000` |

---

## External WS2812B LED strip (webserver target)

The `webserver` target drives an **external 5 V WS2812B** strip (`lib/strip` + `strip_fx`, RMT on
**GPIO2**). This is separate from the onboard LED — the board has no addressable RGB of its own.

Firmware knobs in [`targets/webserver/main/main.c`](../targets/webserver/main/main.c):

```c
#define STRIP_PIN   2          // D2 on the XIAO — strip DIN
#define STRIP_COUNT (60 * 5)   // LED count (5 m × 60 LED/m = 300); set to your strip
```

Wiring — the strip is powered from its **own 5 V supply**, never the XIAO's 5V pin:

```
  5V supply ──┬── +5V ──► strip +5V        (inject at both ends for a full 5 m run)
              │
  XIAO GND ───┼── GND ──► strip GND        ← common ground is REQUIRED
              │
  XIAO D2 ────┴── [~75–330 Ω] ──► strip DIN  (series resistor on the data line)
```

Rules of thumb:

- **Common ground** between supply, XIAO and strip — the #1 cause of "nothing/garbage" output.
- **Direction matters**: feed DIN at the end the printed arrows point *away* from.
- **Power budget**: ≈60 mA/LED at full white → a 300-LED strip can pull ~18 A. Size the supply for
  your real usage; firmware defaults brightness low (64/255). For long runs the far end dims/reddens
  (voltage drop) — inject +5V/GND at both ends.
- Optional but recommended: a ~330 Ω series resistor on data and a ~1000 µF cap across +5V/GND at
  the strip input. The XIAO's 3.3 V data usually drives WS2812B directly; add a 3.3→5 V level
  shifter only if the first LEDs misbehave.

---

## 1. Install ESP-IDF (this machine — the compiler)

ESP-IDF is **vendored as a git submodule** at `lib/esp-idf` (pinned to `v5.3`), mirroring how
the sibling `pico-servo` project vendors `pico-sdk`. After cloning this repo:

```bash
sudo apt update
sudo apt install -y git wget flex bison gperf libffi-dev libssl-dev \
    dfu-util libusb-1.0-0 picocom
# (cmake / ninja / ccache are provided by ESP-IDF's own tools — system copies optional)

# pull the ESP-IDF submodule (and its internal submodules)
git submodule update --init --recursive

# install the esp32c6 toolchain. Python is provided by a uv-managed venv so it
# stays off the system / PlatformIO Python:
uv venv --python 3.12 ~/.idf-uv
source ~/.idf-uv/bin/activate
./lib/esp-idf/install.sh esp32c6
```

To put `idf.py` on PATH in a shell (for raw `idf.py` use):

```bash
source ~/.idf-uv/bin/activate      # uv Python that ESP-IDF was bootstrapped from
. lib/esp-idf/export.sh            # sets IDF_PATH=<repo>/lib/esp-idf, adds idf.py
```

(Add an alias like `alias get_idf='source ~/.idf-uv/bin/activate && . ~/Documents/Microcontroller/esp32/esp32_c6_Iot/lib/esp-idf/export.sh'` to your shell rc.)

**You usually don't need this**: `compile.sh`/`flash.sh` (and therefore `just`) auto-source the
toolchain when `idf.py`/`esptool` aren't on PATH. Set `NO_IDF_AUTOSOURCE=1` to opt out.

The RISC-V toolchain installs to `~/.espressif` (machine-global, outside the repo); only the
ESP-IDF source itself is vendored in `lib/esp-idf`. `compile.sh` errors out early if `idf.py`
is not found.

---

## 2. Set up the Raspberry Pi (the flasher)

The Pi only needs `esptool` and the board plugged in — **not** full ESP-IDF.

```bash
ssh pi
pip install --user esptool      # or: pipx install esptool
ls /dev/ttyACM0                 # ESP32-C6 USB-Serial-JTAG should appear
```

If the port is permission-denied, add the user to `dialout`:
`sudo usermod -aG dialout $USER` then re-login.

---

## 3. Project Structure

```
esp32_c6_Iot/
├── compile.sh / flash.sh / justfile
├── sdkconfig.defaults          ← shared IDF config (chip, console, flash)
├── secrets.h(.example)         ← Wi-Fi creds + DEFAULT_TARGET
├── lib/                        ← ESP-IDF + shared libraries (each is an IDF component)
│   ├── esp-idf/                ← ESP-IDF v5.3 (git submodule; IDF_PATH points here)
│   └── led/ serial/ servo/ strip/ sysinfo/ web/ wifi/   ← shared, reused across targets
└── targets/                    ← one standalone IDF project each
    ├── blink/  └── webserver/
        ├── CMakeLists.txt      ← project root
        └── main/
            ├── CMakeLists.txt  ← idf_component_register
            └── main.c          ← app_main()
```

Build output lands in `build/<target>/` (gitignored).

---

## 4. Target project CMakeLists.txt

Each target wires in the shared libraries it uses and the repo-wide defaults. List the
**specific** `lib/<name>` dirs — not `../../lib`, which also holds the `esp-idf` submodule:

```cmake
cmake_minimum_required(VERSION 3.16)
set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../../lib/led"
    "${CMAKE_CURRENT_LIST_DIR}/../../lib/serial")
set(SDKCONFIG_DEFAULTS   "${CMAKE_CURRENT_LIST_DIR}/../../sdkconfig.defaults")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(blink)
```

`main/CMakeLists.txt` lists the components used:

```cmake
idf_component_register(SRCS "main.c" INCLUDE_DIRS "." REQUIRES led serial)
```

---

## 5. Build

```bash
./compile.sh              # all targets (auto-sources the toolchain if needed)
./compile.sh webserver    # one target
./compile.sh --clean webserver
```

Or `just compile [target]` / `just compile-clean [target]`.

A successful build leaves `build/<target>/<target>.bin` and a `flash_args` manifest.

---

## 6. Flash through the Pi

```bash
./flash.sh --remote          # default target
./flash.sh --remote webserver # specific target
```

What happens:
1. `esptool merge_bin @flash_args` locally → single `firmware.bin` (flashable at `0x0`)
2. `scp` to `pi:~/esp-flash/<target>.bin`
3. `ssh pi esptool --chip esp32c6 -p /dev/ttyACM0 write_flash 0x0 <target>.bin`

Flash an ESP32-C6 plugged into **this** machine instead:

```bash
./flash.sh webserver         # local, uses /dev/ttyACM0
```

Override defaults with env vars: `ESP_PI_HOST`, `ESP_PI_PORT`, `ESP_CHIP`, `ESP_BAUD`, `ESP_PORT`.

---

## 7. Serial Monitor

```bash
just monitor
# = ssh pi "picocom -b 115200 /dev/ttyACM0"
```

Exit picocom with `Ctrl+A` then `Ctrl+X`.

---

## 8. Secrets (Wi-Fi)

```bash
cp secrets.h.example secrets.h
# edit WIFI_SSID / WIFI_PASSWORD / DEFAULT_TARGET
```

A target that needs creds adds the repo root to its include path
(`target_include_directories(${COMPONENT_LIB} PRIVATE "${CMAKE_SOURCE_DIR}/../..")`)
then `#include "secrets.h"`. See `targets/webserver`.

---

## 9. Adding a New Target

1. `mkdir -p targets/<name>/main`
2. Copy a sibling's `CMakeLists.txt` (change `project(<name>)`) and `main/CMakeLists.txt`
   (set the `REQUIRES`).
3. Write `targets/<name>/main/main.c` with `void app_main(void)`.
4. `./compile.sh <name>` then `./flash.sh --remote <name>`.

No root file to edit — `compile.sh` discovers targets by listing `targets/`.

---

## Quick Reference

| Task | Command |
|---|---|
| Load toolchain | `source ~/.idf-uv/bin/activate && . lib/esp-idf/export.sh` |
| Build all | `./compile.sh` |
| Build one | `./compile.sh <target>` |
| Clean build | `./compile.sh --clean <target>` |
| Flash via Pi | `./flash.sh --remote <target>` |
| Flash locally | `./flash.sh <target>` |
| Deploy (build+flash+monitor) | `just deploy <target>` |
| Serial monitor | `just monitor` |
