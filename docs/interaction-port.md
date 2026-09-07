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
| F8 / Shift+F8 | Run Soldat/OpenSoldat | ⬜ N/A (platform feature) |
| Alt+F4 | Exit | ✅ |
| F1 | Help | ⬜ TODO |

### 6.2 Edit

| Key | Action | Status |
|---|---|---|
| Ctrl+Z | Undo | ✅ |
| Ctrl+Y | Redo | ✅ |
| Ctrl+A | Select All | ✅ |
| Ctrl+D | Deselect All | ✅ |
| Delete | Delete selected | ✅ |
| Ctrl+C / V | Copy / Paste | ⬜ TODO |
| Ctrl+I | Invert Selection | ⬜ TODO |
| Shift+Insert | Duplicate | ⬜ TODO |

### 6.3 Tool hotkeys (default VB6 key bindings)

| Key | Tool | Status |
|---|---|---|
| M | MOVE / Transform | ✅ |
| C | Polygon Creation | ✅ |
| Y | Scenery | ✅ |
| O | Spawn / Objects | ✅ |
| T | Waypoints | ✅ |
| L | Lights | ✅ |
| . | Sketch | ✅ |

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
| `vseladd.cur` | VSELADD | ⬜ TODO (uses vselect fallback) |
| `vselsub.cur` | VSELSUB | ⬜ TODO (uses vselect fallback) |
| `pseladd.cur` | PSELADD | ⬜ TODO (uses vselect fallback) |
| `pselsub.cur` | PSELSUB | ⬜ TODO (uses vselect fallback) |
| `vcolor.cur` | VCOLOR | ⬜ TODO |
| `pcolor.cur` | PCOLOR | ⬜ TODO |
| `scale.cur` | SCALE | ⬜ TODO |
| `rotate.cur` | ROTATE | ⬜ TODO |
| `texture.cur` | TEXTURE | ⬜ TODO |
| `connect.cur` | CONNECT | ⬜ TODO |
| `pixpicker.cur` | COLORPICK | ⬜ TODO |
| `litpicker.cur` | LITPICK | ⬜ TODO |
| `depthmap.cur` | DEPTHMAP | ⬜ TODO |
| `smudge.cur` | SMUDGE | ⬜ TODO |
| `eraser.cur` | ERASER | ⬜ TODO |

---

## 9. Legend

| Symbol | Meaning |
|---|---|
| ✅ | Implemented and wired |
| ⚠️ | Partially implemented / simplified from original |
| ⬜ TODO | Not yet implemented |
| ⬜ N/A | Not applicable (platform-specific feature not being ported) |


This document catalogs every keyboard shortcut, mouse interaction, and cursor behavior
from the original VB6 source (`src/frmOpenSoldatMapEditor.frm` and related forms) and
maps each to its modern Lazarus/LCL implementation status.

---

## 1. Keyboard Shortcuts

### 1.1 File / Application

| Original (VB6) | Action | Modern Implementation | Status |
|---|---|---|---|
| Ctrl+N | New map | `frmmain.pas → NewFile` menu item, `ShortCut(VK_N,[ssCtrl])` | ✅ |
| Ctrl+O | Open map | `frmmain.pas → OpenFile` menu item, `ShortCut(VK_O,[ssCtrl])` | ✅ |
| Ctrl+Shift+O | Open compiled | Not yet wired (no OpenCompiled action) | ⚠️ Partial |
| Ctrl+S | Save | `frmmain.pas → SaveFile` menu item | ✅ |
| Ctrl+Shift+S | Save As | `frmmain.pas → SaveFileAs`, `ShortCut(VK_S,[ssCtrl,ssShift])` | ✅ |
| F9 | Save and Compile | `frmmain.pas → SaveAndCompileFile`, `ShortCut(VK_F9,[])` | ✅ |
| F8 | Run OpenSoldat | Not implemented (out-of-scope platform feature) | ⬜ N/A |
| Shift+F8 | Run Soldat | Not implemented (out-of-scope platform feature) | ⬜ N/A |
| Alt+F4 | Exit | `frmmain.pas → QuitApp`, `ShortCut(VK_F4,[ssAlt])` | ✅ |
| F1 | Help | Not yet implemented | ⬜ TODO |
| F5 | Refresh background | Not yet implemented | ⬜ TODO |

