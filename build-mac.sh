#!/usr/bin/env bash
# build-mac.sh — build PolyWorks.app on macOS, dependencies included.
#
# This script owns the whole dependency chain, exactly as build_windows.sh does
# for Windows: it obtains wxWidgets itself, caches it under .deps/, builds
# against it and produces an application bundle that runs on a Mac that has
# never seen Homebrew.  Nothing needs to be installed first except Xcode's
# command line tools and CMake.
#
# Usage:
#   ./build-mac.sh [--package] [--clean] [--skip-test]

set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$REPO/build"
DEPS_DIR="$REPO/.deps"
CACHE_DIR="${PW_DEPS_CACHE:-$DEPS_DIR/cache}"

say()  { printf '\033[1;34m==> %s\033[0m\n' "$*"; }
warn() { printf '\033[1;33m%s\033[0m\n' "$*" >&2; }
die()  { printf '\033[1;31mERROR: %s\033[0m\n' "$*" >&2; exit 1; }

DO_CLEAN=0
DO_TEST=1
DO_PACKAGE=0
for arg in "$@"; do
    case "$arg" in
        --package)   DO_PACKAGE=1 ;;
        --clean)     DO_CLEAN=1 ;;
        --skip-test) DO_TEST=0 ;;
        -h|--help)   sed -n '2,11p' "$0"; exit 0 ;;
        *) die "Unknown option: $arg" ;;
    esac
done

# ---------------------------------------------------------------------------
# wxWidgets dependency.
#
# Built from the official wxWidgets source release rather than downloaded as a
# binary, because the wxWidgets project does not publish one for macOS: its
# release assets are Windows binaries (wxMSW-*_vc*/gcc*), the documentation and
# the source archives, and nothing else.  The alternative -- Homebrew's
# wxwidgets formula -- is not a wxWidgets release artifact, is not pinned, and
# produces dylibs under /opt/homebrew that would then have to be copied into
# the bundle and rewritten to make the result distributable at all.
#
# The build is static (--disable-shared) and uses wxWidgets' bundled copies of
# libpng/libjpeg/libtiff/zlib/expat, so the finished binary links nothing from
# outside the SDK.  That is also what keeps configure from quietly picking up
# whatever Homebrew happens to have installed on the build machine.
#
# The version is pinned and the archive is checksummed; the installed prefix is
# kept under .deps/ so this cost is paid once.
# ---------------------------------------------------------------------------
WX_VERSION="3.2.6"
WX_ARCHIVE="wxWidgets-${WX_VERSION}.tar.bz2"
WX_URL="https://github.com/wxWidgets/wxWidgets/releases/download/v${WX_VERSION}/${WX_ARCHIVE}"
WX_SHA256="939e5b77ddc5b6092d1d7d29491fe67010a2433cf9b9c0d841ee4d04acb9dce7"

# Oldest macOS the result is expected to run on.  10.10 is wxWidgets 3.2's own
# floor; overridable for anyone who needs to target something newer.
MACOS_MIN="${MACOSX_DEPLOYMENT_TARGET:-10.10}"

# ---------------------------------------------------------------------------
require() { command -v "$1" >/dev/null || die "$2"; }

