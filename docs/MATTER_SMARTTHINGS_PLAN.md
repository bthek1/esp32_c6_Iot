# Plan: ESP32-C6 → Matter color light → Samsung SmartThings

Turn the **Seeed Studio XIAO ESP32-C6** into a Matter device that exposes the external
**WS2812B 300-LED strip** (DIN on **GPIO2**, driven by `lib/strip` + `strip_fx`) as a controllable
**colour light** (on/off + brightness + colour), usable from SmartThings / Apple / Google / Alexa /
chip-tool / Home Assistant.

> ⚠️ **Correction:** an earlier draft said "no hub required" — that's wrong. The *phone* only
> **commissions** the device; ongoing control needs an always-on **Matter controller**. SmartThings
> requires a Samsung hub (Station / hub / 2022+ TV / Family Hub / compatible Galaxy). For a hubless
> setup, control it with **chip-tool** or **Home Assistant's Matter Server** (e.g. on the Pi).
> This device has been **verified working with chip-tool** (commission + on/off/brightness/colour).

- **Integration path:** Matter **over Wi-Fi** (the C6 also supports Thread, but Wi-Fi
  commissions straight from the phone with no Thread Border Router).
- **Why Matter:** SmartThings is a certified Matter controller. A Matter device shows up as a
  native tile and *also* works in Alexa / Google Home / Apple Home — one firmware, every ecosystem.
- **Device model:** Matter **Extended Color Light** (endpoint), whose On/Off, Level Control and
  Color Control attribute-change callbacks drive `strip_fx` (`strip_fx_set_mode` /
  `strip_fx_set_brightness` / `strip_fx_set_color`). One uniform-colour facet of the strip is what
  Matter controls; the richer animated patterns stay available through the existing web UI (both
  go through `strip_fx`, the single owner of strip state).

> ✅ **Status: implemented and building.** ESP-IDF is bumped to **v5.5.4**, `lib/esp-matter`
> (+ connectedhomeip) is set up, and **`targets/matter_strip` builds clean** (`./compile.sh
> matter_strip`, app ~`0x1a54d0`, ~12% free in the 2 MB OTA partition). Not yet flashed /
> commissioned on hardware. Real-world corrections vs. the original plan below:
> - Root `CMakeLists.txt` needs the **full esp-matter example boilerplate** (sets `MATTER_SDK_PATH`,
>   includes the `esp32c6_devkit_c` device-HAL cmake, and adds the matter component dirs) — the
>   minimal blink-style CMake fails with an empty `MATTER_SDK_PATH`.
> - `sdkconfig.defaults` + `partitions.csv` are the **esp-matter light-example pair** (they enable
>   `CONFIG_MBEDTLS_HKDF_C`, lwIP hooks, a dual-OTA + `fctry` table, and disable unused clusters).
>   The minimal overlay link-fails on `mbedtls_hkdf`.
> - `main.cpp` uses **`extended_color_light::create(node, &cfg, flags, priv)`** (per-device-type),
>   not the generic `endpoint::create`. The shared C headers got `extern "C"` guards so the C++
>   TU links against the C libs.
> - **Colour needed an extra feature.** esp-matter's `extended_color_light` enables colour-temp + XY
>   but **not** Hue/Saturation, so `CurrentHue`/`CurrentSaturation` were `UNSUPPORTED_ATTRIBUTE` and
>   the HSV callback never fired. `main.cpp` adds it after `create()`:
>   `cluster::color_control::feature::hue_saturation::add(...)`. Verified: `move-to-hue-and-saturation`
>   → ColorMode 0, Hue/Sat update, strip recolours.
> - **chip-tool host build deps:** `libssl-dev libdbus-1-dev libglib2.0-dev libavahi-client-dev
>   libevent-dev` + `checkout_submodules.py --platform linux`, then
>   `gn_build_example.sh examples/chip-tool ./out/host`.
> - The toolchain Python env must be created with the **system** python3.12, not from inside the
>   `~/.idf-uv` venv (`idf_tools.py install-python-env` refuses to nest venvs).
> - `compile.sh` sources `esp-matter/export.sh` automatically for `matter*` targets, and now
>   checks `${PIPESTATUS[0]}` so a failed `idf.py` is no longer masked by the log pipe.

