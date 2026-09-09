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
# The wxWidgets *version* is pinned here.  The *ABI* is not: it is derived from
# whichever mingw-w64 compiler is actually installed, because linking against
# import libraries produced by a different GCC is how you get mismatched
# std::string layouts and exception tables.  The invariant is
#
#     installed compiler -> detected ABI -> matching wxWidgets binary
#
# and never a hardcoded ABI that the host may not actually have.
# ---------------------------------------------------------------------------
WX_VERSION="3.2.6"
WX_TAG="v${WX_VERSION}"
WX_BASE_URL="https://github.com/wxWidgets/wxWidgets/releases/download/${WX_TAG}"
WX_API_URL="https://api.github.com/repos/wxWidgets/wxWidgets/releases/tags/${WX_TAG}"

CROSS_CXX="x86_64-w64-mingw32-g++"
CROSS_CC="x86_64-w64-mingw32-gcc"

# Checksums for archives that have been downloaded and verified by hand.  This
# is a pin, not a requirement: an ABI nobody has pinned yet still builds, but
# the script says so out loud and prints the hash to add here.
pinned_sha256() {
    case "$1" in
        wxWidgets-3.2.6-headers.7z)
            echo e683d94c057d57bc44d075d5dcdea0d930927114629ea3d8d8faf7a1d983c449 ;;
        wxMSW-3.2.6_gcc1220_x64_Dev.7z)
            echo 2634ed5e7672b3f7ed02042eb281f128cef1a7e435c54c589e82cb9fb8a99c1a ;;
        wxMSW-3.2.6_gcc1220_x64_ReleaseDLL.7z)
            echo dab13373f1b5e3aa86a1f18fac0151505e89db5cf3bece8c6df746915932c951 ;;
        *) echo "" ;;
    esac
}

# ---------------------------------------------------------------------------
# Toolchain detection.
#
# Upstream names its MinGW packages wxMSW-<ver>_gcc<MAJOR><MINOR><PATCH>[_x64],
# e.g. gcc1220 is GCC 12.2.0 and gcc730 is GCC 7.3.0.  The ABI to use is
# therefore a property of the installed compiler, not something to hardcode.
#
# Match on the *major* version.  GCC keeps its C++ ABI stable across a release
# series -- 12.1 and 12.2 interoperate -- and upstream publishes one binary per
# series, so "the gcc12xx package" is the correct answer for any GCC 12.  It is
# also the only workable rule in practice: Debian patches its cross compiler to
# report its version as "12-win32", with __GNUC_MINOR__ reading 0, so the
# minor and patch components simply are not trustworthy here.  The major
# version, taken from the preprocessor rather than from a version string, is.
# ---------------------------------------------------------------------------
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

# Set WX_ABI / WX_PREFIX / WX_*_FILE from an ABI tag.
set_wx_abi() {
    WX_ABI="$1"
    WX_HEADERS_FILE="wxWidgets-${WX_VERSION}-headers.7z"
    WX_DEV_FILE="wxMSW-${WX_VERSION}_${WX_ABI}_x64_Dev.7z"
    WX_DLL_FILE="wxMSW-${WX_VERSION}_${WX_ABI}_x64_ReleaseDLL.7z"
    WX_PREFIX="$DEPS_DIR/wxMSW-${WX_VERSION}-${WX_ABI}-x64"
}

# An already-extracted package for this compiler's GCC series, if there is one,
# so that repeat builds need no network access.
find_extracted_wx() {
    local dir tag
    for dir in "$DEPS_DIR"/wxMSW-"${WX_VERSION}"-gcc*-x64; do
        [[ -f "$dir/.stamp" ]] || continue
        tag="${dir##*/wxMSW-${WX_VERSION}-}"; tag="${tag%-x64}"
        [[ "$tag" =~ ^gcc([0-9]+)$ ]] || continue
        local digits="${BASH_REMATCH[1]}"
        [[ "${digits:0:${#digits}-2}" == "$GCC_MAJOR" ]] || continue
        set_wx_abi "$tag"
        return 0
    done
    return 1
}