detect_toolchain() {
    [[ "$(uname -s)" == "Darwin" ]] \
        || die "build-mac.sh builds the macOS application and must run on macOS.
For Linux use ./build-linux.sh, for Windows ./build_windows.sh."

    xcode-select -p >/dev/null 2>&1 \
        || die "Xcode command line tools are not installed.  Run:
    xcode-select --install"

    require cc      "no C compiler found; run: xcode-select --install"
    require make    "make not found; run: xcode-select --install"
    require cmake   "cmake not found.  Install CMake from https://cmake.org/download/"
    require curl    "curl not found"
    require python3 "python3 not found; run: xcode-select --install"

    ARCH="${PW_MACOS_ARCH:-$(uname -m)}"
    case "$ARCH" in
        arm64|x86_64) ;;
        *) die "Unsupported architecture: $ARCH" ;;
    esac

    JOBS="$(sysctl -n hw.logicalcpu 2>/dev/null || echo 4)"
    WX_PREFIX="$DEPS_DIR/wx-${WX_VERSION}-macos-${ARCH}"

    say "Toolchain"
    printf '    macOS           %s\n' "$(sw_vers -productVersion 2>/dev/null || echo unknown)"
    printf '    Architecture    %s\n' "$ARCH"
    printf '    Deployment tgt  %s\n' "$MACOS_MIN"
    printf '    Compiler        %s\n' "$(cc --version 2>/dev/null | head -1)"
    printf '    CMake           %s\n' "$(cmake --version | head -1)"
    printf '    Jobs            %s\n' "$JOBS"
}

fetch_wx() {
    local path="$CACHE_DIR/$WX_ARCHIVE"
    if [[ -f "$path" ]] && [[ "$(shasum -a 256 "$path" | cut -d' ' -f1)" == "$WX_SHA256" ]]; then
        say "Cached: $WX_ARCHIVE"
        return 0
    fi
    [[ -f "$path" ]] && warn "Cached $WX_ARCHIVE has the wrong checksum; re-downloading."

    say "Downloading $WX_ARCHIVE"
    mkdir -p "$CACHE_DIR"
    curl --fail --location --progress-bar -o "$path.part" "$WX_URL" \
        || die "Download failed: $WX_URL"

    local have
    have="$(shasum -a 256 "$path.part" | cut -d' ' -f1)"
    if [[ "$have" != "$WX_SHA256" ]]; then
        rm -f "$path.part"
        die "Checksum mismatch for $WX_ARCHIVE
  expected $WX_SHA256
  got      $have
Refusing to build against an archive that is not the pinned release."
    fi
    mv "$path.part" "$path"
}

build_wx() {
    # The stamp records which package and settings the prefix holds, so
    # changing any of them rebuilds instead of leaving a stale mixture.
    local stamp="$WX_PREFIX/.stamp"
    local want="${WX_VERSION} ${ARCH} ${MACOS_MIN} static"
    if [[ -f "$stamp" ]] && [[ "$(cat "$stamp")" == "$want" ]]; then
        say "wxWidgets ${WX_VERSION} (${ARCH}, static) ready"
        return 0
    fi

    fetch_wx

    local src="$DEPS_DIR/src/wxWidgets-${WX_VERSION}"
    if [[ ! -f "$src/configure" ]]; then
        say "Extracting wxWidgets source"
        rm -rf "$src"
        mkdir -p "$DEPS_DIR/src"
        tar -xjf "$CACHE_DIR/$WX_ARCHIVE" -C "$DEPS_DIR/src"
    fi

    local objdir="$DEPS_DIR/build/wx-${WX_VERSION}-${ARCH}"
    rm -rf "$objdir" "$WX_PREFIX"
    mkdir -p "$objdir"

    say "Building wxWidgets ${WX_VERSION} (this happens once, and takes a while)"
    (
        cd "$objdir"
        # --with-*=builtin keeps configure away from anything Homebrew has
        # installed: the image libraries come from the wxWidgets tree.
        "$src/configure" \
            --prefix="$WX_PREFIX" \
            --disable-shared \
            --disable-debug \
            --disable-tests \
            --without-subdirs \
            --enable-unicode \
            --with-osx_cocoa \
            --with-opengl \
            --with-macosx-version-min="$MACOS_MIN" \
            --with-libpng=builtin \
            --with-libjpeg=builtin \
            --with-libtiff=builtin \
            --with-zlib=builtin \
            --with-expat=builtin \
            --enable-optimise \
            CFLAGS="-arch $ARCH" CXXFLAGS="-arch $ARCH" \
            LDFLAGS="-arch $ARCH" OBJCXXFLAGS="-arch $ARCH" \
            > configure.log 2>&1 \
            || { tail -40 configure.log; die "wxWidgets configure failed (see $objdir/configure.log)"; }

        make -j"$JOBS" > build.log 2>&1 \
            || { tail -40 build.log; die "wxWidgets build failed (see $objdir/build.log)"; }
        make install  >> build.log 2>&1 \
            || { tail -40 build.log; die "wxWidgets install failed (see $objdir/build.log)"; }
    )

    [[ -x "$WX_PREFIX/bin/wx-config" ]] || die "wx-config missing after install"
    echo "$want" > "$stamp"
}

