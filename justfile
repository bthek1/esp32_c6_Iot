default:
    @just --list

# ── Workflow ──────────────────────────────────────────────────────────────────

# compile locally and flash to ESP32-C6 via Pi (default: from secrets.h)
[group('workflow')]
deploy target='': (compile target) (flash target) _wait-tty monitor

# clean compile locally and flash to ESP32-C6 via Pi
[group('workflow')]
deploy-clean target='': (compile-clean target) (flash target) _wait-tty monitor

# ── Build ─────────────────────────────────────────────────────────────────────

# compile locally (default: from secrets.h, e.g: just compile blink)
[group('build')]
compile target='':
    ./compile.sh {{target}}

# clean then compile locally
[group('build')]
compile-clean target='':
    ./compile.sh --clean {{target}}

# ── Device ────────────────────────────────────────────────────────────────────

# flash a target to the ESP32-C6 via the Pi (default: from secrets.h)
[group('device')]
flash target='':
    ./flash.sh --remote {{target}}

# flash a target to an ESP32-C6 plugged into THIS machine
[group('device')]
flash-local target='':
    ./flash.sh {{target}}

# open serial monitor on the Pi
[group('device')]
monitor:
    ssh pi "picocom -b 115200 /dev/ttyACM0"

[private]
_wait-tty:
    @until ssh pi "test -e /dev/ttyACM0"; do sleep 1; done
