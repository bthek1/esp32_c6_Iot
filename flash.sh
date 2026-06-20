#!/bin/bash
set -e

# ── colours ────────────────────────────────────────────────────────────────
BOLD=$'\e[1m'
DIM=$'\e[2m'
RESET=$'\e[0m'
CYAN=$'\e[1;36m'
GREEN=$'\e[1;32m'
YELLOW=$'\e[1;33m'
RED=$'\e[1;31m'
BLUE=$'\e[1;34m'
GRAY=$'\e[0;37m'

step()    { printf "\n${CYAN}  ──  ${BOLD}%s${RESET}\n" "$*"; }
ok()      { printf "${GREEN}  ✓  ${RESET}%s\n" "$*"; }
info()    { printf "${BLUE}  ·  ${RESET}${GRAY}%s${RESET}\n" "$*"; }
warn()    { printf "${YELLOW}  ⚠  ${RESET}%s\n" "$*"; }
die()     { printf "\n${RED}  ✗  ERROR: %s${RESET}\n\n" "$*" >&2; exit 1; }
rule()    { printf "${DIM}  %s${RESET}\n" "$(printf '─%.0s' {1..54})"; }

ROOT="${BASH_SOURCE[0]%/*}"
cd "$ROOT"

# ── config ─────────────────────────────────────────────────────────────────
SECRETS="secrets.h"
_DEFAULT=$([ -f "$SECRETS" ] && sed -n 's/^#define DEFAULT_TARGET[[:space:]]*"\([^"]*\)".*/\1/p' "$SECRETS" || echo "blink")
TARGET=""
REMOTE=0
CHIP="${ESP_CHIP:-esp32c6}"
BAUD="${ESP_BAUD:-460800}"
PI_HOST="${ESP_PI_HOST:-pi}"
PI_PORT="${ESP_PI_PORT:-/dev/ttyACM0}"
LOCAL_PORT="${ESP_PORT:-/dev/ttyACM0}"
PI_FLASH_DIR="~/esp-flash"

for arg in "$@"; do
    case "$arg" in
        --remote) REMOTE=1 ;;
        *)        TARGET="$arg" ;;
    esac
done

TARGET="${TARGET:-$_DEFAULT}"
BUILD_DIR="build/$TARGET"

MODE="${REMOTE:+remote → $PI_HOST}"
MODE="${MODE:-local}"

# Auto-load the ESP-IDF toolchain (which provides esptool) if it isn't on PATH,
# so flashing works from a cold shell (e.g. `just deploy`). Opt out with NO_IDF_AUTOSOURCE.
if ! command -v esptool >/dev/null 2>&1 && ! command -v esptool.py >/dev/null 2>&1 \
   && [ -z "$NO_IDF_AUTOSOURCE" ]; then
    [ -f "$HOME/.idf-uv/bin/activate" ] && source "$HOME/.idf-uv/bin/activate"
    [ -f "lib/esp-idf/export.sh" ] && . lib/esp-idf/export.sh >/dev/null 2>&1
fi

# pick an esptool entrypoint
if command -v esptool.py >/dev/null 2>&1; then ESPTOOL="esptool.py"
elif command -v esptool >/dev/null 2>&1; then ESPTOOL="esptool"
else die "esptool not found locally — needed to merge the firmware image (pip install esptool)"; fi

# ── header ─────────────────────────────────────────────────────────────────
echo
printf "${YELLOW}${BOLD}  ESP32-C6 FLASH${RESET}  ${GRAY}→  ${RESET}${BOLD}%s${RESET}  ${GRAY}[%s]${RESET}\n" "$TARGET" "$MODE"
rule

# ── preflight: merge to a single image at 0x0 ──────────────────────────────
step "Preflight"
[ -f "$BUILD_DIR/flash_args" ] || die "no build for '$TARGET' — run ./compile.sh $TARGET first"

MERGED="$BUILD_DIR/firmware.bin"
( cd "$BUILD_DIR" && "$ESPTOOL" --chip "$CHIP" merge_bin -o firmware.bin @flash_args ) \
    | sed "s/^/${GRAY}       /" | sed "s/$/${RESET}/"
[ -f "$MERGED" ] || die "merge_bin did not produce firmware.bin"
info "Image:  $MERGED  ${YELLOW}($(du -h "$MERGED" | cut -f1))${RESET}"

# ── remote flash (via the Pi) ──────────────────────────────────────────────
if [ "$REMOTE" -eq 1 ]; then
    step "Upload  ${GRAY}→  ${PI_HOST}${RESET}"
    ssh "$PI_HOST" "mkdir -p $PI_FLASH_DIR"
    scp "$MERGED" "$PI_HOST:$PI_FLASH_DIR/${TARGET}.bin" \
        | sed "s/^/${GRAY}       /" | sed "s/$/${RESET}/"
    ok "Uploaded  ${GRAY}${TARGET}.bin${RESET}"

    step "Flash on ${PI_HOST}  ${GRAY}${PI_PORT}${RESET}"
    ssh "$PI_HOST" "
        if command -v esptool.py >/dev/null 2>&1; then ET=esptool.py
        elif command -v esptool >/dev/null 2>&1; then ET=esptool
        else echo 'esptool not found on $PI_HOST — pip install esptool' >&2; exit 1; fi
        \$ET --chip $CHIP -p $PI_PORT -b $BAUD write_flash 0x0 $PI_FLASH_DIR/${TARGET}.bin
    "
    ok "Flashed — ESP32-C6 resetting"

# ── local flash ────────────────────────────────────────────────────────────
else
    step "Flash  ${GRAY}${LOCAL_PORT}${RESET}"
    "$ESPTOOL" --chip "$CHIP" -p "$LOCAL_PORT" -b "$BAUD" write_flash 0x0 "$MERGED"
    ok "Flashed — ESP32-C6 resetting"
fi

# ── done ───────────────────────────────────────────────────────────────────
rule
printf "${GREEN}${BOLD}  Done.${RESET}\n\n"
