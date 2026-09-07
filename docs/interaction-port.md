# PolyWorks Interaction Port — Audit Checklist

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
