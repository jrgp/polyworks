#!/bin/bash
# build.sh — Build the PolyWorks Lazarus application.
# Usage: ./build.sh [--debug|--release]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_VENDOR="$SCRIPT_DIR/src-fp/vendor"
BUILD_DIR="$SCRIPT_DIR/build"

mkdir -p "$BUILD_DIR"

MODE="${1:---debug}"

echo "=== Building PolyWorks application ($MODE) ==="

# Ensure C wrapper library is built
if [[ ! -f "$SRC_VENDOR/libpw_stb_image.a" ]]; then
  echo "  Building stb_image wrapper..."
  gcc -c -O2 -o "$SRC_VENDOR/pw_stb_image.o" "$SRC_VENDOR/pw_stb_image.c"
  ar rcs "$SRC_VENDOR/libpw_stb_image.a" "$SRC_VENDOR/pw_stb_image.o"
fi

# Build Lazarus project
echo "  Running lazbuild..."
lazbuild "$SCRIPT_DIR/polyworks.lpi" 2>&1

echo ""
echo "Build complete. Binary: $BUILD_DIR/polyworks"