# ---------------------------------------------------------------------------
# Dependency audit.
#
# A bundle that loads anything from /opt/homebrew, /usr/local/opt or the build
# tree works perfectly on the machine that built it and nowhere else, which is
# precisely the failure this build exists to avoid.  Walk every Mach-O in the
# bundle, and where a stray dylib does turn up, copy it into Frameworks and
# repoint the loader at it rather than shipping something broken.
#
# With a static wxWidgets there is normally nothing to move; this runs anyway,
# because "normally" is not a guarantee and the check costs a second.
# ---------------------------------------------------------------------------
BAD_PREFIXES='^(/opt/homebrew|/usr/local/opt|/usr/local/Cellar|/usr/local/lib|/opt/local)'

mach_o_files() {
    find "$1" -type f \( -perm -u+x -o -name '*.dylib' \) -print 2>/dev/null \
        | while IFS= read -r f; do
              file -b "$f" | grep -q 'Mach-O' && printf '%s\n' "$f"
          done
}

dependencies_of() { otool -L "$1" | tail -n +2 | awk '{print $1}'; }

bundle_dylibs() {
    local app="$1"
    local exe="$app/Contents/MacOS/polyworks"
    local frameworks="$app/Contents/Frameworks"
    local changed=1 pass=0

    # Iterate: a bundled dylib may itself pull in another one.
    while (( changed )) && (( pass < 8 )); do
        changed=0
        pass=$((pass + 1))
        local target dep base
        while IFS= read -r target; do
            [[ -n "$target" ]] || continue
            while IFS= read -r dep; do
                [[ "$dep" =~ $BAD_PREFIXES ]] || continue
                base="$(basename "$dep")"
                mkdir -p "$frameworks"
                if [[ ! -f "$frameworks/$base" ]]; then
                    say "Bundling $base"
                    cp -f "$dep" "$frameworks/$base"
                    chmod u+w "$frameworks/$base"
                    install_name_tool -id "@rpath/$base" "$frameworks/$base" 2>/dev/null || true
                fi
                install_name_tool -change "$dep" "@rpath/$base" "$target" 2>/dev/null || true
                changed=1
            done < <(dependencies_of "$target")
        done < <(mach_o_files "$app")
    done

    if [[ -d "$frameworks" ]]; then
        install_name_tool -add_rpath "@loader_path/../Frameworks" "$exe" 2>/dev/null || true
        # Any install_name_tool edit invalidates the signature, and on Apple
        # silicon even an ad-hoc signature is mandatory for the binary to run.
        codesign --force --sign - "$app" 2>/dev/null || true
    fi
}

audit_bundle() {
    local app="$1" bad="" target dep
    while IFS= read -r target; do
        [[ -n "$target" ]] || continue
        while IFS= read -r dep; do
            if [[ "$dep" =~ $BAD_PREFIXES ]] || [[ "$dep" == "$DEPS_DIR"* ]] \
               || [[ "$dep" == "$BUILD_DIR"* ]]; then
                bad+="  $(basename "$target"): $dep"$'\n'
            fi
        done < <(dependencies_of "$target")
    done < <(mach_o_files "$app")

    [[ -z "$bad" ]] || die "The bundle depends on libraries outside itself:
$bad
Those paths do not exist on another Mac.  Check that wxWidgets was built
statically and that no Homebrew library leaked into the link."
}