> ⚠️ **Scope note.** This is a large addition. `esp-matter` pulls in the upstream
> **connectedhomeip** SDK (several GB of submodules) and the first build is long (tens of
> minutes). The Matter app does **not** fit the default partition layout — it needs a custom
> partition table and a larger app partition. Budget time for the toolchain reinstall and the
> first clean build.

---

## 0. Prerequisites / decisions (already made)

| Decision        | Choice                                                          |
| --------------- | -------------------------------------------------------------- |
| Path            | Matter over **Wi-Fi**                                          |
| Device type     | **Extended color light** driving the WS2812B strip on **GPIO2** (reuses `lib/strip` + `strip_fx`) |
| ESP-IDF version | Bump submodule **v5.3 → v5.5.4**                              |
| esp-matter      | `main` (tracks IDF v5.5.4)                                     |
| SmartThings hub | **Not required** for Matter-over-Wi-Fi                        |

You will need:
- The XIAO ESP32-C6 on the Pi (`/dev/ttyACM0`) as today.
- The **WS2812B strip wired up** with its **own 5 V supply** and a **common ground** to the board
  (DIN → GPIO2). See the strip wiring section in [SETUP.md](SETUP.md).
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
- [ ] `blink` + `webserver` still build green (so `lib/strip`/`strip_fx` are confirmed on v5.5.4)

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

## 3. New target: `targets/matter_strip/`

Mirror the `blink` project layout. The component dirs include `lib/strip` (reused) and the
esp-matter component trees.

```
targets/matter_strip/
├── CMakeLists.txt
├── sdkconfig.defaults          ← Matter-specific overlay (on top of the shared one)
├── partitions.csv              ← custom table with a large factory app partition
└── main/
    ├── CMakeLists.txt
    └── main.cpp                ← Matter node setup + on/off/level/color callbacks → strip_fx
```

### `targets/matter_strip/CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../../lib/strip"
    "${CMAKE_CURRENT_LIST_DIR}/../../lib/serial"
    "$ENV{ESP_MATTER_PATH}/components"
    "$ENV{ESP_MATTER_PATH}/connectedhomeip/connectedhomeip/config/esp32/components")

# Shared defaults first, then this target's Matter overlay on top.
set(SDKCONFIG_DEFAULTS
    "${CMAKE_CURRENT_LIST_DIR}/../../sdkconfig.defaults;${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(matter_strip)
```

### `targets/matter_strip/main/CMakeLists.txt`

```cmake
# main.cpp because esp-matter is C++; lib/strip stays C and is called directly.
idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES strip serial esp_matter chip)
```

### `targets/matter_strip/partitions.csv` (Matter needs room)

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

> **RMT + BLE/Wi-Fi coexistence.** The strip's RMT TX channel is independent of the radios, so it
> runs fine alongside NimBLE commissioning and the Wi-Fi station. No pin/peripheral conflict with
> GPIO2.

- [ ] partition table selected
- [ ] BLE-commissioning (CHIPOBLE) enabled
- [ ] Wi-Fi station enabled, Thread disabled

---

## 5. `main/main.cpp` — Matter node + on/off/level/color → `strip_fx`

Matter's **Extended Color Light** exposes three clusters that map cleanly onto `strip_fx`:

| Matter cluster | Attribute | strip_fx call |
|---|---|---|
| On/Off | `OnOff` | `strip_fx_set_mode(SOLID / OFF)` |
| Level Control | `CurrentLevel` (0–254) | `strip_fx_set_brightness` (global scale) |
| Color Control | `CurrentHue` / `CurrentSaturation` (0–254) | `strip_fx_set_color` (full-value HSV → RGB) |

Brightness is the global scale; colour is the full-value (V=255) hue/sat, so the two combine the
way the SmartThings light tile expects.

```cpp
#include "strip.h"
#include "strip_fx.h"
#include "serial.h"
#include <esp_matter.h>
#include <esp_matter_endpoint.h>

using namespace esp_matter;
using namespace esp_matter::endpoint;

#define STRIP_PIN   2
#define STRIP_COUNT (60 * 5)    // 5 m × 60 LED/m = 300

