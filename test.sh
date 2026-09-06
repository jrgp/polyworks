#!/bin/bash
# test.sh — Build and run the PolyWorks headless test suite.
# Usage: ./test.sh [--verbose]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_CORE="$SCRIPT_DIR/src-fp/core"
SRC_TESTS="$SCRIPT_DIR/src-fp/tests"
BUILD_DIR="$SCRIPT_DIR/build/tests"
BINARY="$BUILD_DIR/polyworks_tests"

mkdir -p "$BUILD_DIR"

echo "=== Building PolyWorks test suite ==="
fpc \
  -Fu"$SRC_CORE" \
  -Fu"$SRC_TESTS" \
  -FU"$BUILD_DIR" \
  -FE"$BUILD_DIR" \
  -O2 \
  -g \
  "$SRC_TESTS/polyworks_tests.pas" \
  2>&1

echo ""
echo "=== Running tests ==="
cd "$SRC_TESTS"
if [[ "${1:-}" == "--verbose" ]]; then
  "$BINARY" --all --format=plain
else
  "$BINARY" --all
fi

echo ""
echo "Done."
