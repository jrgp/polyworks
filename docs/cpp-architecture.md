# PolyWorks C++ Architecture

## Overview

PolyWorks is a map editor for the game Soldat. This document describes the
C++/Dear ImGui/OpenGL reimplementation started in 2025 after a Pascal/Lazarus
prototype established the file format and data model.  An intermediate
wxWidgets GUI was replaced by Dear ImGui + GLFW; the core and renderer are
unchanged by that move, which is the point of keeping them toolkit-free.

## Project structure

```
polyworks/
├── src/                      # Original VB6 source (read-only reference)
├── src-cpp/
│   ├── core/                 # Platform-independent core (no UI, no GL)
│   │   ├── pms_types.h       # Binary-compatible PMS structs
│   │   ├── pms_io.h/.cpp     # PMS file I/O (load/save/compile)
│   │   ├── map_document.h/.cpp  # Editor document model
│   │   ├── geometry.h/.cpp   # Pure geometric operations
│   │   ├── viewport_geometry.h  # Screen -> viewport -> world pipeline
│   │   └── undo_stack.h/.cpp # Undo/redo via in-memory snapshots
│   ├── renderer/             # OpenGL renderer (depends on core only)
│   │   ├── renderer.h/.cpp   # Render pass (polygons, scenery, overlays)
│   │   └── texture_manager.h/.cpp
│   ├── ui/                   # Dear ImGui + GLFW interface
│   │   ├── app.h/.cpp        # GLFW window, ImGui frame loop, input, DPI
│   │   ├── editor.h/.cpp     # All editor state and every command
│   │   ├── interaction.h/.cpp  # Explicit mouse/tool state machine
│   │   ├── menus.cpp         # Menu bar, shortcuts, status bar
│   │   ├── panels.cpp        # The seven floating tool windows
│   │   ├── dialogs.cpp       # File browser, Map Settings, Preferences
│   │   ├── file_dialog.h/.cpp  # Native file chooser (Win32/Cocoa via NFDe)
│   │   ├── context_menu.cpp  # Viewport right-click menus
│   │   ├── theme.h/.cpp      # ImGui style built from the skin's colors.ini
│   │   ├── gfx.h/.cpp        # Image and .cur loading, GL upload
│   │   ├── platform.h/.cpp   # Paths, process launching
│   │   ├── ini_file.h/.cpp   # polyworks.ini
│   │   └── main.cpp          # Entry point
│   └── tests/
│       └── pw_tests.cpp      # Headless tests (no UI, no GL)
├── maps/                     # 97 .pms map files (regression fixtures)
├── installer/
│   ├── skins/default/        # UI bitmaps, cursors, icons
│   └── PolyWorks Help.html   # Authoritative user-facing spec
├── vendor/
│   ├── stb_image.h           # Single-header image loader
│   └── stb_image.c           # Implementation unit
└── CMakeLists.txt
```

## Dependency graph

```
pms_types.h ← geometry.h/.cpp
pms_types.h ← pms_io.h/.cpp
pms_types.h + pms_io.h ← map_document.h/.cpp
map_document.h + pms_io.h ← undo_stack.h/.cpp

pw_core (static lib) = all of the above

stb_image ← renderer (texture loading)
pw_core + stb_image ← renderer
pw_core + renderer ← ui (Dear ImGui + GLFW)
pw_core (no UI, no GL) ← pw_tests

Nothing under core/ or renderer/ includes imgui.h or GLFW/glfw3.h.  The
dependency runs one way only, which is what lets the whole document model and
the coordinate pipeline be tested without a display.
```

## PMS file format

The PMS format (version 11) is a flat binary file matching the VB6 UDT layout
exactly (all structs packed, no alignment padding). See `pms_types.h` for the
complete struct definitions with `static_assert` size checks.

Key observations:
- `mapRandomID == -1`: PolyWorks native format; contains lights and sketch lines
- `mapRandomID > 0`: compiled game format; has computed sector table, centred coords
- VB6 Boolean in UDT = 2 bytes (int16_t); -1=true, 0=false
- Spawn X/Y are int32 (not float!)
- Pascal-style length-prefixed byte strings (byte[0]=length, byte[1..N]=ASCII)
- Light range is int16, not int32

Sector table:
- Always 51×51 cells (SECTOR_NUM=25, cells = 2*25+1)
- Each cell: int16 count + count×int16 1-based polygon indices
- In native saves: all zeros (5202 bytes)
- In compiled saves: contains polygon indices for spatial lookup

## Coordinate system

```
World coords: float X, Y; Y increases downward (screen convention)
Screen = (world - scroll) * zoom
World  = screen / zoom + scroll

scroll: world coordinate at viewport top-left corner
zoom:   default 1.0; min 0.03125 (1/32); max 512.0

Screen cache: EditorVertex.screen = (world - scroll) * zoom
  Rebuilt by doc.rebuildScreenCache() after any zoom/scroll change.
```

## Editor document model (MapDocument)

`MapDocument` owns all mutable map state. It is the single source of truth
for the editor. The GUI never stores map data — it always reads from and writes
to `MapDocument`.

Key collections:
- `polys` — vector of EditorPoly (3 EditorVertex each with world + screen coords)
- `scenery` — vector of EditorScenery
- `sceneryNames` — [0] = sentinel, [1..N] = texture filenames
- `spawns`, `colliders`, `waypoints`, `lights`, `sketch`
- `options` — MapOptions (name, texture, background colours, etc.)

Editing contract:
- Any method that modifies world positions calls `rebuildScreenCache()`
- Any method that modifies data sets `modified = true`
- Callers push undo snapshots (`undoStack.push(doc)`) BEFORE destructive edits