# Ask the wxWidgets project which MinGW binaries it actually publishes for the
# pinned release and pick the one built by this compiler's GCC series.  Also
# records the sizes upstream reports, so the downloads can be checked against a
# source other than the files themselves.  Populates WX_ASSET_SIZES.
declare -A WX_ASSET_SIZES=()
select_wx_abi() {
    local json
    json="$(curl --fail --location --silent "$WX_API_URL")" \
        || die "Could not reach the wxWidgets release index at
  $WX_API_URL
Network access is needed the first time a given compiler ABI is built."

    local report
    report="$(WX_VERSION="$WX_VERSION" GCC_MAJOR="$GCC_MAJOR" python3 -c '
import json, os, re, sys

rel = json.load(sys.stdin)
assets = {a["name"]: a.get("size", 0) for a in rel.get("assets", [])}
ver, major = os.environ["WX_VERSION"], int(os.environ["GCC_MAJOR"])

# wxMSW-<ver>_gcc<digits>_x64_Dev.7z.  Tags carrying a suffix (gcc1030TDM) are
# a different toolchain distribution, not plain mingw-w64, and are skipped.
pat = re.compile(r"^wxMSW-%s_gcc(\d+)_x64_Dev\.7z$" % re.escape(ver))
found = {}
for name in assets:
    m = pat.match(name)
    if not m:
        continue
    d = m.group(1)
    if len(d) < 3:
        continue
    found[(int(d[:-2]), int(d[-2]), int(d[-1]))] = "gcc" + d

matches = sorted(k for k in found if k[0] == major)
if not matches:
    print("MISSING")
    print("AVAILABLE " + " ".join(
        "%s (GCC %d.%d.%d)" % (found[k], *k) for k in sorted(found)))
    sys.exit(0)

abi = found[matches[-1]]
names = ["wxWidgets-%s-headers.7z" % ver,
         "wxMSW-%s_%s_x64_Dev.7z" % (ver, abi),
         "wxMSW-%s_%s_x64_ReleaseDLL.7z" % (ver, abi)]
missing = [n for n in names if n not in assets]
if missing:
    print("INCOMPLETE " + " ".join(missing))
    sys.exit(0)
print("ABI %s %d.%d.%d" % (abi, *matches[-1]))
for n in names:
    print("SIZE %s %d" % (n, assets[n]))
' <<< "$json")" || die "Could not parse the wxWidgets release index."

    if grep -q '^MISSING' <<< "$report"; then
        die "wxWidgets ${WX_VERSION} publishes no x86_64 MinGW-w64 binary built
by GCC ${GCC_MAJOR}.

  Installed compiler : $CROSS_CC ${GCC_VERSION} ($GCC_TARGET)
  Published x64 ABIs : $(sed -n 's/^AVAILABLE //p' <<< "$report")

The wxWidgets project builds binaries for only a few GCC releases, and linking
against a different major version mixes incompatible C++ ABIs.  Install a
mingw-w64 GCC from one of the series listed above, or pin a wxWidgets release
that ships binaries for GCC ${GCC_MAJOR}."
    fi
    if grep -q '^INCOMPLETE ' <<< "$report"; then
        die "wxWidgets ${WX_VERSION} is missing artifacts needed for GCC ${GCC_MAJOR}:
  $(sed -n 's/^INCOMPLETE //p' <<< "$report")"
    fi

    local abi wxgcc
    read -r _ abi wxgcc < <(grep '^ABI ' <<< "$report")
    set_wx_abi "$abi"

    while read -r _ name size; do
        WX_ASSET_SIZES["$name"]="$size"
    done < <(grep '^SIZE ' <<< "$report")

    say "Selected wxWidgets ${WX_VERSION} binary for ABI ${WX_ABI} (built by GCC ${wxgcc})"
    printf '    %s\n' "$WX_HEADERS_FILE" "$WX_DEV_FILE" "$WX_DLL_FILE"
}