### 1.2 Edit Operations

| Original (VB6) | Action | Modern Implementation | Status |
|---|---|---|---|
| Ctrl+Z | Undo | `viewport.pas KeyDown` + `frmmain.pas UndoAction` menu | ✅ |
| Ctrl+Y | Redo | `viewport.pas KeyDown` + `frmmain.pas RedoAction` menu | ✅ |
| Ctrl+C | Copy | Not yet implemented (future) | ⬜ TODO |
| Ctrl+V | Paste | Not yet implemented (future) | ⬜ TODO |
| Ctrl+A | Select All | `viewport.pas KeyDown` + `frmmain.pas SelectAllAction` menu | ✅ |
| Ctrl+D | Deselect All | `viewport.pas KeyDown` (Ctrl+D) + `frmmain.pas DeselectAllAction` | ✅ |
| Ctrl+I | Invert Selection | Not yet implemented | ⬜ TODO |
| Ctrl+B | Select by Color | Not yet implemented | ⬜ TODO |
| Delete | Delete selected | `viewport.pas KeyDown` + `frmmain.pas DeleteAction` menu | ✅ |
| Shift+Insert | Duplicate | Not yet implemented | ⬜ TODO |
| Backspace | Sever (disconnect waypoints) | Not yet implemented | ⬜ TODO |

### 1.3 Polygon Operations

| Original (VB6) | Action | Modern Implementation | Status |
|---|---|---|---|
| Ctrl+E | Create polygon from selection | Not yet implemented | ⬜ TODO |
| Ctrl+L | Split at vertex | Not yet implemented | ⬜ TODO |
| Ctrl+J | Join vertices | Not yet implemented | ⬜ TODO |
| Ctrl+U | Untexture | Not yet implemented | ⬜ TODO |
| Ctrl+F | Fix texture | Not yet implemented | ⬜ TODO |
| Ctrl+T | Auto-texture | Not yet implemented | ⬜ TODO |
| Ctrl+G | Average vertex colors | Not yet implemented | ⬜ TODO |
| Home | Bring to front | Not yet implemented | ⬜ TODO |
| End | Send to back | Not yet implemented | ⬜ TODO |
| Page Up | Bring forward | Not yet implemented | ⬜ TODO |
| Page Down | Send backward | Not yet implemented | ⬜ TODO |

### 1.4 Settings

| Original (VB6) | Action | Modern Implementation | Status |
|---|---|---|---|
| Ctrl+M | Map Properties | `frmmain.pas MapPropertiesAction`, `ShortCut(VK_M,[ssCtrl])` | ✅ |
| Ctrl+P | Preferences | `frmmain.pas PreferencesAction`, `ShortCut(VK_P,[ssCtrl])` | ✅ |
| Ctrl+' | Toggle grid | Not yet wired (display setting exists) | ⬜ TODO |

### 1.5 View / Zoom

| Original (VB6) | Action | Modern Implementation | Status |
|---|---|---|---|
| Ctrl+= / Ctrl++ | Zoom in | `viewport.pas KeyDown → ZoomBy(2.0)` | ✅ |
| Ctrl+- | Zoom out | `viewport.pas KeyDown → ZoomBy(0.5)` | ✅ |
| Ctrl+0 | Reset zoom | `viewport.pas KeyDown → ZoomReset` | ✅ |
| Numpad+ | Zoom in | `viewport.pas KeyDown VK_ADD` | ✅ |
| Numpad- | Zoom out | `viewport.pas KeyDown VK_SUBTRACT` | ✅ |
| Numpad* | Reset zoom | `viewport.pas KeyDown VK_MULTIPLY` | ✅ |
| Zoom In menu | Zoom in | `frmmain.pas ZoomInAction`, `ShortCut(VK_ADD,[])` | ✅ |
| Zoom Out menu | Zoom out | `frmmain.pas ZoomOutAction`, `ShortCut(VK_SUBTRACT,[])` | ✅ |

### 1.6 Tool Selection Hotkeys

