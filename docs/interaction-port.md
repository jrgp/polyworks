# PolyWorks Interaction Port — Audit Document

This document records the original VB6 interaction model and the status of each
behavior in the modern C++/wxWidgets port.

**VB6 source reference:** `src/frmOpenSoldatMapEditor.frm`
**C++ implementation:** `src-cpp/gui/gl_viewport.cpp`, `src-cpp/core/map_document.cpp`

---

## 1. Modifier-Key → Effective Tool Mapping

VB6 uses two variables: `currentTool` (base tool from toolbar) and `currentFunction`
(live effective tool, updated on every key event). The C++ port mirrors this with
`m_activeTool` (base) and `m_currentFunction` (effective).

### Tool constant table

| Constant | Value | Description |
|---|---|---|
| TOOL_VSELECT | 1 | Vertex select (base) |
| TOOL_PSELECT | 2 | Polygon select (base) |
| TOOL_MOVE | 3 | Move / transform |
| TOOL_VCOLOR | 6 | Vertex color brush |
| TOOL_PCOLOR | 7 | Polygon color fill |
| TOOL_COLORPICK | 8 | Color picker (eyedropper) |
| TOOL_POLY | 9 | Polygon creation |
| TOOL_SCENERY | 10 | Scenery placement |
| TOOL_SPAWN | 11 | Spawn point placement |
| TOOL_WAYPOINT | 12 | Waypoint placement |
| TOOL_SKETCH | 13 | Sketch line drawing |
| TOOL_VSELADD | 14 | Virtual: VSELECT + Shift (add vertices) |
| TOOL_VSELSUB | 15 | Virtual: VSELECT + Alt (subtract vertices) |
| TOOL_PSELADD | 16 | Virtual: PSELECT + Shift (add polygons) |
| TOOL_PSELSUB | 17 | Virtual: PSELECT + Alt (subtract polygons) |
| TOOL_SCALE | 18 | Virtual: MOVE + Ctrl |
| TOOL_ROTATE | 19 | Virtual: MOVE + Alt |
| TOOL_CONNECT | 20 | Virtual: WAYPOINT + Shift |
| TOOL_HAND | 21 | Virtual: Space pan override |

### VB6 modifier mapping (lines 10715–10793)

| Base tool | Modifier | Effective function | Status |
|---|---|---|---|
| VSELECT | Shift | VSELADD | ✅ |
| VSELECT | Alt | VSELSUB | ✅ |
| PSELECT | Shift | PSELADD | ✅ |
| PSELECT | Alt | PSELSUB | ✅ |
| MOVE | Ctrl | SCALE | ✅ (dispatch exists; interactive scale ⬜) |
| MOVE | Alt | ROTATE | ✅ (dispatch exists; interactive rotate ⬜) |
| WAYPOINT | Shift | CONNECT | ✅ |
| VCOLOR/PCOLOR | Alt | COLORPICK | ✅ |
| Ctrl + any tool > MOVE | — | Temporarily MOVE | ✅ |
| Alt + other tools | — | Temporarily VSELECT | ✅ |
| Space (any tool) | — | HAND (pan) | ✅ |

`ComputeCurrentFunction()` in `gl_viewport.cpp` implements all of the above.
It is called on every `OnKeyDown` and `OnKeyUp` to keep `m_currentFunction` current.

---

## 2. Selection Model

### 2.1 SelectMode enum (C++ implementation)

```
SelectMode::Replace   — clear existing selection, then add
SelectMode::Add       — add to existing selection (Shift modifier)
SelectMode::Subtract  — remove from existing selection (Alt modifier)
```

Legacy `bool additive` overloads exist for backward compatibility.

### 2.2 Vertex selection semantics (VB6 `RegionSelPolys`, line 8515)

**Click selection (`selectVertexAt`):**
1. For each polygon, test `PointInPoly(click, poly)`.
2. If the click is inside a polygon, pick the nearest vertex of **that polygon only**.
3. If no polygon contains the click, fall back to global nearest-vertex search within snap radius.
4. Modifier determines `SelectMode`: Replace (plain), Add (Shift), Subtract (Alt).

This means clicking inside polygon A cannot accidentally select a nearer vertex in polygon B.

**Rubber-band selection (`selectVerticesInRect`):**
- Tests each vertex in world coordinates against the selection rectangle.
- Exclusive test (`x > rx1 && x < rx2 && y > ry1 && y < ry2`) — boundary points not included.
- `SelectMode` controls whether existing selection is replaced, added to, or subtracted from.

