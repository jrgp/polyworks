OpenSoldat PolyWorks
================

Map editor for the game [OpenSoldat](https://github.com/opensoldat/opensoldat)

## Modern rewrite (C++ / wxWidgets)

This branch contains a complete rewrite of PolyWorks in C++17 with wxWidgets
and OpenGL, replacing the original VB6 implementation with a cross-platform
application that runs on Windows, Linux and macOS.

The original VB6 sources in `src/` remain the behavioural source of truth; see
`docs/source-port-inventory.md` for the element-by-element audit of the port
against them.

### Requirements

- A C++17 compiler (GCC 9+, Clang 10+, or MSVC 2019+)
- CMake 3.16+
- wxWidgets 3.0+ (core, base, gl)
- OpenGL (provided by your OS/graphics driver)

On Debian/Ubuntu:
```sh
sudo apt install build-essential cmake libwxgtk3.0-gtk3-dev libwxgtk-media3.0-gtk3-dev libgl-dev
```

### Building

```sh
./build-linux.sh          # Linux
./build-mac.sh            # macOS (finds wxWidgets via Homebrew)
build-windows.bat         # Windows, native MSVC

# or directly
cmake -S . -B build && cmake --build build -j
```

Output: `build/bin/polyworks`.

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
  gui/           wxWidgets GUI: viewport, tools, main frame, panels, dialogs
  app/           Entry point and Windows resources
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

`packaging/make-windows-zip.sh` produces `dist/PolyWorks-win64.zip`: a
self-contained folder that runs from anywhere with no installation, no
registry entries and no environment variables. It bundles the executable —
statically linked, so it imports only Windows system DLLs — together with the
skins, cursors, palettes, lists and help resources, and creates the `Textures`
and `Scenery-gfx` folders that PolyWorks searches beside its own executable.

The executable can be cross-compiled from Linux with mingw-w64 against a
static wxWidgets build:

```sh
cmake -S . -B build-win \
  -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake \
  -DPW_MINGW_PREFIX=/path/to/wx-mingw-prefix \
  -DPW_STATIC_RUNTIME=ON -DCMAKE_BUILD_TYPE=Release \
  -DwxWidgets_CONFIG_EXECUTABLE=/path/to/wx-mingw-prefix/bin/wx-config
cmake --build build-win -j
./packaging/make-windows-zip.sh
```

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
