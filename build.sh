#!/bin/bash
# build.sh — Build the PolyWorks Lazarus application.
# Usage: ./build.sh [--debug|--release]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_CORE="$SCRIPT_DIR/src-fp/core"
SRC_GUI="$SCRIPT_DIR/src-fp/gui"
BUILD_DIR="$SCRIPT_DIR/build/app"

mkdir -p "$BUILD_DIR"

MODE="${1:---debug}"

if [[ "$MODE" == "--release" ]]; then
  OPT_FLAGS="-O3 -Xs"
else
  OPT_FLAGS="-O1 -g -dDEBUG"
fi

echo "=== Building PolyWorks application ($MODE) ==="

# Build headless core first (compile to units only)
for UNIT in pw.types pw.utils pw.geometry pw.pms pw.map; do
  echo "  Compiling $UNIT..."
  fpc \
    -Fu"$SRC_CORE" \
    -FU"$BUILD_DIR" \
    $OPT_FLAGS \
    "$SRC_CORE/$UNIT.pas" 2>&1
done

# Build GUI (requires lazbuild)
if [[ -f "$SCRIPT_DIR/src-fp/polyworks.lpi" ]]; then
  echo "  Building Lazarus project..."
  lazbuild \
    --build-mode="${MODE#--}" \
    "$SCRIPT_DIR/src-fp/polyworks.lpi" 2>&1
else
  echo "  (Lazarus project not yet available; core units compiled only)"
fi

echo ""
echo "Build complete."