# Run the finished bundle from somewhere that is not the build tree, with
# Homebrew removed from PATH, to prove it needs neither.
smoke_test() {
    local app="$1" name; name="$(basename "$app")"
    local tmp; tmp="$(mktemp -d /tmp/polyworks-test.XXXXXX)"

    say "Testing the bundle from $tmp"
    cp -R "$app" "$tmp/"

    [[ -f "$tmp/$name/Contents/Resources/PW.icns" ]] \
        || { rm -rf "$tmp"; die "PW.icns is missing from the bundle"; }
    [[ -d "$tmp/$name/Contents/Resources/skins/default" ]] \
        || { rm -rf "$tmp"; die "skins are missing from the bundle"; }
    grep -q 'PW.icns' "$tmp/$name/Contents/Info.plist" \
        || { rm -rf "$tmp"; die "Info.plist does not name the icon file"; }

    # A PATH with no Homebrew on it and no DYLD_* overrides, so anything the
    # app cannot find on its own shows up here rather than on a user's Mac.
    local out="$tmp/run.log"
    env -u DYLD_LIBRARY_PATH -u DYLD_FALLBACK_LIBRARY_PATH -u DYLD_FRAMEWORK_PATH \
        PATH=/usr/bin:/bin:/usr/sbin:/sbin \
        "$tmp/$name/Contents/MacOS/polyworks" > "$out" 2>&1 &
    local pid=$! waited=0
    while (( waited < 15 )) && kill -0 "$pid" 2>/dev/null; do
        sleep 1; waited=$((waited + 1))
    done

    if kill -0 "$pid" 2>/dev/null; then
        kill "$pid" 2>/dev/null || true
        wait "$pid" 2>/dev/null || true
        say "Bundle ran outside the build tree with no Homebrew on PATH"
    else
        wait "$pid" 2>/dev/null || true
        # A missing library is a packaging fault; having no window server
        # (over ssh, or in CI) is a property of the session and is not.
        if grep -qiE 'image not found|library not loaded|dyld' "$out"; then
            sed 's/^/    /' "$out" >&2
            rm -rf "$tmp"
            die "The bundle failed to start outside the build tree."
        fi
        warn "The bundle exited immediately, which is expected when there is no
window server (over ssh, for example).  It reported:"
        sed 's/^/    /' "$out" >&2
    fi
    rm -rf "$tmp"
}

# ---------------------------------------------------------------------------
detect_toolchain
build_wx

if (( DO_CLEAN )); then
    say "Removing $BUILD_DIR"
    rm -rf "$BUILD_DIR"
fi

say "Configuring"
cmake -S "$REPO" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES="$ARCH" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="$MACOS_MIN" \
    -DwxWidgets_CONFIG_EXECUTABLE="$WX_PREFIX/bin/wx-config"

say "Building"
cmake --build "$BUILD_DIR" -j"$JOBS"

APP="$BUILD_DIR/bin/polyworks.app"
[[ -d "$APP" ]] || die "Build produced no $APP"

bundle_dylibs "$APP"
audit_bundle "$APP"

say "Linked libraries"
dependencies_of "$APP/Contents/MacOS/polyworks" | sed 's/^/    /'

if (( DO_TEST )); then
    smoke_test "$APP"
fi

# Archive with ditto rather than zip(1): it is the tool that understands
# application bundles, and it preserves the symlinks, permissions and extended
# attributes that a .app needs in order to still be launchable after a
# round-trip through an archive.
package_app() {
    local dist="$REPO/dist"
    local zip="$dist/PolyWorks-macos.zip"
    mkdir -p "$dist"
    rm -f "$zip"
    say "Creating $zip"
    ( cd "$(dirname "$APP")" && ditto -c -k --keepParent --sequesterRsrc \
          "$(basename "$APP")" "$zip" )
    [[ -f "$zip" ]] || die "ditto produced no archive"
    say "Done"
    ls -lh "$zip"
}

if (( DO_PACKAGE )); then
    package_app
fi

say "Built $APP"
