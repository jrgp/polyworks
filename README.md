OpenSoldat PolyWorks
================

Map editor for the game [OpenSoldat](https://github.com/opensoldat/opensoldat)

## Modern rewrite (C++ / Dear ImGui)

This branch contains a complete rewrite of PolyWorks in C++17 with Dear ImGui,
GLFW and OpenGL, replacing the original VB6 implementation with a
cross-platform application that runs on Windows, Linux and macOS.

The stack is deliberately short:

```
C++ core  ->  Dear ImGui  ->  GLFW  ->  OpenGL
```

The core (`src-cpp/core`, `src-cpp/renderer`) knows nothing about ImGui or
GLFW and is exercised entirely headlessly by the test suite.  Dear ImGui draws
the whole interface -- menus, the tool palette, the floating tool windows,
dialogs, context menus and the status bar -- so there is no native widget
toolkit, and therefore no GTK, no Cocoa widgets and no third-party DLLs to
ship.

The original VB6 sources in `src/` remain the behavioural source of truth; see
`docs/source-port-inventory.md` for the element-by-element audit of the port
against them.

### Requirements

- A C++17 compiler (GCC 9+, Clang 10+, or MSVC 2019+)
- CMake 3.16+
- OpenGL (provided by your OS/graphics driver)

Dear ImGui and GLFW are **not** system dependencies: CMake fetches both,
pinned by version and SHA-256, into `.deps/cache/` and compiles them into the
application.

On Debian/Ubuntu the build needs only the compiler, CMake and the X11/OpenGL
development headers GLFW links against:
```sh
sudo apt install build-essential cmake libgl-dev xorg-dev
```

### Building

```sh
./build-linux.sh          # Linux
./build-mac.sh            # macOS (self-contained .app)
./build_windows.sh        # Windows, cross-compiled with mingw-w64

# or directly
cmake -S . -B build && cmake --build build -j
```

Output: `build/bin/polyworks`, or `build/bin/polyworks.app` on macOS.

### macOS

`./build-mac.sh` needs nothing installed but Xcode's command line tools and
CMake — in particular nothing from Homebrew. The only libraries are Dear ImGui
and GLFW, which are compiled into the executable, so nothing from
`/opt/homebrew` or `/usr/local/opt` can leak into the link.

After linking, the script walks every Mach-O in the bundle and fails if any of
them loads a library from outside it (bundling and re-pointing it with
`install_name_tool` if one somehow appears), signs the bundle, then copies it
to a temporary directory with `ditto` and launches it with Homebrew removed
from `PATH` — so a bundle that only works on the machine that built it does not
get shipped.

The signature is ad-hoc by default. It is not optional: on Apple silicon the
kernel refuses to execute a binary carrying no signature at all, and Finder
reports that as *"the application is damaged and can't be opened"* — which
looks like a corrupt download rather than the missing signature it is. Set
`CODESIGN_IDENTITY` to sign with a Developer ID instead. The signature is
verified after every step that can invalidate it, including after the release
ZIP has been unpacked again.

An ad-hoc signature satisfies the kernel but not Gatekeeper's notarisation
check, so the first launch of a downloaded build still needs **right-click ▸
Open**, or `xattr -dr com.apple.quarantine PolyWorks.app`. That prompt is
expected; "damaged" is not.

The Dock and Finder icon is the application's own icon: `installer/PW.ico`, the
Windows resource the original has always shipped, converted to `.icns` by
`packaging/make-icns.py`. That runs on any Python 3 and enlarges the icon's
48x48 artwork with nearest-neighbour sampling, which keeps the original pixels
crisp instead of smearing them.

### Testing (headless, no display required)

```sh
./build/bin/pw_tests
```

Run it from the repository root so that the real maps in `maps/` are found:
they are used as compatibility fixtures and every one of them is round-tripped
through the loader and saver on each run.

### Running

```sh
./build/bin/polyworks [map.pms]
```

### Project layout

```
src-cpp/
  core/          Headless core: types, PMS format, geometry, map model, undo
  renderer/      OpenGL renderer and the asset resolver
  ui/            Dear ImGui interface: shell, editor state, interaction state
                 machine, menus, panels, dialogs, context menus, theme
  tests/         Headless test suite
vendor/          stb_image
maps/            Real Soldat map fixtures (compatibility testing)
installer/       Original runtime assets: skins, cursors, palettes, help
packaging/       Portable-distribution build scripts
cmake/           Cross-compilation toolchain files
docs/            Port audit, interaction notes, architecture
arch.md          Reverse-engineered VB6 architecture documentation
```

### Portable Windows distribution

`build_windows.sh` produces `dist/PolyWorks-win64.zip`: a self-contained
folder that runs from anywhere with no installation, no registry entries and
no environment variables. In practice the executable is the only binary in it:
ImGui and GLFW are compiled in and the GCC runtime is linked statically, so
there are no redistributable DLLs left to bundle. Alongside it go the skins,
cursors, palettes, lists and help resources, plus the `Textures` and
`Scenery-gfx` folders that PolyWorks searches beside its own executable.

```sh
./build_windows.sh --package
```

There is no Windows binary dependency to obtain: the same pinned ImGui and
GLFW sources the Linux and macOS builds use are compiled by the cross
compiler. The script verifies that `x86_64-w64-mingw32-gcc` really targets
64-bit MinGW-w64 before starting, and prints the detected version and target.

After linking it inspects the executable's import table and fails if any GTK,
X11, MSYS or Cygwin dependency appears, so the dependency chain is guaranteed
to remain `PolyWorks.exe → GLFW/Win32 → system OpenGL`. It also warns about any
import that is not a Windows system DLL, since such a thing would have to be
packaged.

The ZIP's contents are not chosen by hand either: `make-windows-zip.sh` walks
the import table of the executable and of every DLL it pulls in, copies each
non-system dependency it finds, and fails if one cannot be located. On a
correct build it copies nothing.

### Releases

Pushing a version tag builds and publishes both platforms:

```sh
git tag v1.0.0
git push origin v1.0.0
```

`.github/workflows/release.yml` cross-compiles Windows in a Debian bookworm
container — the toolchain this project is developed against, so CI uses the
same mingw-w64 GCC a developer does — and
builds macOS natively on an Apple silicon runner — macOS cannot be
cross-compiled, and the two jobs run in parallel. Each job runs the same build
script a developer would (`./build_windows.sh --package`, `./build-mac.sh
--package`); the workflow contains no build or packaging logic of its own. The
release is created only if both succeed, and carries:

```
polyworks-1.0.0-win-x64.zip
polyworks-1.0.0-macos-arm64.zip
```

The workflow can also be started by hand from the Actions tab, which builds and
uploads both artifacts without creating a release — useful for checking a
change to either build script.

### Asset resolution

Textures and scenery are looked up, in order:

1. the path as written in the map, if it names an existing file;
2. directories beside the map (`Textures`, `Scenery-gfx`, the map's own folder,
   and the same three one level up) — so maps kept inside a Soldat
   installation resolve their artwork with no configuration;
3. the configured Soldat directory from **File > Preferences**;
4. directories beside the executable (`Textures`, `Scenery-gfx`);
5. the skin directory.

Lookups are case-insensitive, because maps are authored on Windows. When
nothing matches, PolyWorks substitutes `notfound.bmp`, prints every path it
searched, and keeps the map editable.

---

## Original VB6 application

![Screenshot of the Polyworks GUI](/docs/img/screenshot.jpg?raw=true "OpenSoldat Polyworks")

Requirements for the original VB6 version:
* Visual Basic 6 SP6
* NSIS (optional - for generating the Installer)
* rcedit or Resource Hacker (optional - for replacing the old icon)

Notes
-----
When VB6 is installed, it may have issues finding referenced controls, such as "MBMouse.ocx".
You have to add them manually out of the /installer folder.

If you want to contribute while using Visual Studio 2015 update 1+, you may need this addon:
https://visualstudiogallery.msdn.microsoft.com/00cc8ff8-beb3-4f08-8aa6-59eefba3bb40
(You will still need VB6 for compilation)

Folders
-------
```
.
├─ doc        # build/development instructions
├─ src        # source code and project files
├─ installer  # installer files and data, config, manual (also build output dir)
├─ LICENSE
└─ README.md
```

License
-------
MIT

### Note:
PolyWorks v1.4.0.17 with source code was originally released by Anna Zajaczkowski as:  
["Feel free to do whatever you want with it."](https://web.archive.org/web/20191012125637/https://forums.soldat.pl/index.php?topic=174.msg214342)  
It was subsequently relicensed under the MIT License by [the PolyWorks contributors](https://web.archive.org/web/20220710122849/https://github.com/opensoldat/polyworks/issues/8).  