| Original (VB6) key | Tool | Modern Implementation | Status |
|---|---|---|---|
| M | Transform/Move (TOOL_SELECT) | `viewport.pas KeyDown Ord('M')` | ✅ |
| C | Polygon Creation (TOOL_POLY) | `viewport.pas KeyDown Ord('C')` | ✅ |
| Y | Scenery (TOOL_SCENERY) | `viewport.pas KeyDown Ord('Y')` | ✅ |
| O | Objects/Spawn (TOOL_SPAWN) | `viewport.pas KeyDown Ord('O')` | ✅ |
| T | Waypoints (TOOL_WAYPOINT) | `viewport.pas KeyDown Ord('T')` | ✅ |
| L | Lights (TOOL_LIGHT) | `viewport.pas KeyDown Ord('L')` | ✅ |
| . (period) | Sketch (TOOL_SKETCH) | `viewport.pas KeyDown Ord('.')` | ✅ |
| [ | Previous tool | `viewport.pas KeyDown Ord('[')` | ✅ |
| ] | Next tool | `viewport.pas KeyDown Ord(']')` | ✅ |

**Note:** VB6 tool hotkeys are configurable in Preferences; the above are the defaults.
The modern version uses fixed hotkeys. A preferences-configurable hotkey system is a
future enhancement.

### 1.7 Navigation and Editing

| Original (VB6) | Action | Modern Implementation | Status |
|---|---|---|---|
| Arrow keys (no selection) | Scroll viewport | `viewport.pas KeyDown → Doc.Scroll` | ✅ |
| Arrow keys (with selection) | Nudge selected entities by 1 world unit | `viewport.pas KeyDown → NudgeSelection` | ✅ |
| Shift+Arrow keys | Nudge by grid size step | `viewport.pas KeyDown ssShift` | ✅ |
| Space (held) | Pan mode cursor | `viewport.pas KeyDown VK_SPACE → FSpaceDown` | ✅ |
| Space+LMB drag | Pan viewport | `viewport.pas MouseDown/Move with FSpaceDown` | ✅ |
| Escape | Cancel tool action / deselect all | `viewport.pas KeyDown VK_ESCAPE → Tool.Cancel + ClearSelection` | ✅ |
| Tab | Tab through objects | Not yet implemented | ⬜ TODO |

---

## 2. Mouse Interactions

### 2.1 Viewport buttons

| Button | Condition | Action | Modern Implementation | Status |
|---|---|---|---|---|
| Left | Normal | Tool action (depends on active tool) | `viewport.pas MouseDown → Tool.MouseDown` | ✅ |
| Left | Space held | Pan viewport | `viewport.pas MouseDown → FPanActive` | ✅ |
| Middle | Any | Pan viewport | `viewport.pas MouseDown mbMiddle → FPanActive` | ✅ |
| Right | Any | Context menu / cancel tool action | `viewport.pas MouseDown mbRight → PopupMenu` | ✅ |
| Wheel up | Any | Zoom in (×1.25) at cursor | `viewport.pas DoMouseWheel` | ✅ |
| Wheel down | Any | Zoom out (÷1.25) at cursor | `viewport.pas DoMouseWheel` | ✅ |

### 2.2 Per-tool right-click behavior (original VB6)

The original had per-tool popup menus on right-click. The modern implementation uses a
single generic context menu (Select All / Deselect All / Delete). Full per-tool right-click
menus are a future enhancement.

| Tool | Original right-click | Modern | Status |
|---|---|---|---|
| TOOL_CREATE/QUAD | Poly type popup (mnuPolyTypes) | Generic context menu | ⚠️ Simplified |
| TOOL_MOVE | Move popup (mnuMove) | Generic context menu | ⚠️ Simplified |
| TOOL_PSELECT/VSELECT | Vertex select popup (mnuVertexSelect) | Generic context menu | ⚠️ Simplified |
| TOOL_SCENERY | Toggle scenery tree at mouse pos | Generic context menu | ⚠️ Simplified |
| TOOL_OBJECTS | Objects popup (mnuObjects) | Generic context menu | ⚠️ Simplified |
| TOOL_WAYPOINT | Waypoint popup (mnuWaypoint) | Generic context menu | ⚠️ Simplified |

