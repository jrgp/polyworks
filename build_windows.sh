#!/usr/bin/env bash
# build_windows.sh — build the Windows PolyWorks executable on a Unix host.
#
# Fetches the official wxWidgets Windows/MinGW-w64 binary package (cached),
# verifies it, and cross-compiles against it with mingw-w64.  Run
# packaging/make-windows-zip.sh afterwards to produce the portable ZIP, or
# pass --package to do both.
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

# ---------------------------------------------------------------------------
# wxWidgets dependency.
#
# These are the official binaries published by the wxWidgets project itself on
# its GitHub releases page -- not a third-party redistribution.  Three archives
# make up a usable development package:
#
#   headers     the wx headers, shared by every build
#   Dev         import libraries plus the wx/setup.h describing this exact
#               build's configuration
#   ReleaseDLL  the runtime DLLs that ship next to PolyWorks.exe
#
# gcc1220 is the ABI tag: built with GCC 12.2.0, which is what
# x86_64-w64-mingw32-g++ is on Debian 12.  Those binaries import
# libgcc_s_seh-1.dll and libstdc++-6.dll but not libwinpthread-1.dll, so they
# are win32-threads builds and match the default (non -posix) mingw-w64
# compiler.  If you bump WX_VERSION or WX_ABI, update the checksums below --
# they are what stops a URL from being trusted blindly.
# ---------------------------------------------------------------------------
WX_VERSION="3.2.6"
WX_ABI="gcc1220"
WX_BASE_URL="https://github.com/wxWidgets/wxWidgets/releases/download/v${WX_VERSION}"

WX_HEADERS_FILE="wxWidgets-${WX_VERSION}-headers.7z"
WX_DEV_FILE="wxMSW-${WX_VERSION}_${WX_ABI}_x64_Dev.7z"
WX_DLL_FILE="wxMSW-${WX_VERSION}_${WX_ABI}_x64_ReleaseDLL.7z"

WX_HEADERS_SHA256="e683d94c057d57bc44d075d5dcdea0d930927114629ea3d8d8faf7a1d983c449"
WX_DEV_SHA256="2634ed5e7672b3f7ed02042eb281f128cef1a7e435c54c589e82cb9fb8a99c1a"
WX_DLL_SHA256="dab13373f1b5e3aa86a1f18fac0151505e89db5cf3bece8c6df746915932c951"

WX_PREFIX="$DEPS_DIR/wxMSW-${WX_VERSION}-${WX_ABI}-x64"

# ---------------------------------------------------------------------------
fetch_verified() {
    # fetch_verified <filename> <sha256>
    local name="$1" want="$2" path="$CACHE_DIR/$1"

    if [[ -f "$path" ]]; then
        local have
        have="$(sha256sum "$path" | cut -d' ' -f1)"
        if [[ "$have" == "$want" ]]; then
            say "Cached: $name"
            return 0
        fi
        warn "Cached $name has the wrong checksum; re-downloading."
        rm -f "$path"
    fi

    say "Downloading $name"
    mkdir -p "$CACHE_DIR"
    curl --fail --location --progress-bar -o "$path.part" "$WX_BASE_URL/$name" \
        || die "Download failed: $WX_BASE_URL/$name"

    local have
    have="$(sha256sum "$path.part" | cut -d' ' -f1)"
    if [[ "$have" != "$want" ]]; then
        rm -f "$path.part"
        die "Checksum mismatch for $name
  expected $want
  got      $have
Refusing to build against an archive that is not the pinned release."
    fi
    mv "$path.part" "$path"
}

prepare_wx() {
    # A stamp file records which package the prefix holds, so bumping
    # WX_VERSION or WX_ABI re-extracts instead of merging two ABIs.
    local stamp="$WX_PREFIX/.stamp"
    local want="${WX_VERSION} ${WX_ABI} ${WX_DEV_SHA256}"

    if [[ -f "$stamp" ]] && [[ "$(cat "$stamp")" == "$want" ]]; then
        say "wxWidgets ${WX_VERSION} (${WX_ABI}) ready"
        return 0
    fi

    fetch_verified "$WX_HEADERS_FILE" "$WX_HEADERS_SHA256"
    fetch_verified "$WX_DEV_FILE"     "$WX_DEV_SHA256"
    fetch_verified "$WX_DLL_FILE"     "$WX_DLL_SHA256"

    say "Extracting wxWidgets into $WX_PREFIX"
    rm -rf "$WX_PREFIX"
    mkdir -p "$WX_PREFIX"
    # cmake -E tar reads 7z through libarchive, so no separate 7-Zip tool is
    # needed; CMake is already required to build the project.
    ( cd "$WX_PREFIX" && for a in "$WX_HEADERS_FILE" "$WX_DEV_FILE" "$WX_DLL_FILE"; do
          cmake -E tar xf "$CACHE_DIR/$a"
      done )

    [[ -f "$WX_PREFIX/include/wx/wx.h" ]] \
        || die "wx headers missing after extraction"
    [[ -d "$WX_PREFIX/lib/${WX_ABI}_x64_dll" ]] \
        || die "wx libraries missing after extraction"

    echo "$want" > "$stamp"
}

# ---------------------------------------------------------------------------
command -v x86_64-w64-mingw32-g++ >/dev/null \
    || die "x86_64-w64-mingw32-g++ not found (Debian: apt install g++-mingw-w64-x86-64)"
command -v cmake >/dev/null || die "cmake not found"
command -v curl  >/dev/null || die "curl not found"

# The Windows build must not see the host's wxGTK.  wxWidgets is resolved from
# PW_WX_MSW_PREFIX, but a stray wxWidgets_CONFIG_EXECUTABLE in the environment
# would be a foot-gun, so refuse it explicitly rather than silently ignoring it.
if [[ -n "${wxWidgets_CONFIG_EXECUTABLE:-}" ]]; then
    die "wxWidgets_CONFIG_EXECUTABLE is set. The Windows build uses the wxMSW
binary package, never a wx-config script; unset it and re-run."
fi

prepare_wx

if (( DO_CLEAN )); then
    say "Removing $BUILD_DIR"
    rm -rf "$BUILD_DIR"
fi

say "Configuring"
cmake -S "$REPO" -B "$BUILD_DIR" \
    -DCMAKE_TOOLCHAIN_FILE="$REPO/cmake/mingw-w64-x86_64.cmake" \
    -DPW_WX_MSW_PREFIX="$WX_PREFIX" \
    -DCMAKE_BUILD_TYPE=Release

say "Building"
cmake --build "$BUILD_DIR" -j"$(nproc)"

EXE="$BUILD_DIR/bin/PolyWorks.exe"
[[ -f "$EXE" ]] || die "Build produced no $EXE"

# A Windows binary that imports GTK, X11 or MSYS libraries would mean the wx
# dependency was resolved from the host by mistake.  Check rather than assume.
BAD=$(x86_64-w64-mingw32-objdump -p "$EXE" | sed -n 's/^\s*DLL Name: //p' \
      | grep -iE 'gtk|gdk|glib|x11|cygwin|msys|pango|cairo' || true)
[[ -z "$BAD" ]] || die "Windows build imports non-Win32 libraries: $BAD"

say "Built $EXE"
x86_64-w64-mingw32-objdump -p "$EXE" | sed -n 's/^\s*DLL Name: /    /p' | sort -u

if (( DO_PACKAGE )); then
    "$REPO/packaging/make-windows-zip.sh" "$EXE"
fi
