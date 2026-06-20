# Plan: ESP32-C6 → Matter on/off light → Samsung SmartThings

Turn the **Seeed Studio XIAO ESP32-C6** into a Matter device that pairs directly with the
**SmartThings phone app** (no SmartThings hub required) and exposes the onboard user LED
(**GPIO15**, active-low) as a controllable on/off light.

- **Integration path:** Matter **over Wi-Fi** (the C6 also supports Thread, but Wi-Fi
  commissions straight from the phone with no Thread Border Router).
- **Why Matter:** SmartThings is a certified Matter controller. A Matter device shows up as a
  native tile and *also* works in Alexa / Google Home / Apple Home — one firmware, every ecosystem.
- **Device model:** Matter **On/Off Light** (endpoint), whose attribute-change callback drives
  `lib/led` (`led_on` / `led_off`).

> ⚠️ **Scope note.** This is a large addition. `esp-matter` pulls in the upstream
> **connectedhomeip** SDK (several GB of submodules) and the first build is long (tens of
> minutes). The Matter app does **not** fit the default partition layout — it needs a custom
> partition table and a larger app partition. Budget time for the toolchain reinstall and the
> first clean build.

---

## 0. Prerequisites / decisions (already made)

| Decision        | Choice                                            |
| --------------- | ------------------------------------------------- |
| Path            | Matter over **Wi-Fi**                             |
| Device type     | **On/off light** on **GPIO15** (reuses `lib/led`) |
| ESP-IDF version | Bump submodule **v5.3 → v5.5.4**                  |
| esp-matter      | `main` (tracks IDF v5.5.4)                         |
| SmartThings hub | **Not required** for Matter-over-Wi-Fi            |

You will need:
- The XIAO ESP32-C6 on the Pi (`/dev/ttyACM0`) as today.
- A **2.4 GHz** Wi-Fi network the phone is also on (Matter commissioning is 2.4 GHz; the C6 is
  2.4 GHz-only anyway).
- The **SmartThings app** on a phone (used to scan the device's QR / setup code).

---

## 1. Bump ESP-IDF v5.3 → v5.5.4

esp-matter pins to a specific IDF release; a mismatch is the #1 build-failure cause.

```bash
cd lib/esp-idf
git fetch --tags --depth 1 origin v5.5.4
git checkout v5.5.4
git submodule update --init --recursive       # IDF's own submodules
cd ../..
# update the pin in .gitmodules (branch = v5.5.4) and commit the submodule bump
```

Reinstall the toolchain for the new version (installs to `~/.espressif`, machine-global):

```bash
source ~/.idf-uv/bin/activate
./lib/esp-idf/install.sh esp32c6
. ./lib/esp-idf/export.sh
```

**Verify the existing targets still build on v5.5.4** before adding Matter — this isolates the
IDF bump from the Matter work:

```bash
./compile.sh blink
./compile.sh webserver
```

- [ ] `lib/esp-idf` checked out at v5.5.4, submodules updated
- [ ] `.gitmodules` branch updated to `v5.5.4`, submodule bump committed
- [ ] toolchain reinstalled (`install.sh esp32c6`)
- [ ] `blink` + `webserver` still build green

---

## 2. Add esp-matter under `lib/`

`esp-matter` is its own component tree **plus** the connectedhomeip submodule. Add it as a
submodule alongside `esp-idf` (it must **not** be scanned as a component dir — same rule as
`esp-idf`).

```bash
git submodule add https://github.com/espressif/esp-matter.git lib/esp-matter
cd lib/esp-matter
git submodule update --init --depth 1
cd connectedhomeip/connectedhomeip
./scripts/checkout_submodules.py --platform esp32 --shallow   # large, slow
cd ../../../..
```

Matter needs its own environment exported (in addition to IDF). Two env vars matter:

```bash
export ESP_MATTER_PATH="$PWD/lib/esp-matter"
. "$ESP_MATTER_PATH/export.sh"                 # sets up connectedhomeip tooling
```

- [ ] `lib/esp-matter` submodule added
- [ ] connectedhomeip submodules checked out (`--platform esp32`)
- [ ] `esp-matter/export.sh` sources cleanly after IDF export

> **Toolchain note.** `compile.sh`/`flash.sh` currently auto-source only the IDF toolchain.
> They'll need to also export `ESP_MATTER_PATH` + `esp-matter/export.sh` **for the matter
> target only** (see §6). Keep it conditional so `blink`/`webserver` don't pay the cost.

---

## 3. New target: `targets/matter_light/`

Mirror the `blink` project layout. The component dirs include `lib/led` (reused) and the
esp-matter component trees.

```
targets/matter_light/
├── CMakeLists.txt
├── sdkconfig.defaults          ← Matter-specific overlay (on top of the shared one)
├── partitions.csv              ← custom table with a large factory app partition
└── main/
    ├── CMakeLists.txt
    └── main.c                  ← Matter node setup + on/off callback → lib/led
```

### `targets/matter_light/CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../../lib/led"
    "${CMAKE_CURRENT_LIST_DIR}/../../lib/serial"
    "$ENV{ESP_MATTER_PATH}/components"
    "$ENV{ESP_MATTER_PATH}/connectedhomeip/connectedhomeip/config/esp32/components")