### 2.3 Mouse modifier keys

| Interaction | Modifier | Effect | Status |
|---|---|---|---|
| Poly creation drag | Shift held | Constrain angle (ConstrainAngle) | Implemented in TPolyTool |
| Move drag | Shift held | Constrain to axis | ⬜ TODO |
| Scale drag | Ctrl held | Unconstrained scale | ⬜ TODO |
| Scale drag | Ctrl+Shift | Constrained scale | ⬜ TODO |
| Rotate drag | Alt held | Unconstrained rotate | ⬜ TODO |
| Rotate drag | Shift+Alt | Constrained rotate | ⬜ TODO |
| Vertex select | Shift held | Add to selection | ✅ (TSelectTool) |
| Vertex select | Ctrl held | Subtract from selection | ✅ (TSelectTool) |

### 2.4 Double-click

| Tool | Original | Modern | Status |
|---|---|---|---|
| TOOL_CREATE | Sets toolAction=True (commits polygon) | Double-click → close polygon in TPolyTool | ✅ |

---

## 3. Custom Cursors

### 3.1 Original cursor files

All from `installer/skins/default/cursors/`:

| File | Used for | Modern cursor ID | Status |
|---|---|---|---|
| `vselect.cur` | TOOL_SELECT | `CUR_SELECT = -100` | Windows: ✅ loaded; Linux/macOS: crArrow fallback |
| `create.cur` | TOOL_POLY | `CUR_POLY = -101` | Windows: ✅ loaded; Linux/macOS: crCross fallback |
| `scenery.cur` | TOOL_SCENERY | `CUR_SCENERY = -102` | Windows: ✅ loaded; Linux/macOS: crCross fallback |
| `objects.cur` | TOOL_SPAWN, TOOL_COLLIDER | `CUR_SPAWN = -103` | Windows: ✅ loaded; Linux/macOS: crCross fallback |
| `waypoint.cur` | TOOL_WAYPOINT | `CUR_WAYPOINT = -104` | Windows: ✅ loaded; Linux/macOS: crCross fallback |
| `light.cur` | TOOL_LIGHT | `CUR_LIGHT = -105` | Windows: ✅ loaded; Linux/macOS: crCross fallback |
| `sketch.cur` | TOOL_SKETCH | `CUR_SKETCH = -106` | Windows: ✅ loaded; Linux/macOS: crCross fallback |
| `hand.cur` | Pan mode (space/middle held) | `CUR_HAND = -107` | Windows: ✅ loaded; Linux/macOS: crHandPoint fallback |
| `move.cur` | TOOL_MOVE variants | (not separately mapped) | ⬜ TODO |
| `pselect.cur` | TOOL_PSELECT variants | (not separately mapped) | ⬜ TODO |
| `vcolor.cur` | vertex color tool | (not separately mapped) | ⬜ TODO |
| `pcolor.cur` | poly color tool | (not separately mapped) | ⬜ TODO |
| `texture.cur` | texture tool | (not separately mapped) | ⬜ TODO |
| `connect.cur` | TOOL_CONNECTION | (not separately mapped) | ⬜ TODO |
| `quad.cur` | quad creation mode | (not separately mapped) | ⬜ TODO |
| `scale.cur` | scale tool | (not separately mapped) | ⬜ TODO |
| `rotate.cur` | rotate tool | (not separately mapped) | ⬜ TODO |
| `eraser.cur` | eraser sub-tool | (not separately mapped) | ⬜ TODO |
| `smudge.cur` | smudge sub-tool | (not separately mapped) | ⬜ TODO |
| `pixpicker.cur` | pixel color picker | (not separately mapped) | ⬜ TODO |
| `litpicker.cur` | light picker | (not separately mapped) | ⬜ TODO |
| `depthmap.cur` | depth map tool | (not separately mapped) | ⬜ TODO |
| `color_picker.cur` | color picker (duplicate) | (not separately mapped) | ⬜ TODO |
| `vseladd.cur` | vertex select add | (not separately mapped) | ⬜ TODO |
| `vselsub.cur` | vertex select subtract | (not separately mapped) | ⬜ TODO |
| `pseladd.cur` | poly select add | (not separately mapped) | ⬜ TODO |
| `pselsub.cur` | poly select subtract | (not separately mapped) | ⬜ TODO |