### 2.3 Original behavior vs implementation

| Behavior | Original (VB6) | C++ status |
|---|---|---|
| Click vertex in polygon | Poly-first hit test → nearest vertex of that poly | ✅ |
| Click outside all polys | Global nearest vertex (snap radius) | ✅ |
| Shift+click | Add vertex to selection | ✅ |
| Alt+click | Remove vertex from selection | ✅ |
| Rubber-band (plain) | Replace selection | ✅ |
| Shift+rubber-band | Add to selection | ✅ |
| Alt+rubber-band | Subtract from selection | ✅ |
| Cross-polygon rubber-band | Select vertices from multiple polys | ✅ |
| Cross-polygon modifier-click | Add/subtract vertices across polys | ✅ |
| Polygon body selection | `selectPolyAt` (PointInPoly) | ✅ |
| Polygon rubber-band | `selectPolysInRect` | ✅ |
| Partial-poly selection | Vertices selected independently of polygon | ✅ |
| Delete with partial selection | Poly deleted only when all 3 vertices selected | ✅ |

### 2.4 Selection visualization

| State | Rendering |
|---|---|
| Unselected vertex | Small white dot |
| Selected vertex | Highlighted (yellow/green accent) |
| Unselected polygon wire | Semi-transparent color |
| Selected polygon wire | Brighter / different color |
| Rubber-band rect | Dashed outline drawn during drag |

---

## 3. Dragging Selected Vertices

### 3.1 Drag lifecycle (C++ `HandleLeftDownEdit` / `HandleMouseMoveEdit` / `HandleLeftUpEdit`)

```
LMB Down  → identify vertex under cursor → push undo snapshot
           → set ViewportState::Dragging
           → record drag start position
LMB Move  → compute world-space delta from drag start
           → MapDocument::moveSelectedWorld(dx, dy)
           → viewport Refresh()
LMB Up    → if no actual movement → pop undo snapshot (no-op commit)
           → set ViewportState::Idle
```

### 3.2 Behavior table

| Behavior | Original | C++ status |
|---|---|---|
| Single vertex drag | Move vertex in world coords | ✅ |
| Multi-vertex drag | All selected vertices move together | ✅ |
| Cross-polygon multi-drag | All selected verts across polys move | ✅ |
| Coordinate system | World-space delta (not screen pixels) | ✅ |
| Zoom-invariant movement | Vertex follows cursor at any zoom level | ✅ |
| Click without drag | Undo snapshot popped (no change recorded) | ✅ |
| Arrow key nudge | 1 world unit; Shift = 10 world units | ✅ |
| Axis-constrained drag (Shift) | Not yet implemented | ⬜ |
| Snapping during drag | Grid snap via SnapSelectedToGrid | ⬜ (not triggered during drag) |

---

## 4. Color Tools

### 4.1 PCOLOR — Polygon Color Fill (VB6 `ColorFill`, line 9555)

**Behavior:**
- Single click on a polygon.
- If any vertices are currently selected → color ALL selected vertices.
- If nothing selected → color all 3 vertices of the clicked polygon.
- Uses blend formula from palette (Normal/Multiply/Screen/Darken/Lighten/Difference).
- Uses opacity from palette (0–100%).
- One undo entry per click.

**C++ implementation:** `MapDocument::applyColorToSelected()` and `applyColorToPolyAt()`,
dispatched from `HandleLeftDownEdit` when `m_currentFunction == TOOL_PCOLOR`.

| Behavior | Original | C++ status |
|---|---|---|
| Click → color polygon | ✅ | ✅ |
| Selection → color all selected | ✅ | ✅ |
| Palette blend mode | ✅ | ✅ (all 6 modes) |
| Palette opacity | ✅ | ✅ |
| Undo | ✅ | ✅ |

### 4.2 VCOLOR — Vertex Color Brush (VB6 `VertexColoring`, line 7496)

**Behavior:**
- Click-and-drag paint brush.
- Paints all vertices within `paintRadius / zoom` world units of the cursor.
- If any vertices are selected → only paints selected vertices (session-aware).
- If nothing selected → paints unselected vertices.
- Continuous painting while dragging (no delay between strokes).
- Uses palette color, opacity, blend mode, and radius.

**C++ implementation:** `MapDocument::applyColorToVerticesNear()`, triggered in
`HandleMouseMoveEdit` while `m_currentFunction == TOOL_VCOLOR` and state is `Dragging`.

