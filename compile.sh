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

# ── parse args ─────────────────────────────────────────────────────────────
TARGET=""
CLEAN=0
SECRETS="secrets.h"

for arg in "$@"; do
    case "$arg" in
        --clean) CLEAN=1 ;;
        *)       TARGET="$arg" ;;
    esac
done

if [ -z "$TARGET" ] && [ -f "$SECRETS" ]; then
    TARGET=$(sed -n 's/^#define DEFAULT_TARGET[[:space:]]*"\([^"]*\)".*/\1/p' "$SECRETS")
fi

# ── preflight ──────────────────────────────────────────────────────────────
# Auto-load the ESP-IDF toolchain if idf.py isn't already on PATH, so the build
# works from a cold shell (e.g. `just deploy`). Sourcing is opt-out via NO_IDF_AUTOSOURCE.
if ! command -v idf.py >/dev/null 2>&1 && [ -z "$NO_IDF_AUTOSOURCE" ]; then
    [ -f "$HOME/.idf-uv/bin/activate" ] && source "$HOME/.idf-uv/bin/activate"
    [ -f "lib/esp-idf/export.sh" ] && . lib/esp-idf/export.sh >/dev/null 2>&1
fi
command -v idf.py >/dev/null 2>&1 || die "idf.py not found — run 'source ~/.idf-uv/bin/activate && . lib/esp-idf/export.sh' first (see docs/SETUP.md)"

# which targets to build
if [ -n "$TARGET" ]; then
    [ -d "targets/$TARGET" ] || die "unknown target 'targets/$TARGET'"
    TARGETS=("$TARGET")
else
    TARGETS=()
    for d in targets/*/; do TARGETS+=("$(basename "$d")"); done
fi

LABEL="${TARGET:-all targets (${#TARGETS[@]})}"

# ── header ─────────────────────────────────────────────────────────────────
echo
printf "${CYAN}${BOLD}  ESP32-C6 BUILD${RESET}  ${GRAY}→  ${RESET}${BOLD}%s${RESET}\n" "$LABEL"
rule

build_one() {
    local name="$1"
    local proj="targets/$name"
    local out="build/$name"

    step "Build  ${GRAY}$name${RESET}"

    if [ "$CLEAN" -eq 1 ]; then
        rm -rf "$out"
        info "Removed $out/"
    fi

    local START=$SECONDS
    idf.py -C "$proj" -B "$out" build 2>&1 \
        | sed "s/^/${GRAY}       /" \
        | sed "s/$/${RESET}/"
    ok "Built $name  ${GRAY}($((SECONDS - START)) s)${RESET}"

    local BIN
    BIN=$(find "$out" -maxdepth 1 -name "*.bin" ! -name "bootloader*" 2>/dev/null | head -1)
    if [ -n "$BIN" ]; then
        info "${BIN}  ${YELLOW}($(du -h "$BIN" | cut -f1))${RESET}"
    fi
}

for t in "${TARGETS[@]}"; do
    build_one "$t"
done

# ── done ───────────────────────────────────────────────────────────────────
rule
printf "${GREEN}${BOLD}  Done.${RESET}\n\n"