// Last hue/sat from the Color Control cluster (Matter units, 0..254).
static uint8_t s_hue = 0, s_sat = 0;

// Full-saturation-aware HSV → RGB (h 0..359, s/v 0..255).
static void hsv_to_rgb(int h, int s, int v, uint8_t *r, uint8_t *g, uint8_t *b) {
    int c = v * s / 255;
    int x = c * (60 - abs((h % 120) - 60)) / 60;
    int m = v - c, rr = 0, gg = 0, bb = 0;
    switch ((h / 60) % 6) {
        case 0: rr = c; gg = x; break;
        case 1: rr = x; gg = c; break;
        case 2: gg = c; bb = x; break;
        case 3: gg = x; bb = c; break;
        case 4: rr = x; bb = c; break;
        default: rr = c; bb = x; break;
    }
    *r = rr + m; *g = gg + m; *b = bb + m;
}

static void apply_color(void) {
    uint8_t r, g, b;
    hsv_to_rgb((int)s_hue * 360 / 254, (int)s_sat * 255 / 254, 255, &r, &g, &b);
    strip_fx_set_color(r, g, b);    // does not change mode; tints solid + patterns
}

// Called by the Matter stack whenever an attribute changes.
static esp_err_t attribute_update_cb(attribute::callback_type_t type,
                                     uint16_t endpoint_id, uint32_t cluster_id,
                                     uint32_t attribute_id, esp_matter_attr_val_t *val,
                                     void *priv) {
    if (type != attribute::PRE_UPDATE) return ESP_OK;
    using namespace chip::app::Clusters;

    if (cluster_id == OnOff::Id && attribute_id == OnOff::Attributes::OnOff::Id) {
        strip_fx_set_mode(val->val.b ? STRIP_FX_SOLID : STRIP_FX_OFF);
        serial_println("matter on/off -> %d", val->val.b);
    } else if (cluster_id == LevelControl::Id &&
               attribute_id == LevelControl::Attributes::CurrentLevel::Id) {
        strip_fx_set_brightness(val->val.u8);          // 0..254 ≈ 0..255 scale
        serial_println("matter level -> %d", val->val.u8);
    } else if (cluster_id == ColorControl::Id) {
        if (attribute_id == ColorControl::Attributes::CurrentHue::Id)        s_hue = val->val.u8;
        else if (attribute_id == ColorControl::Attributes::CurrentSaturation::Id) s_sat = val->val.u8;
        apply_color();
        serial_println("matter color -> h=%d s=%d", s_hue, s_sat);
    }
    return ESP_OK;
}