| Behavior | Original | C++ status |
|---|---|---|
| Drag → paint vertices | ✅ | ✅ |
| Paint radius from palette | ✅ | ✅ |
| Respects selection (selected-only mode) | ✅ | ✅ |
| Respects selection (unselected-only mode) | ✅ | ✅ |
| Palette blend mode | ✅ | ✅ (all 6 modes) |
| Alt → COLORPICK (eyedropper) | ✅ | ✅ (virtual tool switch) |
| Undo one entry per drag session | ✅ | ✅ |

### 4.3 BlendColor formula (VB6 `ApplyBlend`, line 9653)

```
result_channel = blend_fn(dest, src) * opacity + dest * (1 - opacity)
```

| Mode | Formula |
|---|---|
| 0 Normal | result = src |
| 1 Multiply | result = (dest × src) / 255 |
| 2 Screen | result = 255 - ((255-dest) × (255-src)) / 255 |
| 3 Darken | result = min(dest, src) |
| 4 Lighten | result = max(dest, src) |
| 5 Difference | result = abs(dest - src) |

All 6 modes implemented in `MapDocument::blendColor()`.

### 4.4 Palette → Viewport color sync

**Flow:**
```
PalettePanel (color/opacity/blend/radius change)
    → onColorSelected / onColorChanged / onChannelChanged / onBlendModeChanged / onRadiusChanged callback
    → MainFrame::AttachPalettePanel lambda
    → GlViewport::setPaintColor(r, g, b, opacity, blendMode, radius)
    → m_paintR/G/B/Opacity/BlendMode/Radius stored in GlViewport
    → used by applyColorToVerticesNear / applyColorToSelected / applyColorToPolyAt
```

**Status:** ✅ fully wired in `mainframe.cpp::AttachPalettePanel`.

---

## 5. Undo / Redo Semantics

### 5.1 Pre-save pattern

Undo snapshots are pushed **before** the destructive operation (not after):

```cpp
m_undoStack.push(m_document);   // snapshot current state
// ... perform edit ...
// if no actual change happened: m_undoStack.pop()  (click-without-drag)
```

### 5.2 Granularity

| Operation | Undo entries |
|---|---|
| Move N vertices (any drag) | 1 entry (pushed at LMB down) |
| PCOLOR click | 1 entry per click |
| VCOLOR drag session | 1 entry per mouse-down (entire drag = 1 entry) |
| Polygon creation | 1 entry per polygon |
| Spawn/scenery/waypoint placement | 1 entry per placement |
| Delete selected | 1 entry |
| Multi-polygon move | 1 entry |

### 5.3 What undo does NOT restore (matching VB6)

- Zoom level
- Scroll position

---

## 6. Keyboard Shortcuts

### 6.1 File / Application

| Key | Action | Status |
|---|---|---|
| Ctrl+N | New map | ✅ |
| Ctrl+O | Open map | ✅ |
| Ctrl+S | Save | ✅ |
| Ctrl+Shift+S | Save As | ✅ |
| F9 | Save and Compile | ✅ |
| F8 / Shift+F8 | Run Soldat/OpenSoldat | ✅ (executable discovered under the configured game directory) |
| Alt+F4 | Exit | ✅ |
| F5 | Refresh | ✅ (`wxID_REFRESH`; the background quad is recomputed every frame so no explicit refresh is required) |
| F1 | Help | ⬜ N/A — the original has no Help menu or F1 handler (verified: the `.frm` menu tree has no `mnuHelp*` entry) |

### 6.2 Edit

| Key | Action | Status |
|---|---|---|
| Ctrl+Z | Undo | ✅ |
| Ctrl+Y | Redo | ✅ |
| Ctrl+A | Select All | ✅ |
| Ctrl+D | Deselect All | ✅ |
| Delete | Delete selected | ✅ |
| Ctrl+C / V | Copy / Paste | ✅ — writes/reads a prefab in the temp directory, matching `mnuCopy_Click` (frm:11769) / `mnuPaste_Click` (frm:12062) which used `<app>\Temp\copy.PFB` |
| Ctrl+I | Invert Selection | ✅ |
| Ctrl+D | Duplicate | ✅ |
| Tab / Shift+Tab | Cycle selection | ✅ — `MapDocument::cycleSelection`, ported from `TabPressed` (frm:6070) |
| K / I / M / N | Waypoint direction flags | ✅ — `modConfig.bas:179-183`.  The original's `J` (Left) is unreachable because the tool-hotkey check runs first; that ordering is preserved |
| Numpad 1-8 | Toggle display layers | ✅ — `modConfig.bas:186-193` |

