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
# When no executable is given, build-win/bin/PolyWorks.exe is used.  See
# cmake/mingw-w64-x86_64.cmake for how to produce it.

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
# Executable.  The Windows build links wxWidgets and the GCC runtime
# statically, so there are no non-system DLLs to ship.  Verify that rather
# than assume it: a dynamically linked build would silently produce a ZIP that
# cannot run on a clean machine.
# ---------------------------------------------------------------------------
say "Copying executable"
install -m 0755 "$EXE" "$STAGE/PolyWorks.exe"

if command -v x86_64-w64-mingw32-objdump >/dev/null 2>&1; then
    SYSTEM_DLLS='^(ADVAPI32|COMCTL32|comdlg32|GDI32|KERNEL32|msvcrt|ole32|OLEACC|OLEAUT32|OPENGL32|SHELL32|SHLWAPI|USER32|UxTheme|VERSION|WINSPOOL|WS2_32|IMM32|WINMM|RPCRT4|dwmapi|bcrypt|CRYPT32|WINHTTP|UUID|GLU32)\.(dll|DRV)$'
    NONSYS=$(x86_64-w64-mingw32-objdump -p "$STAGE/PolyWorks.exe" \
             | sed -n 's/^\s*DLL Name: //p' | sort -u \
             | grep -Ev "$SYSTEM_DLLS" || true)
    if [[ -n "$NONSYS" ]]; then
        say "Non-system DLL imports detected; they must be bundled:"
        echo "$NONSYS"
        die "Refusing to build a ZIP that depends on DLLs it does not ship."
    fi
    say "Verified: no non-system DLL imports"
fi

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