# Shared defaults first, then this target's Matter overlay on top.
set(SDKCONFIG_DEFAULTS
    "${CMAKE_CURRENT_LIST_DIR}/../../sdkconfig.defaults;${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(matter_light)
```

### `targets/matter_light/main/CMakeLists.txt`

```cmake
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES led serial esp_matter chip)
```

### `targets/matter_light/partitions.csv` (Matter needs room)

```csv
# Name,    Type, SubType, Offset,   Size
nvs,       data, nvs,     0x9000,   0x6000
otadata,   data, ota,     0xf000,   0x2000
phy_init,  data, phy,     0x11000,  0x1000
factory,   app,  factory, 0x20000,  0x1E0000
fctry,     data, nvs,     0x200000, 0x6000
```
(Exact sizes finalised against the build; the point is a **~2 MB app partition** + a factory
NVS (`fctry`) partition for Matter's commissionable data.)

- [ ] target dir scaffolded mirroring `blink`
- [ ] CMake points at the **specific** lib + esp-matter component dirs (never at `../../lib`)
- [ ] custom `partitions.csv` with large factory app + `fctry` NVS

---

## 4. `sdkconfig.defaults` overlay for Matter

Key options the Matter app needs on top of the shared defaults:

```ini
# Use the custom partition table
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"

# Commission over BLE (phone talks BLE first, then hands off Wi-Fi creds)
CONFIG_ENABLE_CHIPOBLE=y
CONFIG_BT_ENABLED=y
CONFIG_BT_NIMBLE_ENABLED=y

# Matter on Wi-Fi (not Thread)
CONFIG_ENABLE_WIFI_STATION=y
CONFIG_ENABLE_WIFI_AP=n
CONFIG_OPENTHREAD_ENABLED=n

# Bigger main task stack + event loop for the Matter stack
CONFIG_ESP_MAIN_TASK_STACK_SIZE=4096
CONFIG_ESP_SYSTEM_EVENT_TASK_STACK_SIZE=4096

# Matter factory data / DAC provider (test creds for development)
CONFIG_FACTORY_PARTITION_DAC_PROVIDER=y    # or use test attestation for first bring-up
```

> Wi-Fi creds: with Matter, the **phone delivers the Wi-Fi SSID/password during commissioning**
> over BLE — so you do **not** hardcode `secrets.h` creds for the radio here. The device joins
> the network the phone tells it to. `lib/wifi` is not used for this target.

- [ ] partition table selected
- [ ] BLE-commissioning (CHIPOBLE) enabled
- [ ] Wi-Fi station enabled, Thread disabled

---

## 5. `main/main.c` — Matter node + on/off → `lib/led`

Skeleton (esp-matter C++ API; file compiles as C++ — name it `main.cpp` if needed and adjust
`REQUIRES`/SRCS):

```cpp
#include "led.h"
#include "serial.h"
#include <esp_matter.h>
#include <esp_matter_endpoint.h>

using namespace esp_matter;
using namespace esp_matter::endpoint;

#define LED_PIN 15   // XIAO ESP32-C6 user LED, active-low

// Called by the Matter stack whenever an attribute changes (e.g. On/Off cluster).
static esp_err_t attribute_update_cb(attribute::callback_type_t type,
                                     uint16_t endpoint_id, uint32_t cluster_id,
                                     uint32_t attribute_id, esp_matter_attr_val_t *val,
                                     void *priv) {
    if (type == attribute::PRE_UPDATE &&
        cluster_id == chip::app::Clusters::OnOff::Id &&
        attribute_id == chip::app::Clusters::OnOff::Attributes::OnOff::Id) {
        val->val.b ? led_on() : led_off();
        serial_println("matter on/off -> %d", val->val.b);
    }
    return ESP_OK;
}