### 6.3 Tool hotkeys (default VB6 key bindings)

| Key | Tool | Status |
|---|---|---|
Defaults from `modConfig.bas:162-175` (DirectInput scancodes).

| Key | Tool | Status |
|---|---|---|
| A | Move | ✅ |
| Q | Create | ✅ |
| S | Vertex Selection | ✅ |
| W | Polygon Selection | ✅ |
| D | Vertex Color | ✅ |
| E | Polygon Color | ✅ |
| F | Texture | ✅ |
| R | Scenery | ✅ |
| G | Waypoints | ✅ |
| T | Objects | ✅ |
| H | Color Picker | ✅ |
| Y | Sketch | ✅ |
| J | Lights | ✅ |
| U | Depthmap | ✅ |

Hotkeys are not user-remappable in the port; the original's Preferences
"HotKeys" / "Waypoint Keys" pages let the user rebind them.  The defaults are
reproduced exactly.

### 6.4 Navigation

| Key | Action | Status |
|---|---|---|
| Arrow keys (no selection) | Scroll viewport | ✅ |
| Arrow keys (with selection) | Nudge 1 world unit | ✅ |
| Shift+Arrow | Nudge 10 world units | ✅ |
| Space (held) | Pan cursor | ✅ |
| Space + LMB drag | Pan viewport | ✅ |
| Escape | Cancel / deselect | ✅ |
| Ctrl+= / Ctrl+- | Zoom in/out | ✅ |
| Ctrl+0 | Reset zoom | ✅ |

---

## 6a. Zoom, Wheel and Viewport Coordinates

### Mouse wheel (`MouseHelper_MouseWheel`, `frm:12733` → `ZoomScroll`, `frm:4108`)

The original multiplies `zoomFactor` by **1.25** for one wheel notch forward and
**0.8** for one notch backward — it does *not* jump between power-of-two zoom
levels (that is what the numpad `+` / `-` keys do, via `GetZoomDir`).

`ZoomScroll` clamps against `gMinZoom` / `gMaxZoom` in two stages: a step that
would overshoot a limit is *shortened* so it lands exactly on the limit, but if
the limit has already been reached the whole gesture is discarded
(`frm:4113-4119`).

The scroll adjustment is asymmetric (`frm:4129-4135`):

| Direction | Original expression | Equivalent behaviour |
|---|---|---|
| In (`zoomDir > 1`) | `scroll += X / zoomFactor / ((2 / (zoomDir - 1)) / 2)` | keeps the world point **under the cursor** fixed |
| Out (`zoomDir < 1`) | `scroll -= ScaleWidth / zoomFactor / (2 / (1 - zoomDir))` | keeps the world point at the **viewport centre** fixed |

Both expressions reduce algebraically to an exact anchored zoom; the only
difference is which screen point is used as the anchor. Zooming out therefore
ignores the cursor entirely. This asymmetry is reproduced verbatim in
`MapDocument::zoomScroll()` and covered by
`zoomscroll_in_anchors_cursor` / `zoomscroll_out_anchors_viewport_centre`.

**High-resolution devices.** macOS trackpads, precision touchpads and free-spin
mice report many sub-notch rotations per gesture. `GlViewport::OnMouseWheel`
accumulates `GetWheelRotation()` and only applies a 1.25/0.8 step once a full
`GetWheelDelta()` has been travelled, resetting the accumulator when the
direction reverses. Horizontal wheel events (`wxMOUSE_WHEEL_HORIZONTAL`) never
zoom. Without this accumulation a single two-finger flick applied dozens of
zoom steps.

### Screen ↔ world transform

There is exactly one transform pair, on `MapDocument`:

```
screen = (world - scroll) * zoom
world  = screen / zoom + scroll
```

All screen coordinates in the editor — mouse event positions, the cached
`EditorVertex::screen` values, the renderer's vertex output and the ortho
projection — are in **wxWidgets logical units (points)**, never physical device
pixels. Hit-test tolerances that are specified in pixels are converted with
`pixels / zoom` (`GlViewport::WorldTolerance()`, the paint radii, the waypoint
connect radius).

### HiDPI / backing-store scaling