# ---------------------------------------------------------------------------
fetch_verified() {
    # fetch_verified <filename>
    #
    # Verified two ways: the size the wxWidgets release index reports (an
    # endpoint distinct from the download CDN), and, where one has been
    # recorded, a pinned SHA-256.
    local name="$1" path="$CACHE_DIR/$1"
    local want_sha want_size
    want_sha="$(pinned_sha256 "$name")"
    want_size="${WX_ASSET_SIZES[$name]:-}"

    check_file() {
        local f="$1" have
        if [[ -n "$want_size" ]] && [[ "$(stat -c%s "$f")" != "$want_size" ]]; then
            echo "size is $(stat -c%s "$f") bytes, upstream says $want_size"
            return 1
        fi
        if [[ -n "$want_sha" ]]; then
            have="$(sha256sum "$f" | cut -d' ' -f1)"
            [[ "$have" == "$want_sha" ]] || { echo "SHA-256 $have, expected $want_sha"; return 1; }
        fi
        return 0
    }

    if [[ -f "$path" ]]; then
        if problem="$(check_file "$path")"; then
            say "Cached: $name"
            return 0
        fi
        warn "Cached $name does not verify ($problem); re-downloading."
        rm -f "$path"
    fi

    say "Downloading $name"
    mkdir -p "$CACHE_DIR"
    curl --fail --location --progress-bar -o "$path.part" "$WX_BASE_URL/$name" \
        || die "Download failed: $WX_BASE_URL/$name"

    if ! problem="$(check_file "$path.part")"; then
        rm -f "$path.part"
        die "$name does not match the official release: $problem
Refusing to build against an archive that is not the published artifact."
    fi
    mv "$path.part" "$path"

    if [[ -z "$want_sha" ]]; then
        warn "No pinned SHA-256 for $name. It matched upstream's published size;
to pin it, add this to pinned_sha256() in $(basename "$0"):
        $name)
            echo $(sha256sum "$path" | cut -d' ' -f1) ;;"
    fi
}

prepare_wx() {
    # A stamp file records which package the prefix holds, so bumping
    # WX_VERSION or changing compiler re-extracts instead of merging two ABIs.
    if find_extracted_wx; then
        say "wxWidgets ${WX_VERSION} (${WX_ABI}) ready"
        return 0
    fi

    select_wx_abi

    fetch_verified "$WX_HEADERS_FILE"
    fetch_verified "$WX_DEV_FILE"
    fetch_verified "$WX_DLL_FILE"

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

    # Upstream's package name records the GCC version but not its thread model,
    # and win32-threads and posix-threads libstdc++ disagree about
    # std::thread/std::mutex.  The DLLs say which they are: a posix-threads
    # build imports libwinpthread-1.dll.
    local wx_threads="win32"
    if x86_64-w64-mingw32-objdump -p "$WX_PREFIX/lib/${WX_ABI}_x64_dll"/wxbase*.dll \
         2>/dev/null | grep -qi 'libwinpthread'; then
        wx_threads="posix"
    fi
    if [[ -n "${GCC_THREADS:-}" && "$GCC_THREADS" != "$wx_threads" ]]; then
        die "Thread-model mismatch: $CROSS_CC is a ${GCC_THREADS}-threads build,
but the official wxWidgets ${WX_ABI} binaries are ${wx_threads}-threads.
Mixing them puts two incompatible libstdc++ configurations on either side of
the wx ABI boundary.  Use the ${wx_threads}-threads mingw-w64 compiler
(on Debian: update-alternatives --config ${CROSS_CC})."
    fi

    echo "${WX_VERSION} ${WX_ABI} x64" > "$WX_PREFIX/.stamp"
}

# ---------------------------------------------------------------------------
command -v x86_64-w64-mingw32-g++ >/dev/null \
    || die "x86_64-w64-mingw32-g++ not found (Debian: apt install g++-mingw-w64-x86-64)"
command -v python3 >/dev/null || die "python3 not found"
command -v cmake >/dev/null || die "cmake not found"
command -v curl  >/dev/null || die "curl not found"

# The Windows build must not see the host's wxGTK.  wxWidgets is resolved from
# PW_WX_MSW_PREFIX, but a stray wxWidgets_CONFIG_EXECUTABLE in the environment
# would be a foot-gun, so refuse it explicitly rather than silently ignoring it.
if [[ -n "${wxWidgets_CONFIG_EXECUTABLE:-}" ]]; then
    die "wxWidgets_CONFIG_EXECUTABLE is set. The Windows build uses the wxMSW
binary package, never a wx-config script; unset it and re-run."
fi

detect_toolchain
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
