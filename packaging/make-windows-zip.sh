#!/usr/bin/env bash
# packaging/make-windows-zip.sh — build the portable Windows distribution.
#
# Produces dist/PolyWorks-win64.zip containing a self-contained PolyWorks
# directory that can be extracted anywhere and run without installation,
# registry entries, environment variables or PATH changes.
#
# The file list is derived from installer/pw.nsi, which is the original
# application's authoritative runtime manifest, minus the parts that are
# specific to the VB6 runtime (OCX controls, dx8vb.dll) and plus the
# directories the C++ asset resolver searches relative to the executable.
#
# Usage:
#   packaging/make-windows-zip.sh [path/to/PolyWorks.exe]
#
# When no executable is given, build-win/bin/PolyWorks.exe is used.  Run
# ./build_windows.sh first: it fetches the official wxWidgets Windows binaries
# that this script bundles alongside the executable.

set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
EXE="${1:-$REPO/build-win/bin/PolyWorks.exe}"
DIST="$REPO/dist"
STAGE="$DIST/PolyWorks"
ZIP="$DIST/PolyWorks-win64.zip"

say() { printf '\033[1;34m==> %s\033[0m\n' "$*"; }
die() { printf '\033[1;31mERROR: %s\033[0m\n' "$*" >&2; exit 1; }

[[ -f "$EXE" ]] || die "PolyWorks.exe not found at $EXE"

say "Staging into $STAGE"
rm -rf "$STAGE" "$ZIP"
mkdir -p "$STAGE"

# ---------------------------------------------------------------------------
# Executable and its runtime DLLs.
#
# PolyWorks links the official wxWidgets Windows binaries, which are DLL
# builds, so the wx DLLs and the GCC runtime they import have to travel with
# the executable.  Rather than listing them by hand -- a list that silently
# rots the moment a dependency changes -- walk the import table transitively
# and copy everything that is not a Windows system DLL.  Anything that cannot
# be found is a hard error, which is what stops a ZIP that only runs on this
# machine from being published.
# ---------------------------------------------------------------------------
say "Copying executable"
install -m 0755 "$EXE" "$STAGE/PolyWorks.exe"

OBJDUMP=x86_64-w64-mingw32-objdump
command -v "$OBJDUMP" >/dev/null || die "$OBJDUMP not found; cannot verify dependencies."

# DLLs that are part of Windows itself and must never be redistributed.
SYSTEM_DLLS='^(ADVAPI32|COMCTL32|COMDLG32|CRYPT32|DWMAPI|GDI32|GDIPLUS|GLU32|IMM32|KERNEL32|MSIMG32|MSVCRT|OLE32|OLEACC|OLEAUT32|OPENGL32|RPCRT4|SETUPAPI|SHELL32|SHLWAPI|USER32|USERENV|UXTHEME|VERSION|WINHTTP|WINMM|WINSPOOL|WS2_32|WSOCK32|UUID|BCRYPT|NETAPI32|IPHLPAPI)\.(DLL|DRV)$'

# Where redistributable DLLs may come from: the wxWidgets package fetched by
# build_windows.sh, and the mingw-w64 GCC runtime directory.
DLL_SEARCH_DIRS=()
while IFS= read -r d; do DLL_SEARCH_DIRS+=("$d"); done < <(
    find "$REPO/.deps" -maxdepth 3 -type d -name 'gcc*_x64_dll' 2>/dev/null)
if command -v x86_64-w64-mingw32-g++ >/dev/null; then
    DLL_SEARCH_DIRS+=("$(dirname "$(x86_64-w64-mingw32-g++ -print-libgcc-file-name)")")
fi

imports_of() { "$OBJDUMP" -p "$1" | sed -n 's/^[[:space:]]*DLL Name: //p'; }

find_dll() {
    local want="$1" dir f
    for dir in "${DLL_SEARCH_DIRS[@]}"; do
        [[ -d "$dir" ]] || continue
        # Import names are case-insensitive on Windows; the files on disk may
        # not match the case recorded in the import table.
        f=$(find "$dir" -maxdepth 1 -iname "$want" -print -quit 2>/dev/null)
        [[ -n "$f" ]] && { printf '%s' "$f"; return 0; }
    done
    return 1
}

