#!/usr/bin/env bash
# build-linux.sh — build PolyWorks for Linux.
#
# The only system dependencies are a C++17 compiler, CMake, OpenGL and the X11
# development headers GLFW links against (Debian: build-essential cmake
# libgl-dev xorg-dev).  Dear ImGui and GLFW are fetched and compiled by CMake.

set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${1:-$REPO/build}"

cmake -S "$REPO" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" -j"$(nproc)"

echo "Build complete: $BUILD_DIR/bin/polyworks"
echo "Tests:          $BUILD_DIR/bin/pw_tests"
