#!/usr/bin/env bash
# test.sh — Build and run the PolyWorks headless test suite.
# Usage: ./test.sh [--verbose]
# Works on Linux and macOS.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$SCRIPT_DIR"

# shellcheck source=scripts/common.sh
source "$REPO/scripts/common.sh"

require_cmd fpc "Install Free Pascal: https://freepascal.org"
FPC="$(command -v fpc)"

VERBOSE="${1:-}"

pw_print "PolyWorks headless test suite"
pw_ok "FPC: $FPC ($("$FPC" -iV))"

mkdir -p "$TESTS_BIN_DIR"

pw_print "Compiling tests"
"$FPC" \
  -Fu"$SRC_CORE" \
  -Fu"$SRC_TESTS" \
  -FU"$TESTS_BIN_DIR" \
  -FE"$TESTS_BIN_DIR" \
  -O2 -g \
  "$SRC_TESTS/polyworks_tests.pas" 2>&1

echo ""
pw_print "Running tests"
BINARY="$TESTS_BIN_DIR/polyworks_tests"
cd "$SRC_TESTS"
if [[ "$VERBOSE" == "--verbose" ]]; then
    "$BINARY" --all --format=plain
else
    "$BINARY" --all
fi

echo ""
pw_ok "Done."
