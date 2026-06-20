# esp32_c6_Iot

IoT firmware for the **ESP32-C6** (ESP-IDF, C). Multi-target layout with shared
`components/`, compiled locally and flashed to the board over a **Raspberry Pi**.
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
```

See [docs/SETUP.md](docs/SETUP.md) for the full toolchain + wiring guide, and
[CLAUDE.md](CLAUDE.md) for architecture and conventions.

## Targets

| Target | What it does |
|---|---|
| `blink` | toggles a GPIO LED (default) |
| `sweep` | sweeps a hobby servo 0–180° via LEDC |
| `webserver` | connects to Wi-Fi and serves an embedded page |

## Layout

```
components/   shared: led, serial, servo, wifi
targets/      one ESP-IDF project each: blink, sweep, webserver
compile.sh    build all or one target (local, needs idf.py)
flash.sh      merge image + flash via Pi (esptool over /dev/ttyACM0)
justfile      deploy / compile / flash / monitor
```

> ⚠️ Scaffolded template — not yet compiled or flashed. See the Status note in CLAUDE.md.
