#!/usr/bin/env bash
# build-mac.sh — Zero-intervention macOS build for PolyWorks
#
# Usage:
#   ./build-mac.sh            # build application
#   ./build-mac.sh --skip-tests  # skip headless test suite (faster)
#
# What this script does:
#   1. Detects or installs Homebrew
#   2. Detects or installs FPC
#   3. Detects Lazarus / lazbuild (with multiple search paths)
#   4. Rebuilds required Lazarus packages if stale
#   5. Builds the vendored stb_image C wrapper
#   6. Runs the headless test suite
#   7. Builds the PolyWorks Lazarus application
#   8. Creates build/PolyWorks.app bundle
#
# Prerequisites that CANNOT be automated (and why):
#   - Lazarus IDE: no official Homebrew formula and the DMG installer
#     is interactive.  Install from https://lazarus-ide.org and the
#     script will find it automatically under standard macOS paths.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$SCRIPT_DIR"
SKIP_TESTS=false

for arg in "$@"; do
    case "$arg" in
        --skip-tests) SKIP_TESTS=true ;;
        *) echo "Unknown argument: $arg"; exit 1 ;;
    esac
done

# shellcheck source=scripts/common.sh
source "$REPO/scripts/common.sh"

# ===========================================================================
# 1. Architecture
# ===========================================================================
# uname -m on Apple Silicon returns "arm64" (Apple's name).
# FPC and lazbuild use LLVM/GNU naming: "aarch64".
# clang -arch uses Apple's naming: "arm64".
# We keep both so each tool gets the name it expects.
UNAME_ARCH=$(uname -m)   # arm64 | x86_64
case "$UNAME_ARCH" in
    arm64)   CLANG_ARCH="arm64";  FPC_CPU="aarch64" ;;
    x86_64)  CLANG_ARCH="x86_64"; FPC_CPU="x86_64"  ;;
    aarch64) CLANG_ARCH="arm64";  FPC_CPU="aarch64" ;;
    *)       CLANG_ARCH="$UNAME_ARCH"; FPC_CPU="$UNAME_ARCH" ;;
esac
pw_print "Architecture: uname=$UNAME_ARCH  clang=-arch $CLANG_ARCH  fpc/lazbuild --cpu=$FPC_CPU"

# ===========================================================================
# 2. Homebrew — detect or install
# ===========================================================================
pw_print "Detecting Homebrew"

BREW=""
# Standard locations; newer Apple Silicon is /opt/homebrew, Intel is /usr/local
for candidate in /opt/homebrew/bin/brew /usr/local/bin/brew; do
    if [[ -x "$candidate" ]]; then
        BREW="$candidate"
        break
    fi
done

if [[ -z "$BREW" ]] && command -v brew &>/dev/null 2>&1; then
    BREW="$(command -v brew)"
fi

if [[ -z "$BREW" ]]; then
    pw_warn "Homebrew not found — installing automatically."
    pw_warn "This will download and run the official Homebrew installer from brew.sh"
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
    # Re-detect after install
    for candidate in /opt/homebrew/bin/brew /usr/local/bin/brew; do
        if [[ -x "$candidate" ]]; then
            BREW="$candidate"
            break
        fi
    done
    [[ -n "$BREW" ]] || pw_die "Homebrew install appeared to succeed but brew not found."
fi

BREW_PREFIX="$("$BREW" --prefix)"
pw_ok "Homebrew: $BREW ($BREW_PREFIX)"

# Put Homebrew binaries on PATH for the rest of this script
export PATH="$BREW_PREFIX/bin:$BREW_PREFIX/sbin:$PATH"

# ===========================================================================
# 3. FPC — detect or install via Homebrew
# ===========================================================================
pw_print "Detecting FPC"

FPC=""
if command -v fpc &>/dev/null; then
    FPC="$(command -v fpc)"
elif [[ -x "$BREW_PREFIX/bin/fpc" ]]; then
    FPC="$BREW_PREFIX/bin/fpc"
fi

if [[ -z "$FPC" ]]; then
    pw_warn "FPC not found — installing via Homebrew."
    "$BREW" install fpc
    FPC="$(command -v fpc)"
fi

FPC_VERSION="$("$FPC" -iV 2>/dev/null || echo "unknown")"
pw_ok "FPC: $FPC (version $FPC_VERSION)"

