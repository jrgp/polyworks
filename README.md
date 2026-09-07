OpenSoldat PolyWorks
================

Map editor for the game [OpenSoldat](https://github.com/opensoldat/opensoldat)

## Modern rewrite (Free Pascal / Lazarus)

This branch contains a complete rewrite of PolyWorks in Free Pascal and Lazarus,
replacing the original VB6 implementation with a cross-platform application.

### Requirements

- Free Pascal 3.2+ (`fpc`)
- Lazarus 2.2+ (`lazbuild`)
- OpenGL (provided by your OS/graphics driver)
- GCC (for building the stb_image C wrapper, first run only)

On Debian/Ubuntu:
```sh
sudo apt install fpc lazarus libgl-dev
```

### Building

```sh
# Build the full Lazarus application
./build.sh

# Output: build/polyworks
```

### Testing (headless, no display required)

```sh
./test.sh          # run all tests
./test.sh --verbose  # verbose output
```

All tests run without a display server, OpenGL context, or GUI.

### Running

```sh
./build/polyworks
```

Open a Soldat `.pms` map file with **File → Open**.

### Project layout

```
src-fp/
  core/          Headless core: types, PMS format, geometry, map model, undo
  gui/           Lazarus GUI: renderer, viewport, tools, main form, panels, dialogs
  tests/         FPCUnit test suite (headless)
  vendor/        stb_image C wrapper
maps/            Real Soldat map fixtures (regression testing)
polyworks.lpi    Lazarus project file
polyworks.lpr    Application entry point
build.sh         Linux/generic build script
build-mac.sh     macOS build script (zero manual intervention)
test.sh          Test runner
scripts/         Shared build helpers
arch.md          Reverse-engineered VB6 architecture documentation
plan.md          Implementation roadmap
progress.md      Current implementation status
decisions.md     Architecture and compatibility decisions
```

### Building on macOS

```sh
./build-mac.sh
```

That's it. The script:

- Detects or installs [Homebrew](https://brew.sh)
- Detects or installs Free Pascal via Homebrew (`brew install fpc`)
- Locates Lazarus / `lazbuild` in standard macOS paths
- Builds the vendored `stb_image` C wrapper (no external headers needed)
- Runs the full headless test suite
- Builds the application with `lazbuild`
- Creates `build/PolyWorks.app` (runnable from Finder or Terminal)

**One unavoidable manual step:** Lazarus has no Homebrew formula.
If it isn't found, the script prints a single actionable error.
Download the macOS installer from https://lazarus-ide.org, drag it to
`~/Applications` or `/Applications`, then re-run `./build-mac.sh`.

```sh
# Run from Terminal
./build/polyworks

# Or open the .app bundle
open build/PolyWorks.app
```

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
