# Host-side unit tests

Fast unit tests for the **pure-logic** library code — they build and run on the
dev machine with plain `gcc` and the **Unity** framework vendored in
`lib/esp-idf`. No ESP32, no `idf.py`, no flashing.

```bash
just test            # or:  make -C test/host
make -C test/host clean
```

## What this covers

Library code with no hardware dependency — currently `lib/web`'s form parsers
(`web_form_color` / `web_form_int` / `web_form_str` / `web_html_escape`).

`lib/web` declares `REQUIRES esp_http_server`, but those parser functions never
call the HTTP server. `stubs/esp_http_server.h` provides a minimal stand-in so
`web.c` compiles on the host; `stubs/stubs.c` gives the unused route/response
helpers no-op bodies so it links. The stubs are **not** behavioural mocks —
nothing in them is exercised by a test.

## Adding a test suite

1. Drop `test_<name>.c` in this dir. Define `setUp`/`tearDown` (Unity requires
   them, empty is fine), write `void test_*(void)` cases, and a `main()` that
   `RUN_TEST(...)`s them between `UNITY_BEGIN()`/`UNITY_END()`.
2. Add `<name>` to the `TESTS` list in the `Makefile`.
3. If the suite links extra library `.c` files, add them to `COMMON_SRC` (or
   give the suite its own rule).

## What this does *not* cover

Anything that touches GPIO, RMT, Wi-Fi, the temperature sensor, etc. That code
needs **on-device** tests: a Unity test-runner target flashed to the C6 (e.g.
via `pytest-embedded`), reusing the existing `flash.sh` / `monitor` flow. Not
set up yet — add a `targets/unit_test` project when hardware-coupled libs need
coverage.
