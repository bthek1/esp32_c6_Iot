# esp32_c6_Iot

IoT firmware for the **ESP32-C6** (ESP-IDF, C). Multi-target layout with shared
libraries under `lib/`, compiled locally and flashed to the board over a **Raspberry Pi**.
Structured after the sibling `pico-servo` project.

## Quick start

```bash
# one-time: install ESP-IDF, then each shell session:
. $IDF_PATH/export.sh

cp secrets.h.example secrets.h   # edit Wi-Fi creds if using webserver

just compile blink               # build
just flash blink                 # flash via the Pi
just monitor                     # serial monitor on the Pi

just deploy blink                # compile + flash + monitor in one go

just test                        # host-side unit tests (gcc + Unity, no board)
just test-device                 # on-device Unity tests (flash + monitor)
```

See [docs/SETUP.md](docs/SETUP.md) for the full toolchain + wiring guide, and
[CLAUDE.md](CLAUDE.md) for architecture and conventions.

## Targets

| Target | What it does |
|---|---|
| `blink` | toggles a GPIO LED (default) |
| `sweep` | sweeps a hobby servo 0–180° via LEDC (lib present; target not built yet) |
| `webserver` | Wi-Fi + a tabbed dashboard (Tailwind/htmx/Alpine.js/uPlot): user-LED control + blink slider, **WS2812B strip control** (9 patterns, base colour, brightness, speed, per-LED editing + live preview), serial console, live heap/temperature chart |
| `unit_test` | on-device Unity test runner for `led`/`strip`/`sysinfo`/`wifi` (not a firmware app — flash to run the suite) |

## Layout

```
lib/          shared components: led, serial, servo, strip (+strip_fx), sysinfo,
              web, wifi (+ esp-idf submodule)
targets/      one ESP-IDF project each: blink, webserver, unit_test
test/host/    host-side unit tests (plain gcc + vendored Unity, no hardware)
compile.sh    build all or one target (local, needs idf.py)
flash.sh      merge image + flash via Pi (esptool over /dev/ttyACM0)
justfile      deploy / compile / flash / monitor / test / test-device
```

## Testing

Two layers (details in [test/host/README.md](test/host/README.md) and
[targets/unit_test/README.md](targets/unit_test/README.md)):

- **Host tests** (`just test`) — plain `gcc` + the Unity vendored in `lib/esp-idf`,
  no board. Cover pure-logic library code (currently `lib/web`'s form parsers).
  Fast inner loop.
- **On-device tests** (`just test-device`) — a `unit_test` IDF target flashed to
  the C6; Unity exercises real peripherals (`led`, `strip` RMT, `sysinfo` temp
  sensor) and a live Wi-Fi connect, printing standard Unity output (CI-drivable
  via `pytest-embedded`). All 14 cases pass on hardware.

> ℹ️ `blink` is hardware-verified on the XIAO ESP32-C6. `webserver` runs on hardware and drives
> an external **WS2812B** strip on GPIO2 (wiring in [docs/SETUP.md](docs/SETUP.md)). See the
> Status note in CLAUDE.md.