The OpenGL back buffer is allocated in **physical device pixels**, while
`GetClientSize()` reports logical points. On a scaled display (macOS Retina,
GTK/Wayland `scale-factor`, Windows per-monitor DPI) the two differ.
`GlViewport::OnPaint` therefore sizes `glViewport()` with
`GetClientSize() * GetContentScaleFactor()` but keeps `glOrtho()` in logical
points, so the world↔screen math above is unaffected by the display scale.

Using the logical size for `glViewport()` produced a characteristic failure on
Retina displays: because GL's origin is bottom-left, the whole scene was drawn
at half scale into the **lower-left quadrant** of the canvas, and mouse
coordinates — which are always logical — no longer matched anything that was
visible, so selection appeared to ignore the zoom level.

---

## 7. Mouse Hit Testing Priority

When objects overlap, the original VB6 hit-test priority (inferred from code order):

1. Vertices (nearest within snap radius)
2. Polygon body (PointInPoly)
3. Scenery
4. Spawn points
5. Colliders
6. Waypoints
7. Lights

The C++ implementation checks in approximately this order, dispatched per-tool.

---

## 8. Custom Cursors

All `.cur` files from `installer/skins/default/cursors/`.

| Cursor | Tool | Status |
|---|---|---|
| `vselect.cur` | VSELECT | ✅ Windows; fallback on Linux/macOS |
| `create.cur` | POLY | ✅ Windows; fallback on Linux/macOS |
| `hand.cur` | Pan (Space/Middle) | ✅ Windows; fallback on Linux/macOS |
| `scenery.cur` | SCENERY | ✅ Windows; fallback on Linux/macOS |
| `objects.cur` | SPAWN/COLLIDER | ✅ Windows; fallback on Linux/macOS |
| `waypoint.cur` | WAYPOINT | ✅ Windows; fallback on Linux/macOS |
| `light.cur` | LIGHT | ✅ Windows; fallback on Linux/macOS |
| `sketch.cur` | SKETCH | ✅ Windows; fallback on Linux/macOS |
| `vseladd.cur` | VSELADD | ✅ |
| `vselsub.cur` | VSELSUB | ✅ |
| `pseladd.cur` | PSELADD | ✅ |
| `pselsub.cur` | PSELSUB | ✅ |
| `vcolor.cur` | VCOLOR | ✅ |
| `pcolor.cur` | PCOLOR | ✅ |
| `scale.cur` | SCALE | ✅ |
| `rotate.cur` | ROTATE | ✅ |
| `texture.cur` | TEXTURE | ✅ |
| `connect.cur` | CONNECT | ✅ |
| `pixpicker.cur` | COLORPICK | ✅ |
| `litpicker.cur` | LITPICK | ✅ |
| `depthmap.cur` | DEPTHMAP | ✅ |
| `smudge.cur` | SMUDGE | ✅ |
| `eraser.cur` | ERASER | ✅ |

All 27 cursors are keyed off `currentFunction` (the modifier-adjusted tool),
matching `SetCursor currentFunction + 1` at frm:5037 -- *not* off the palette
tool.  Platforms whose native cursor loader rejects the Windows `.cur` files
fall back to the nearest stock cursor.

---

## 9. Legend

| Symbol | Meaning |
|---|---|
| ✅ | Implemented and wired |
| ⚠️ | Partially implemented / simplified from original |
| ⬜ TODO | Not yet implemented (no entries remain) |
| ⬜ N/A | Not applicable (platform-specific feature not being ported) |
---

## Interaction behaviour documented by the forensic audit

Behaviour verified directly against the original VB6 sources during the
independent re-audit. These were previously undocumented, and in each case the
C++ implementation disagreed with the original until fixed.

### Transform tools (Scale / Rotate)

`ComputeCurrentFunction` selects `TOOL_SCALE` (Ctrl-drag) and `TOOL_ROTATE`
(Alt-drag), but `HandleLeftDownEdit` had no case for either, so both were
silent no-ops. Now handled by the `Transforming` viewport state.

- **Centre of transform** — VB6 `rCenter` defaults to the midpoint of the
  selection bounding rectangle (`selRect`), not the centroid.
- **Rotation** (`Rotating`, `frm:7355`) — angle delta via `atan2`. With Shift
  held the angle is quantised to 15° steps as
  `floor((deg + 7.5) / 15) * 15`.
- **Scaling** (`Scaling`, `frm:7170`) — unconstrained by default;
  Ctrl+Shift constrains to proportional scaling.
