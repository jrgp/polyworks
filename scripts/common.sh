#!/usr/bin/env bash
# scripts/common.sh — shared build helpers sourced by build.sh / build-mac.sh
# Source this file; do not execute it directly.
# Requires: REPO variable set to the repository root before sourcing.

VENDOR_DIR="$REPO/src-fp/vendor"
SRC_CORE="$REPO/src-fp/core"
SRC_TESTS="$REPO/src-fp/tests"
BUILD_DIR="$REPO/build"
TESTS_BIN_DIR="$BUILD_DIR/tests"

# ---------------------------------------------------------------------------
# pw_print  — print a labelled progress message
# ---------------------------------------------------------------------------
pw_print() { printf '\033[1;34m==> %s\033[0m\n' "$*"; }
pw_ok()    { printf '\033[1;32m    OK: %s\033[0m\n' "$*"; }
pw_warn()  { printf '\033[1;33m    WARN: %s\033[0m\n' "$*"; }
pw_die()   { printf '\033[1;31mERROR: %s\033[0m\n' "$*" >&2; exit 1; }

# ---------------------------------------------------------------------------
# require_cmd  — die with a helpful message if a command isn't found
# ---------------------------------------------------------------------------
require_cmd() {
    local cmd="$1"; shift
    local hint="${1:-}"
    if ! command -v "$cmd" &>/dev/null; then
        if [[ -n "$hint" ]]; then
            pw_die "Required command '$cmd' not found. $hint"
        else
            pw_die "Required command '$cmd' not found."
        fi
    fi
}

# ---------------------------------------------------------------------------
# build_stb_wrapper  — compile libpw_stb_image.a from vendored sources
#   $1 = C compiler (cc / clang / gcc)
#   $2 = extra CFLAGS (optional)
# ---------------------------------------------------------------------------
build_stb_wrapper() {
    local cc="${1:-cc}"
    local extra_cflags="${2:-}"

    pw_print "Building stb_image C wrapper"
    local src="$VENDOR_DIR/pw_stb_image.c"
    local obj="$VENDOR_DIR/pw_stb_image.o"
    local lib="$VENDOR_DIR/libpw_stb_image.a"

    # Always rebuild: the .a is platform-specific and not committed to git.
    # shellcheck disable=SC2086
    "$cc" -O2 -I"$VENDOR_DIR" $extra_cflags -c "$src" -o "$obj"
    ar rcs "$lib" "$obj"
    pw_ok "$(ls -lh "$lib")"
}

# ---------------------------------------------------------------------------
# run_tests  — build and run the headless FPCUnit test suite
#   $1 = fpc binary path
# ---------------------------------------------------------------------------
run_tests() {
    local fpc="${1:-fpc}"
    pw_print "Building and running headless tests"
    mkdir -p "$TESTS_BIN_DIR"
    "$fpc" \
        -Fu"$SRC_CORE" \
        -Fu"$SRC_TESTS" \
        -FU"$TESTS_BIN_DIR" \
        -FE"$TESTS_BIN_DIR" \
        -O2 -g \
        "$SRC_TESTS/polyworks_tests.pas" 2>&1

    # Run from tests dir so relative map paths resolve
    (cd "$SRC_TESTS" && "$TESTS_BIN_DIR/polyworks_tests" --all)
    pw_ok "All headless tests passed"
}
