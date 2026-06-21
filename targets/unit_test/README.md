# unit_test — on-device Unity tests

Integration tests for the hardware-coupled libraries, run on the actual
ESP32-C6. They exercise real peripherals (GPIO, RMT, the on-chip temperature
sensor) and the Wi-Fi AP, so unlike the host tests in `test/host/` they need the
board (and, for Wi-Fi, the network) present.

## Run

```bash
just test-device          # compile + flash via Pi + monitor
# or, by hand:
./compile.sh unit_test
./flash.sh --remote unit_test     # or: ./flash.sh unit_test  (board on this machine)
just monitor
```

Results print over the USB-Serial-JTAG console in standard Unity format
(`...:PASS` / `:FAIL` per case, then a `N Tests M Failures K Ignored` summary),
so `pytest-embedded`'s `dut.expect_unity_test_output()` can drive it in CI.

## Suites (`main/`)

| File | Library | What it checks |
|---|---|---|
| `test_led.c`     | `lib/led`     | on/off/toggle logical state on GPIO15; blink-task start/stop lifecycle |
| `test_strip.c`   | `lib/strip`   | RMT channel + encoder init, pixel count, out-of-range writes ignored, and that `strip_show()` *completes* (times the blocking RMT flush — a broken setup would hang) |
| `test_sysinfo.c` | `lib/sysinfo` | temperature in a plausible range, stats/resources JSON keys + shape, truncation safety |
| `test_wifi.c`    | `lib/wifi`    | pre-connect state, then a real `wifi_connect()` to the AP in `secrets.h` |

Each file exposes one `run_<lib>_tests()` entry point; `main.c` calls them
between `UNITY_BEGIN()`/`UNITY_END()`.

## Environment notes

- **Wi-Fi**: `test_wifi.c` joins the AP from `secrets.h`. With placeholder creds
  the connect case is **ignored** (not failed). `wifi_connect()` inits the
  netif/event loop and so must run once — it's the last suite for that reason.
- **LED**: the user LED on GPIO15 visibly flickers during `test_led` — expected.
- **Strip**: no physical strip needed; the RMT transfer runs regardless of what
  (if anything) is wired to GPIO2.
- **Adding a suite**: add `test_<name>.c` with a `run_<name>_tests()`, declare it
  in `tests.h`, list the file in `main/CMakeLists.txt` `SRCS`, and call it from
  `main.c`.
