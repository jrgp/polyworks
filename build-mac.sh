#!/usr/bin/env bash
set -e

# Detect wxWidgets via Homebrew (arm64 or x86_64) or system cmake prefix path.
if command -v brew &>/dev/null; then
    WX_PREFIX="$(brew --prefix wxwidgets 2>/dev/null || true)"
fi

CMAKE_EXTRA=""
if [ -n "$WX_PREFIX" ] && [ -d "$WX_PREFIX" ]; then
    CMAKE_EXTRA="-DCMAKE_PREFIX_PATH=$WX_PREFIX"
    echo "Using wxWidgets from Homebrew: $WX_PREFIX"
fi

cmake -S . -B build $CMAKE_EXTRA
cmake --build build -- -j"$(sysctl -n hw.logicalcpu)"
echo "Build complete: build/bin/polyworks"