### 3.2 Cross-platform cursor loading

- **Windows:** `LoadCursorFromFile` (WinAPI) called via `{$IFDEF WINDOWS}` in `viewport.pas`.
  Registered into `Screen.Cursors[CUR_*]` at application first-show.
- **Linux/macOS:** `.cur` files are not loaded; built-in LCL cursors used as fallbacks.
  `crArrow` for selection, `crCross` for creation tools, `crHandPoint` for pan mode.

The loading is triggered in `frmmain.pas → HandleFirstShow → LoadAllCursors(PWSkinsDir + 'cursors/')`.

---

## 4. Keyboard Focus Behavior

### 4.1 Focus policy

- The map viewport (`TMapViewport`) is `TabStop := True` and calls `SetFocus` on every `MouseDown`.
- All global shortcuts (Ctrl+Z, tool keys, Delete, Arrow, Space) are handled in
  `viewport.pas KeyDown`, which fires when the viewport has focus.
- Menu shortcuts (Ctrl+N, Ctrl+S, etc.) fire regardless of focus via LCL menu accelerators.

### 4.2 Focus conflict with floating panels

- Floating panels (frmtools, frminfo, etc.) are `TForm` descendants with `BorderStyle = bsNone`.
- Clicking a tool button calls back into viewport via `OnToolSelect` and then calls
  `FViewport.SetFocus` explicitly so keyboard shortcuts continue to work.
- Text fields in dialogs (map properties, preferences): shortcuts are suppressed by LCL's
  standard behavior while a `TEdit` has focus. This matches VB6 behavior.

---

## 5. Tool Interaction State Machines

### 5.1 TOOL_SELECT (Transform/Move)

| State | Trigger | Action |
|---|---|---|
| Idle | LMB click on vertex/entity | Select nearest, begin drag |
| Drag | Mouse move with LMB | Move selected entities (MoveSelectedWorld) |
| Drag + Shift | Mouse move | (TODO: axis-constrained drag) |
| Release | LMB up | Commit move, push undo |
| Select rect | LMB drag on empty space | Rubber-band selection |
| Shift+rect | LMB drag on empty + Shift | Add to selection |
| Ctrl+rect | LMB drag on empty + Ctrl | Subtract from selection |
| Cancel | Escape | Deselect all, abort drag |

### 5.2 TOOL_POLY (Polygon Creation)

| State | Trigger | Action |
|---|---|---|
| Idle | LMB click | Place first vertex |
| 1 vertex | LMB click | Place second vertex |
| 2 vertices | LMB click | Place third vertex, create polygon |
| Any | Double-click | Close/commit polygon |
| Any + Shift | LMB click | Constrain angle (snap to 45°) |
| Any | Escape | Cancel, discard in-progress polygon |

### 5.3 TOOL_SCENERY, TOOL_SPAWN, TOOL_WAYPOINT, TOOL_LIGHT, TOOL_SKETCH

Single-click placement tools. On LMB click: place entity at world coordinate, push undo.
Escape cancels any pending in-progress state.

---

## 6. Implementation Source References

- `viewport.pas` — `KeyDown`, `KeyUp`, `MouseDown`, `MouseMove`, `MouseUp`, `DoMouseWheel`
- `frmmain.pas` — menu shortcuts (`BuildMenus`), `FloatingToolSelect`, `ViewportToolSelect`
- `tools.pas` — `TSelectTool`, `TPolyTool`, `TSketchTool`, `TBaseTool.Cancel`
- `pw.map.pas` — `MoveSelectedWorld`, `DeleteSelected`, `ClearSelection`
- `pw.undo.pas` — `TUndoStack.Push`, `Undo`, `Redo`

---

## 7. Legend

| Symbol | Meaning |
|---|---|
| ✅ | Implemented and wired |
| ⚠️ | Partially implemented / simplified from original |
| ⬜ TODO | Not yet implemented |
| ⬜ N/A | Not applicable (platform-specific feature not being ported) |