- Both are applied by `ApplyTransform` (`frm:7245`) as **scale-then-rotate**
  about the centre. The C++ port applies the transform to geometry captured at
  drag start (`TransformSession`) rather than incrementally, which avoids
  cumulative floating-point drift over a long drag.

### Movement

- `Moving()` (`frm:7076`) takes **screen-space** deltas.
- A step of `n = zoomFactor` corresponds to exactly **1 world unit**.
- With Shift, `n = gridSpacing / gridDivisions * zoomFactor`, i.e.
  `gridSpacing / gridDivisions` world units.
- Shift during a drag constrains movement to a single axis; the axis with the
  larger delta wins.

### Arrow-key nudge

`frm:11007-11010`: 1 world unit per press, or `gridSpacing / gridDivisions`
with Shift. The port previously had **two** competing handlers — one on the
viewport (`1/zoom`, ×10 with Shift, no undo) and one on the main frame
(`1.0`/`10.0`, with undo) — so the step size and undo behaviour depended on
which widget had focus. The viewport handler has been removed.

### Snapping

`SnapSelected` (`frm:8246`) runs on **mouse-up**, not continuously during the
drag.

- Grid snapping takes priority over vertex snapping
  (`If snapToGrid And showGrid ... ElseIf ohSnap ...`).
- Grid snapping is gated on the grid actually being **visible**.
- Vertex snapping never snaps to a selected vertex, and refuses to act if the
  selected vertices do not all share the anchor's coordinates.

Both `snapToGrid` and `snapToVertices` were previously written by the menu
handlers but never read anywhere in the port.

### Duplicate

`mnuDuplicate` (`frm:13147`) offsets the duplicate by **+32 in X only** — not
by X and Y, and not by 10.

### Polygon properties

`GetInfo` (`frm:4668`) drives the property panel, guarded by `frmInfo.noChange`
to stop control-update events writing back into the model. Notable rules:

- Bounciness is displayed as `Int((Perp.Z - 1) * 100)`, clamped to ≥ 0.
- The bounciness field is **enabled only for `polyType == 18` (Bouncy)**.
- Values are formatted with VB6's `Int(x * m + 0.5) / m` rounding, which the
  port reproduces so displayed numbers match the original.

### View reset on load

`LoadFile` (`frm:1935-1939`) resets `zoomFactor = 1` and
`scroll = (-ScaleWidth/2, -ScaleHeight/2)`, placing world origin at the centre
of the viewport. Soldat maps are built around the origin, so this frames the
map on open.

### Command-line / file-association open

`Form_Load` (`frm:10648-10670`) opens a `.pms` passed on the command line,
stripping one pair of surrounding quotes and resolving the name against, in
order: the path as given, `<appPath>/Maps/`, then `<OpenSoldatDir>/Maps/`.
The installer registers this as the `.pms` handler (`installer/pw.nsi:185`).

### Unsaved-changes guard

The original never discards a modified map silently. `mnuNew_Click`
(`frm:12752`), `mnuOpen_Click` (`frm:12772`) and `Terminate` (`frm:4381`) all
call the same `prompt` helper, which shows a three-way message box:

| Answer | Behaviour |
|---|---|
| Yes | Save the map, then continue with the operation |
| No | Discard the changes and continue |
| Cancel | Abort — no new map, no open dialog, and the application stays running |

The port reproduces this in `MainFrame::ConfirmDiscardChanges()`. Closing the
window routes through `wxEVT_CLOSE_WINDOW` so that Cancel can veto the close;
`File > Exit` therefore calls `Close(false)` rather than destroying the frame
directly. On the non-cancelled path the close handler performs the original's
`SaveSettings` shutdown work: preferences are written and the colour palette is
saved back to `palettes/current.txt`.

### Startup and shutdown state

`appPath = App.Path` (`modConfig.bas:52`) anchors every static resource to the
executable's own directory, and the original keeps all of its state there:

| File | Read | Written |
|---|---|---|
| `polyworks.ini` | startup (`LoadConfig`) | exit (`SaveSettings`) |
| `palettes/current.txt` | `frmPalette.Form_Load` (`frm:867`) | exit (`modConfig.bas:389`) |
| `Workspace/current.ini` | startup | exit |

The port matches this on Windows. On Linux and macOS the settings file is used
only when a `polyworks.ini` already exists beside the executable, so that the
platform's own configuration conventions apply by default while portable mode
remains available on request; a read-only application directory always falls
back to the per-user store.