extern "C" void app_main(void) {
    serial_init();
    strip_init(STRIP_PIN, STRIP_COUNT);
    strip_fx_start();                 // background render task (lib/strip)

    // Create the Matter node + an Extended Color Light endpoint.
    node::config_t node_cfg;
    node_t *node = node::create(&node_cfg, attribute_update_cb, NULL);

    extended_color_light::config_t light_cfg;
    light_cfg.on_off.on_off = false;  // start OFF
    endpoint::create(node, &light_cfg, ENDPOINT_FLAG_NONE, NULL);

    // Start the Matter stack — prints the commissioning QR / setup code on the console.
    esp_matter::start(NULL);
}
```

At boot the console prints the **setup payload** (manual code + QR URL). That is the
pairing credential for SmartThings.

> **Colour mode caveat.** Some controllers drive colour via `CurrentX`/`CurrentY` (CIE xy) instead
> of hue/sat. SmartThings typically uses hue/sat for an Extended Color Light, but if colours don't
> track, also handle the `CurrentX`/`CurrentY` attributes (convert xy → RGB) in the callback.

- [ ] node + extended-color-light endpoint created
- [ ] callback drives `strip_fx` on/off, brightness and colour
- [ ] `esp_matter::start()` called; QR/setup code prints on USB-Serial-JTAG console

---

## 6. Build & flash (wire into existing scripts)

`compile.sh`/`flash.sh` loop over `targets/`. Add a **conditional** Matter export so only this
target sources `esp-matter/export.sh` (keeps `blink`/`webserver` light):

```bash
# inside the per-target build, when target == matter_strip:
export ESP_MATTER_PATH="$REPO_ROOT/lib/esp-matter"
. "$ESP_MATTER_PATH/export.sh"
```

Then:

```bash
./compile.sh matter_strip        # first build is long (connectedhomeip compiles)
./flash.sh --remote matter_strip # merge + scp + esptool write-flash 0x0 via Pi
just monitor                     # ssh pi picocom -b 115200 /dev/ttyACM0  → read the QR/code
```

> **Merged-image caveat.** `flash.sh` builds one `firmware.bin` from `flash_args` and flashes
> at `0x0`. Confirm the Matter `fctry`/factory-NVS partitions are included in `flash_args` so
> the merged image carries commissioning data. If not, flash those partitions separately or
> extend the merge step.

- [ ] `matter_strip` compiles (after the long first build)
- [ ] flashes via Pi at `0x0`
- [ ] monitor shows the Matter setup QR / manual pairing code

---

## 7. Commission into SmartThings

1. Power the C6 **and the strip's 5 V supply**; open the serial monitor and note the **QR URL** /
   **manual setup code**.

   > **Default test credentials.** This build sets `CONFIG_ENABLE_TEST_SETUP_PARAMS=y` (vendor
   > `0xFFF1`, product `0x8000`), so it uses the standard CHIP example commissioning values —
   > you don't have to read them off the console:
   > - **Manual pairing code: `3497-011-2332`** (digits `34970112332`)
   > - **QR payload: `MT:Y.K9042C00KA0648G00`**
   > - discriminator `3840`, passcode `20202021`
   >
   > SmartThings will warn it's an uncertified/test device — accept that for DIY use. Verified on
   > hardware: the device boots from `0x20000`, starts the CHIP Wi-Fi layer, and advertises over
   > BLE (`bleAdv … Start slow advertisement`) ready to commission. Read the live code any time
   > with `just monitor` (press reset to see the boot banner).
2. SmartThings app → **Add device** → **Scan QR code** (or **Enter setup code** /
   **Partner devices → Matter**).
3. The phone connects over **BLE**, asks which **2.4 GHz Wi-Fi** to join, and pushes the
   credentials to the device.
4. Device joins Wi-Fi, finishes Matter commissioning, and appears as a **colour light** tile.
5. From the tile: **toggle** → strip on/off · **brightness slider** → strip brightness ·
   **colour wheel** → strip colour. 🎉

- [ ] device commissioned via SmartThings app (no hub)
- [ ] tile on/off, brightness and colour all drive the WS2812B strip
- [ ] (bonus) same device also pairs into Alexa/Google/Apple Home via Matter multi-admin

---

## 8. Update docs & repo hygiene

- [ ] `CLAUDE.md`: add `matter_strip` to the target list + a short "Matter" section
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
- **C++ in main** — esp-matter is C++; name the file `main.cpp` and keep `app_main` `extern "C"`.
  `lib/strip`/`strip_fx` stay C and are called directly.
- **Strip power** — Matter only powers the board logic; the **strip still needs its own 5 V supply**
  and a common ground (≈60 mA/LED at full white → ~18 A for 300 LEDs at full white). Keep the
  Matter brightness/colour modest unless the supply can take it.
- **Patterns not in standard Matter** — On/Off + Level + Color cover a solid colour light. The
  animated patterns (rainbow/fire/…) aren't standard Matter clusters; expose them later via a
  Matter **Mode Select** cluster, or keep using the web UI for patterns (both share `strip_fx`).

---

## Sources
- [espressif/esp-matter (GitHub)](https://github.com/espressif/esp-matter)
- [esp-matter — Developing with the SDK (ESP32-C6)](https://docs.espressif.com/projects/esp-matter/en/latest/esp32c6/developing.html)
- [esp-matter — light example (extended color light)](https://github.com/espressif/esp-matter/tree/main/examples/light)
- [Seeed Studio Wiki — Matter Development with XIAO ESP32](https://wiki.seeedstudio.com/xiao_esp32_matter_env/)
- [ESP-IDF & ESP32-C6 workshop](https://developer.espressif.com/workshops/esp-idf-with-esp32-c6/introduction/)