# ===========================================================================
# 4. Lazarus / lazbuild — detect (not auto-installable)
# ===========================================================================
pw_print "Detecting Lazarus / lazbuild"

LAZBUILD=""

# Search order: PATH, common macOS install locations, home dir
_lazbuild_candidates=(
    "$(command -v lazbuild 2>/dev/null || true)"
    "$HOME/Downloads/lazarus/lazbuild"
    "$HOME/Applications/Lazarus/lazbuild"
    "/Applications/Lazarus/lazbuild"
    "$BREW_PREFIX/bin/lazbuild"
)

for candidate in "${_lazbuild_candidates[@]}"; do
    if [[ -n "$candidate" && -x "$candidate" ]]; then
        LAZBUILD="$candidate"
        break
    fi
done

# Last resort: walk common parent directories
if [[ -z "$LAZBUILD" ]]; then
    while IFS= read -r f; do
        if [[ -x "$f" ]]; then
            LAZBUILD="$f"
            break
        fi
    done < <(find "$HOME" /Applications -maxdepth 4 -name "lazbuild" -type f 2>/dev/null | head -5)
fi

if [[ -z "$LAZBUILD" ]]; then
    echo ""
    pw_die "Lazarus not found.
  PolyWorks requires the Lazarus IDE and its lazbuild command-line tool.
  There is no fully automated Homebrew install path for Lazarus.

  Install Lazarus:
    1. Download the macOS package from https://lazarus-ide.org (choose the
       package matching your FPC version: $FPC_VERSION)
    2. Open the .dmg and drag Lazarus to ~/Applications or /Applications
    3. Re-run this script

  Typical install paths already searched:
    ~/Downloads/lazarus/lazbuild
    ~/Applications/Lazarus/lazbuild
    /Applications/Lazarus/lazbuild"
fi

LAZARUS_DIR="$(dirname "$LAZBUILD")"
LAZBUILD_VERSION="$("$LAZBUILD" --version 2>/dev/null | head -1 || echo "unknown")"
pw_ok "Lazarus: $LAZBUILD (version $LAZBUILD_VERSION)"

# ===========================================================================
# 5. Validate and fix FPC / Lazarus configuration
# ===========================================================================
pw_print "Validating FPC / Lazarus configuration"

# lazbuild reads Lazarus's environmentoptions.xml which stores the last-used
# target CPU.  On Apple Silicon the Lazarus installer may have written "arm64"
# (Apple's uname name) whereas FPC 3.x uses the LLVM/GNU name "aarch64".
# When lazbuild detects the mismatch it picks up the wrong name and passes
# -Parm64 to FPC, which fails with "Illegal processor type".
#
# Fix: patch environmentoptions.xml before every build so that the stored
# target matches what FPC actually expects.  We look in the two standard
# Lazarus config locations on macOS.
_fix_env_options() {
    local xml="$1"
    [[ -f "$xml" ]] || return 0
    # Replace arm64-darwin with aarch64-darwin in the stored target triple
    if grep -q 'arm64-darwin' "$xml" 2>/dev/null; then
        pw_warn "  Patching $xml: arm64-darwin → aarch64-darwin"
        # Use perl for in-place sed on macOS (avoids -i '' portability issues)
        perl -pi -e 's/arm64-darwin/aarch64-darwin/g' "$xml"
    fi
}

# Lazarus config dirs (macOS standard locations)
_fix_env_options "$HOME/.lazarus/environmentoptions.xml"
_fix_env_options "$HOME/Library/Application Support/lazarus/environmentoptions.xml"
# Also check next to the Lazarus.app bundle
_fix_env_options "$LAZARUS_DIR/environmentoptions.xml"
# And project-level override if it exists
_fix_env_options "$REPO/environmentoptions.xml"

pw_ok "Will compile with: $FPC  cpu=$FPC_CPU  os=darwin"

# ===========================================================================
# 6. Rebuild stale Lazarus packages
# ===========================================================================
# Lazarus packages can have stale .ppu files when the FPC version changes.
# We rebuild LCL and lazopenglcontext before building the project.
# Failure here is non-fatal (the project build will catch real problems).
pw_print "Rebuilding Lazarus packages (LCL + lazopenglcontext)"