## Undo/redo

`UndoStack` serializes the entire document state to in-memory byte vectors
using the native PMS format (same as `savePmsFile`). Restoring means
deserializing and calling `pmsDataToDoc`.

Note: zoom and scroll are NOT included in snapshots (matching VB6 behavior).

## Renderer (planned)

OpenGL 2.1 immediate mode for initial implementation.
Projection: `glOrtho(0, viewW, viewH, 0, -1, 1)` so screen coords from
`ScreenV` go directly to clip space.

Render order (matching VB6):
1. Background gradient
2. Scenery back layer
3. Textured polygons
4. Scenery middle layer
5. Wireframe / points overlay (if enabled)
6. Scenery front layer
7. Collision overlay
8. Selection handles
9. Grid
10. Waypoints, spawns, colliders, lights

## GUI architecture

Dear ImGui draws every control.  There is one GLFW window; the menu bar is at
the top, the status bar at the bottom, the map viewport fills the region
between them, and the tool windows float over it as ImGui windows.  The style
is built at startup from the skin's `colors.ini`, so the application does not
look like default ImGui.

The one exception is choosing a file.  Windows and macOS open the operating
system's own chooser through Native File Dialog Extended (`file_dialog.h`),
which is what the original's `CommonDialog` control did and what gives the user
the sidebar, recent places, volumes and -- on macOS -- the file-access prompt a
self-drawn browser cannot.  Linux keeps the ImGui browser in `dialogs.cpp`,
because every native backend there needs GTK or a D-Bus portal and neither
belongs in this dependency graph.  Both paths deliver their answer the same
way, so no caller knows which one ran.

`Editor` owns everything that is not pixels:
- The MapDocument and UndoStack
- The preferences
- The renderer, texture manager and viewport geometry
- The interaction state machine
- The state of every panel
- Every command the UI can invoke

The UI translation units (`menus.cpp`, `panels.cpp`, `dialogs.cpp`,
`context_menu.cpp`) are pure functions of that state: they read it, draw it and
call its commands.  This is not architectural taste.  The wxWidgets version
kept a second copy of the scenery level/rotate/scale settings and of the paint
colour inside the panel widgets, and they drifted, so those controls silently
did nothing.  With one owner that bug cannot be written.

### Coordinate pipeline

`core/viewport_geometry.h` holds the whole screen -> viewport -> world
conversion, headlessly testable, because getting it wrong is not cosmetic: the
wxWidgets build shipped a viewport sized from the window while the projection
was sized from the canvas (the map filled a corner of the window on Retina),
and a cursor mapping that applied the framebuffer scale (clicks missed as soon
as the display was scaled).  One `ViewportGeometry` now drives both the GL
rectangle and the mouse mapping:

- `glViewport`/`glScissor` are sized in **framebuffer pixels** (`glRect`)
- `glOrtho` is sized in **logical units**
- cursor positions are **logical units** and are only translated, never scaled

`consumeWheelNotches()` in the same header normalises high-resolution trackpad
scroll into whole wheel notches, which is what stops a single two-finger flick
on macOS from applying the zoom step dozens of times.

## Tool system

14 tools, indexed 0..13, with hotkeys:
```
0  MOVE        (Transform)     A
1  CREATE      (PolyCreation)  Q
2  VSELECT     (VertexSel)     S
3  PSELECT     (PolySel)       W
4  VCOLOR      (VertexColor)   D
5  PCOLOR      (PolyColor)     E
6  TEXTURE                     F
7  SCENERY                     R
8  WAYPOINT                    G
9  OBJECTS                     T
10 COLORPICKER                 H
11 SKETCH                      Y
12 LIGHTS                      J
13 DEPTHMAP                    U
```

Each tool has a cursor from `installer/skins/default/cursors/`.

## Build

```bash
# Linux / macOS
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j4

# Tests only (no display needed)
./build/bin/pw_tests

# Full app (requires OpenGL)
./build/bin/polyworks
```

## Testing

The headless test suite (`src-cpp/tests/pw_tests.cpp`) tests:
- Struct size assertions (binary compatibility)
- ARGB helpers
- Pascal-style string round-trips
- Geometry (snapToGrid, pointInPoly, winding, segments, zoom snapping)
- Coordinate conversion
- MapDocument: add/delete/move, screen cache
- PMS round-trips on all 97 maps in `maps/`
- PMS ↔ MapDocument round-trip
- UndoStack: push/undo/redo, depth limit, empty-stack safety

Run: `./build/bin/pw_tests`

## Behavioral fidelity notes

These VB6 behaviors are preserved intentionally:

1. **Delete requires all 3 vertices selected** — partial vertex selection does
   not delete the polygon.

2. **Spawn X/Y are integers** — stored as int32 in the file; converted to float
   in the editor but rounded when saving.

3. **Sector table is all zeros in native saves** — PolyWorks never computed
   the sector table for its own save format. Only the compile step fills it.

4. **mapRandomID=-1 triggers lights+sketch** — these sections are only present
   in native PolyWorks maps. Compiled maps have 8 trailing zero bytes instead.

5. **Waypoint connections always write 20 slots** — even if fewer are used.

6. **Scenery alpha=0 on load → 255** — VB6 remapped zero-alpha to opaque.

7. **VB6 Boolean in UDT is 2 bytes, -1=true** — reproduced in PmsProp.active
   and PmsCollider.active.

8. **PointInPoly only works for CW triangles** — VB6 assumes CW winding; all
   polygon creation enforces CW winding by swapping vertices if needed.
