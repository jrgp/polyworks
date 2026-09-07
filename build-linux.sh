#!/usr/bin/env bash
set -e
cmake -S . -B build
cmake --build build -- -j"$(nproc)"
echo "Build complete: build/bin/polyworks"
