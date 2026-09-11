#!/usr/bin/env bash
# build_windows.sh — build the Windows PolyWorks executable on a Unix host.
#
# Cross-compiles PolyWorks for Windows with mingw-w64.  The GUI stack is Dear
# ImGui + GLFW + OpenGL; both dependencies are source-only, pinned by version
# and SHA-256 and fetched by CMake (cmake/DearImGui.cmake) into a shared cache,
# so there is no Windows binary package to obtain here and nothing outside the
# Win32 and system OpenGL DLLs to ship.  Run packaging/make-windows-zip.sh
# afterwards to produce the portable ZIP, or pass --package to do both.
#
# Usage:
#   ./build_windows.sh [--package] [--clean]

set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$REPO/build-win"
DEPS_DIR="$REPO/.deps"
CACHE_DIR="${PW_DEPS_CACHE:-$DEPS_DIR/cache}"

say()  { printf '\033[1;34m==> %s\033[0m\n' "$*"; }
warn() { printf '\033[1;33m%s\033[0m\n' "$*" >&2; }
die()  { printf '\033[1;31mERROR: %s\033[0m\n' "$*" >&2; exit 1; }

DO_PACKAGE=0
DO_CLEAN=0
for arg in "$@"; do
    case "$arg" in
        --package) DO_PACKAGE=1 ;;
        --clean)   DO_CLEAN=1 ;;
        -h|--help) sed -n '2,11p' "$0"; exit 0 ;;
        *) die "Unknown option: $arg" ;;
    esac
done

CROSS_CC="x86_64-w64-mingw32-gcc"

# ---------------------------------------------------------------------------
# Toolchain detection.
#
# There is no longer an ABI-matched binary dependency to select -- ImGui and
# GLFW are compiled from source by this same compiler -- but the target still
# has to be verified, because a 32-bit or non-MinGW compiler would produce an
# executable that silently does not belong in the x64 distribution.
detect_toolchain() {
    local macros
    macros="$($CROSS_CC -E -dM -x c /dev/null 2>/dev/null)" \
        || die "$CROSS_CC could not preprocess an empty file; the toolchain looks broken."

    GCC_MAJOR="$(sed -n 's/^#define __GNUC__ //p'           <<< "$macros")"
    local minor patch
    minor="$(sed -n 's/^#define __GNUC_MINOR__ //p'         <<< "$macros")"
    patch="$(sed -n 's/^#define __GNUC_PATCHLEVEL__ //p'    <<< "$macros")"
    [[ "$GCC_MAJOR" =~ ^[0-9]+$ ]] \
        || die "Could not determine the GCC major version of $CROSS_CC."
    GCC_VERSION="${GCC_MAJOR}.${minor}.${patch}"

    # The version *string* is what distributions like to rewrite; report it too
    # so a mismatch with the macros above is visible rather than mysterious.
    GCC_VERSION_STRING="$($CROSS_CC -dumpversion 2>/dev/null || echo unknown)"

    GCC_TARGET="$($CROSS_CC -dumpmachine)"
    [[ "$GCC_TARGET" == x86_64-*-mingw32* ]] \
        || die "$CROSS_CC targets '$GCC_TARGET', which is not 64-bit MinGW-w64.
This build is x86_64 Windows only."

    # "Thread model: win32" / "posix".  Not part of upstream's package name, so
    # it is checked against the downloaded DLLs after extraction instead.
    GCC_THREADS="$($CROSS_CC -v 2>&1 | sed -n 's/^Thread model: //p' | head -1)"

    say "Cross-compiler: $CROSS_CC"
    printf '    Target          %s\n' "$GCC_TARGET"
    printf '    GCC version     %s (reported as "%s")\n' \
           "$GCC_VERSION" "$GCC_VERSION_STRING"
    printf '    GCC series      %s\n' "$GCC_MAJOR"
    printf '    Thread model    %s\n' "${GCC_THREADS:-unknown}"
}

# ---------------------------------------------------------------------------
command -v x86_64-w64-mingw32-g++ >/dev/null \
    || die "x86_64-w64-mingw32-g++ not found (Debian: apt install g++-mingw-w64-x86-64)"
command -v cmake >/dev/null || die "cmake not found"
command -v curl  >/dev/null || die "curl not found"

detect_toolchain

if (( DO_CLEAN )); then
    say "Removing $BUILD_DIR"
    rm -rf "$BUILD_DIR"
fi

say "Configuring"
cmake -S "$REPO" -B "$BUILD_DIR" \
    -DCMAKE_TOOLCHAIN_FILE="$REPO/cmake/mingw-w64-x86_64.cmake" \
    -DPW_DEPS_CACHE="$CACHE_DIR" \
    -DCMAKE_BUILD_TYPE=Release

say "Building"
cmake --build "$BUILD_DIR" -j"$(nproc)"

EXE="$BUILD_DIR/bin/PolyWorks.exe"
[[ -f "$EXE" ]] || die "Build produced no $EXE"

# A Windows binary importing GTK, X11 or MSYS libraries would mean a dependency
# leaked in from the Linux host.  Check rather than assume.
BAD=$(x86_64-w64-mingw32-objdump -p "$EXE" | sed -n 's/^\s*DLL Name: //p' \
      | grep -iE 'gtk|gdk|glib|x11|cygwin|msys|pango|cairo|wxmsw|wxbase' || true)
[[ -z "$BAD" ]] || die "Windows build imports non-Win32 libraries: $BAD"

# With ImGui and GLFW static and the GCC runtime linked statically, the only
# imports left should be Windows' own DLLs.  Anything else would have to be
# shipped, so surface it here instead of discovering it under Wine.
NONSYS=$(x86_64-w64-mingw32-objdump -p "$EXE" | sed -n 's/^\s*DLL Name: //p' \
      | grep -ivE '^(kernel32|user32|gdi32|shell32|shlwapi|advapi32|ole32|oleaut32|comctl32|comdlg32|winmm|ws2_32|imm32|version|msvcrt|opengl32|glu32|dwmapi|uxtheme|rpcrt4|setupapi|cfgmgr32|hid|dinput8|xinput1_[0-9]|bcrypt|crypt32|secur32|api-ms-.*|ucrtbase)\.dll$' || true)
[[ -z "$NONSYS" ]] || warn "Executable imports non-system DLLs that must be packaged:
$NONSYS"

say "Built $EXE"
x86_64-w64-mingw32-objdump -p "$EXE" | sed -n 's/^\s*DLL Name: /    /p' | sort -u

if (( DO_PACKAGE )); then
    "$REPO/packaging/make-windows-zip.sh" "$EXE"
fi