_rebuild_package() {
    local lpk="$1"
    if [[ -f "$lpk" ]]; then
        pw_print "  Rebuilding: $(basename "$lpk")"
        "$LAZBUILD" \
            --compiler="$FPC" \
            --os=darwin \
            --cpu="$FPC_CPU" \
            --build-all \
            "$lpk" 2>&1 | grep -v "^Hint:" | grep -v "^Note:" | tail -5 || true
    else
        pw_warn "  Package not found, skipping: $lpk"
    fi
}

# Locate Lazarus component tree relative to lazbuild
_LCL_PKG="$LAZARUS_DIR/lcl/lcl.lpk"
_OGL_PKG="$LAZARUS_DIR/components/opengl/lazopenglcontext.lpk"

# Fallback: search from parent of lazbuild directory
if [[ ! -f "$_LCL_PKG" ]]; then
    _LCL_PKG="$(find "$(dirname "$LAZARUS_DIR")" -maxdepth 3 -name "lcl.lpk" 2>/dev/null | head -1 || true)"
fi
if [[ ! -f "$_OGL_PKG" ]]; then
    _OGL_PKG="$(find "$(dirname "$LAZARUS_DIR")" -maxdepth 5 -name "lazopenglcontext.lpk" 2>/dev/null | head -1 || true)"
fi

_rebuild_package "$_LCL_PKG"
_rebuild_package "$_OGL_PKG"

# ===========================================================================
# 7. Build stb_image C wrapper
# ===========================================================================
build_stb_wrapper "clang" "-arch $CLANG_ARCH"

# ===========================================================================
# 8. Headless tests
# ===========================================================================
if [[ "$SKIP_TESTS" == "false" ]]; then
    run_tests "$FPC"
else
    pw_warn "Skipping headless tests (--skip-tests)"
fi

# ===========================================================================
# 9. Build the Lazarus application
# ===========================================================================
pw_print "Building PolyWorks (Lazarus)"

mkdir -p "$BUILD_DIR"

"$LAZBUILD" \
    --compiler="$FPC" \
    --os=darwin \
    --cpu="$FPC_CPU" \
    "$REPO/polyworks.lpi" 2>&1

BUILT_BIN="$BUILD_DIR/polyworks"
[[ -f "$BUILT_BIN" ]] || pw_die "Build appeared to succeed but binary not found: $BUILT_BIN"
pw_ok "Binary: $BUILT_BIN"

# ===========================================================================
# 10. Create macOS .app bundle
# ===========================================================================
pw_print "Creating PolyWorks.app bundle"

APP_DIR="$BUILD_DIR/PolyWorks.app"
APP_CONTENTS="$APP_DIR/Contents"
APP_MACOS="$APP_CONTENTS/MacOS"
APP_RESOURCES="$APP_CONTENTS/Resources"

# (Re-)create bundle skeleton
rm -rf "$APP_DIR"
mkdir -p "$APP_MACOS" "$APP_RESOURCES"

# Copy binary
cp "$BUILT_BIN" "$APP_MACOS/polyworks"
chmod +x "$APP_MACOS/polyworks"

# Write Info.plist
cat > "$APP_CONTENTS/Info.plist" <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
    "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleName</key>
    <string>PolyWorks</string>
    <key>CFBundleDisplayName</key>
    <string>PolyWorks</string>
    <key>CFBundleIdentifier</key>
    <string>org.opensoldat.polyworks</string>
    <key>CFBundleVersion</key>
    <string>2.0.0</string>
    <key>CFBundleShortVersionString</key>
    <string>2.0</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleSignature</key>
    <string>????</string>
    <key>CFBundleExecutable</key>
    <string>polyworks</string>
    <key>NSHighResolutionCapable</key>
    <true/>
    <key>NSPrincipalClass</key>
    <string>NSApplication</string>
    <key>LSMinimumSystemVersion</key>
    <string>11.0</string>
    <key>NSHumanReadableCopyright</key>
    <string>MIT License</string>
</dict>
</plist>
PLIST

# PkgInfo (optional but conventional)
printf 'APPL????' > "$APP_CONTENTS/PkgInfo"

pw_ok "Bundle: $APP_DIR"

# ===========================================================================
# Done
# ===========================================================================
echo ""
printf '\033[1;32m'
echo "════════════════════════════════════════════"
echo " PolyWorks build complete!"
echo ""
echo " Binary:    $BUILT_BIN"
echo " App:       $APP_DIR"
echo ""
echo " Run from terminal:  $BUILT_BIN"
echo " Run from Finder:    open $APP_DIR"
echo "════════════════════════════════════════════"
printf '\033[0m'