say "Resolving runtime DLLs"
QUEUE=("$STAGE/PolyWorks.exe")
BUNDLED=()
while (( ${#QUEUE[@]} )); do
    current="${QUEUE[0]}"; QUEUE=("${QUEUE[@]:1}")
    while IFS= read -r dep; do
        [[ -n "$dep" ]] || continue
        if [[ "${dep^^}" =~ $SYSTEM_DLLS ]]; then
            continue
        fi
        # Already staged?  Compare case-insensitively, as Windows would.
        if find "$STAGE" -maxdepth 1 -iname "$dep" | grep -q .; then
            continue
        fi
        src=$(find_dll "$dep") || die "Required DLL not found anywhere: $dep
Searched: ${DLL_SEARCH_DIRS[*]}
Run ./build_windows.sh first so the wxWidgets package is present."
        install -m 0644 "$src" "$STAGE/$dep"
        BUNDLED+=("$dep")
        QUEUE+=("$STAGE/$dep")
    done < <(imports_of "$current")
done

for d in "${BUNDLED[@]}"; do echo "    $d"; done
say "Bundled ${#BUNDLED[@]} redistributable DLL(s)"

# ---------------------------------------------------------------------------
# Static assets.
#
# skins/     — bitmaps, cursors and colors.ini.  getSkinsPath() in
#              src-cpp/app/main.cpp looks for <exeDir>/skins/default first, so
#              the directory name and nesting must be preserved exactly.
# palettes/  — colour palettes.  PalettePanel reads <exeDir>/palettes/
#              current.txt on open and writes it back on exit (VB6
#              frmPalette.frm:867, modConfig.bas:389).
# lists/     — named scenery lists (VB6 frmScenery.frm:436).
# Help/      — images referenced by PolyWorks Help.html.
# Workspace/ — default floating-window layout.
# ---------------------------------------------------------------------------
say "Copying static assets"
cp -a "$REPO/installer/skins"     "$STAGE/skins"
cp -a "$REPO/installer/palettes"  "$STAGE/palettes"
cp -a "$REPO/installer/lists"     "$STAGE/lists"
cp -a "$REPO/installer/Help"      "$STAGE/Help"
cp -a "$REPO/installer/Workspace" "$STAGE/Workspace"

install -m 0644 "$REPO/installer/PolyWorks Help.html" "$STAGE/PolyWorks Help.html"
for ico in PW PMS PFB; do
    install -m 0644 "$REPO/installer/$ico.ico" "$STAGE/$ico.ico"
done

# Windows thumbnail caches that crept into the repository are not assets.
find "$STAGE" -iname 'Thumbs.db' -delete

# ---------------------------------------------------------------------------
# Directories the asset resolver searches relative to the executable.  They
# ship empty because Soldat's artwork is not redistributable, but their
# presence is what lets a user drop the game's files in and have everything
# resolve with no configuration (MainFrame::RegisterAppAssetPaths).
# ---------------------------------------------------------------------------
say "Creating game-asset directories"
mkdir -p "$STAGE/Textures" "$STAGE/Scenery-gfx" "$STAGE/Maps" "$STAGE/Prefabs"

cat > "$STAGE/Textures/README.txt" <<'EOF'
Put Soldat's polygon textures here (the contents of <Soldat>/Textures).

PolyWorks searches this folder automatically because it sits next to
PolyWorks.exe -- no configuration is needed.  Maps that live inside a Soldat
installation also resolve textures from that installation directly.

Soldat's artwork is not redistributable, so this folder ships empty.
EOF

cat > "$STAGE/Scenery-gfx/README.txt" <<'EOF'
Put Soldat's scenery graphics here (the contents of <Soldat>/Scenery-gfx).

PolyWorks searches this folder automatically because it sits next to
PolyWorks.exe -- no configuration is needed, and the Scenery panel lists
whatever it finds here.

Soldat's artwork is not redistributable, so this folder ships empty.
EOF

cat > "$STAGE/Maps/README.txt" <<'EOF'
A convenient place to keep .pms maps.

Maps opened from anywhere else work exactly the same; this folder is only the
default location used when a map name is passed on the command line without a
path (for example by the .pms file association).
EOF

install -m 0644 "$REPO/packaging/README.dist.txt" "$STAGE/README.txt"

# ---------------------------------------------------------------------------
say "Creating $ZIP"
( cd "$DIST" && zip -qr "$(basename "$ZIP")" PolyWorks )

say "Done"
ls -lh "$ZIP"
( cd "$DIST" && find PolyWorks -type f | sort | sed 's/^/    /' )