extern "C" void app_main(void) {
    serial_init();
    led_init(LED_PIN, true);     // active-low

    // Create the Matter node + an On/Off Light endpoint.
    node::config_t node_cfg;
    node_t *node = node::create(&node_cfg, attribute_update_cb, NULL);

    on_off_light::config_t light_cfg;
    endpoint::create(node, &light_cfg, ENDPOINT_FLAG_NONE, NULL);

    // Start the Matter stack — prints the commissioning QR / setup code on the console.
    esp_matter::start(NULL);
}
```

At boot the console prints the **setup payload** (manual code + QR URL). That is the
pairing credential for SmartThings.

- [ ] node + on/off-light endpoint created
- [ ] attribute callback drives `led_on`/`led_off`
- [ ] `esp_matter::start()` called; QR/setup code prints on USB-Serial-JTAG console

---

## 6. Build & flash (wire into existing scripts)

`compile.sh`/`flash.sh` loop over `targets/`. Add a **conditional** Matter export so only this
target sources `esp-matter/export.sh` (keeps `blink`/`webserver` light):

```bash
# inside the per-target build, when target == matter_light:
export ESP_MATTER_PATH="$REPO_ROOT/lib/esp-matter"
. "$ESP_MATTER_PATH/export.sh"
```

Then:

```bash
./compile.sh matter_light        # first build is long (connectedhomeip compiles)
./flash.sh --remote matter_light # merge + scp + esptool write_flash 0x0 via Pi
just monitor                     # ssh pi picocom -b 115200 /dev/ttyACM0  → read the QR/code
```

> **Merged-image caveat.** `flash.sh` builds one `firmware.bin` from `flash_args` and flashes
> at `0x0`. Confirm the Matter `fctry`/factory-NVS partitions are included in `flash_args` so
> the merged image carries commissioning data. If not, flash those partitions separately or
> extend the merge step.

- [ ] `matter_light` compiles (after the long first build)
- [ ] flashes via Pi at `0x0`
- [ ] monitor shows the Matter setup QR / manual pairing code

---

## 7. Commission into SmartThings

1. Power the C6; open the serial monitor and note the **QR URL** / **manual setup code**.
2. SmartThings app → **Add device** → **Scan QR code** (or **Enter setup code** /
   **Partner devices → Matter**).
3. The phone connects over **BLE**, asks which **2.4 GHz Wi-Fi** to join, and pushes the
   credentials to the device.
4. Device joins Wi-Fi, finishes Matter commissioning, and appears as an **on/off light** tile.
5. Toggle the tile → **GPIO15 LED turns on/off**. 🎉

- [ ] device commissioned via SmartThings app (no hub)
- [ ] toggling the tile drives the GPIO15 LED
- [ ] (bonus) same device also pairs into Alexa/Google/Apple Home via Matter multi-admin

---

## 8. Update docs & repo hygiene

- [ ] `CLAUDE.md`: add `matter_light` to the target list + a short "Matter" section
- [ ] `secrets.h.example`: note that Matter-over-Wi-Fi does **not** use `WIFI_*` (phone supplies
      creds during commissioning)
- [ ] `.gitignore`: ensure connectedhomeip build artifacts under `lib/esp-matter` are ignored
- [ ] do **not** commit `sdkconfig`, `build/`, generated Matter factory binaries
- [ ] memory: record esp-matter ↔ IDF v5.5.4 pinning so future bumps stay aligned

---

## Risks & gotchas

- **IDF/esp-matter version drift** — keep them pinned together; never bump one alone.
- **First build is huge** — connectedhomeip compiles a lot; expect tens of minutes and several GB.
- **Partition sizing** — default table is too small; the Matter app needs ~2 MB. The board is
  4 MB flash, which is enough but leaves limited OTA room (single-app `factory` layout here).
- **2.4 GHz only** — fine (C6 is 2.4 GHz), but the *phone* must reach a 2.4 GHz SSID to relay.
- **Test vs production DAC** — first bring-up uses test attestation creds; real product
  certification (Matter DAC/PAI) is a separate, later concern.
- **C++ in main** — esp-matter is C++; keep `app_main` `extern "C"`. `lib/led` stays C and is
  called directly.

---

## Sources
- [espressif/esp-matter (GitHub)](https://github.com/espressif/esp-matter)
- [esp-matter — Developing with the SDK (ESP32-C6)](https://docs.espressif.com/projects/esp-matter/en/latest/esp32c6/developing.html)
- [Seeed Studio Wiki — Matter Development with XIAO ESP32](https://wiki.seeedstudio.com/xiao_esp32_matter_env/)
- [ESP-IDF & ESP32-C6 workshop](https://developer.espressif.com/workshops/esp-idf-with-esp32-c6/introduction/)
