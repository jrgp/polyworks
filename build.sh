#!/usr/bin/env bash
# build.sh — Build the PolyWorks Lazarus application (Linux / generic).
# Usage: ./build.sh [--skip-tests]
#
# For macOS use: ./build-mac.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$SCRIPT_DIR"

# shellcheck source=scripts/common.sh
source "$REPO/scripts/common.sh"

SKIP_TESTS=false
for arg in "$@"; do
    case "$arg" in
        --skip-tests) SKIP_TESTS=true ;;
        *) echo "Unknown argument: $arg"; exit 1 ;;
    esac
done

# ---------------------------------------------------------------------------
# Require tools
# ---------------------------------------------------------------------------
require_cmd fpc  "Install Free Pascal: https://freepascal.org"
require_cmd lazbuild "Install Lazarus: https://lazarus-ide.org"

FPC="$(command -v fpc)"
LAZBUILD="$(command -v lazbuild)"
CC="${CC:-gcc}"

pw_ok "FPC:      $FPC ($("$FPC" -iV))"
pw_ok "lazbuild: $LAZBUILD"

# ---------------------------------------------------------------------------
# Build stb_image wrapper (always rebuild — platform-specific, not in git)
# ---------------------------------------------------------------------------
build_stb_wrapper "$CC"

# ---------------------------------------------------------------------------
# Headless tests
# ---------------------------------------------------------------------------
if [[ "$SKIP_TESTS" == "false" ]]; then
    run_tests "$FPC"
else
    pw_warn "Skipping headless tests (--skip-tests)"
fi

# ---------------------------------------------------------------------------
# Lazarus application
# ---------------------------------------------------------------------------
pw_print "Building PolyWorks"
mkdir -p "$BUILD_DIR"
"$LAZBUILD" "$REPO/polyworks.lpi" 2>&1

pw_ok "Binary: $BUILD_DIR/polyworks"

