# Soldat PolyWorks — Reverse-Engineering Analysis & Migration Plan

> Analysed from the VB6 source tree (29,208 lines across 19 files).  
> No source files were modified.

---

## 1. Major Subsystems

| Subsystem | Primary location |
|---|---|
| File I/O (PMS read/write) | `frmOpenSoldatMapEditor.frm` — `LoadFile`, `SaveFile`, `SaveAndCompile` |
| Map data model | `modOpenSoldatMap.bas` (all type definitions); arrays live in the main form |
| Rendering | `modRender.bas` (DX8 init) + `frmOpenSoldatMapEditor.frm` `Render()` |
| Editing tools | `frmOpenSoldatMapEditor.frm` (tool dispatch in `picViewport_Mouse*`) |
| Selection | `frmOpenSoldatMapEditor.frm` — `RegionSelection`, `RegionSelPolys`, `VertexSel*` |
| Undo/redo | `frmOpenSoldatMapEditor.frm` — `SaveUndo`, `LoadUndo` |
| Geometry math | `frmOpenSoldatMapEditor.frm` — `PointInPoly`, `IsCW`, `SegXSeg`, `IsInSector`, `SnapVertexToGrid`, `NearCoord` |
| Sector table | `frmOpenSoldatMapEditor.frm` — `IsInSector`; computed in `SaveAndCompile` |
| Texture management | `modRender.bas` globals; loading in `LoadFile` / form load |
| Scenery | `frmScenery.frm` (panel); instance/texture arrays in main form |
| Waypoints | `frmWaypoints.frm` (panel); arrays `Waypoints`, `Connections` in main form |
| Lights (PW extension) | `frmOpenSoldatMapEditor.frm` — `ApplyLights`, `ApplyLightsToVert` |
| Config / settings | `modConfig.bas` — `LoadSettings`, `SaveSettings` |
| GIF→TGA conversion | `modOSME.bas` — `GifToBmp`, GDI+ pipeline |
| Prefab save/load | `frmOpenSoldatMapEditor.frm` — `SavePrefab`, `LoadPrefab` |
| Color management | `frmColor.frm`, `frmPalette.frm`, `modUtils.bas` |
| Properties panel | `frmInfo.frm` — live display of selected entity properties |
| Map options dialog | `frmMap.frm` — texture, bg colors, jet packs, grenades, etc. |
| Display toggles | `frmDisplay.frm` — per-layer visibility flags |
| Preferences dialog | `frmPreferences.frm` — undo depth, zoom limits, snap radii, theme |
| Tool palette | `frmTools.frm` — tool buttons and shortcut display |
| Sketch lines | `frmOpenSoldatMapEditor.frm` — sketch tool, `TSketchLine` arrays |

---

## 2. Data Structures and Global State

All map data arrays are declared as module-level fields on `frmOpenSoldatMapEditor` — there is no separate model class.

### Core map arrays

```
Polys()        TPolygon    — screen-space polygon vertices (X/Y in pixels at current zoom)
PolyCoords()   TPolygon    — world-space polygon vertices (X/Y in world units, saved to file)
vertexList()   TVertexList — per-vertex selection flags (0/1 × 3) + per-vertex RGB + polyType
mPolyCount     Integer     — number of polygons in map

Scenery()      TScenery    — placed scenery instances; [0]=cursor, [1..n]=placed
sceneryCount   Integer
SceneryTextures() TextureData — loaded DX8 textures; [0]=fallback "not found"
sceneryElements Integer    — count of unique scenery texture entries

Spawns()       TSpawnPoint
spawnPoints    Integer
Colliders()    TCollider
colliderCount  Integer
Waypoints()    TWaypoint
waypointCount  Integer
Connections()  TConnection — flat list of waypoint connection pairs
conCount       Integer

Lights()       TLightSource — PolyWorks-extension lights
lightCount     Integer
SketchLines()  TSketchLine  — PolyWorks-extension sketch lines
sketchCount    Integer
```

### `TCustomVertex`
```
X, Y, Z : Single     — position; Z repurposed as bounciness for poly type 18
rhw      : Single     — always 1 (pre-transformed, no projection)
color    : Long       — ARGB; DX8 diffuse color (light-modified at runtime)
tu, tv   : Single     — texture UV coordinates
```

### `TPolygon` (maps to `TMapFile_Polygon` in file)
```
vertex(1..3) : TCustomVertex
Perp         : TPolyHit   — 3 edge normals: X=sinθ, Y=cosθ, Z=bounciness
```

### `TVertexList` (editor-only, never saved)
```
vertex(1..3) : Integer  — selection state (0=unselected, 1=selected)
color(1..3)  : TColor   — base RGB before light blending (bytes R/G/B)
polyType     : Byte
```

### `TScenery` (in-memory, derived from `TProp` + loaded texture)
```
Translation : D3DVECTOR2   — world position
screenTr    : D3DVECTOR2   — screen position (recalculated on zoom/scroll)
rotation    : Single
Scaling     : D3DVECTOR2
Style       : Integer      — 1-based index into SceneryTextures / lstScenery
alpha       : Long
color       : Long
level       : Long         — 0=back, 1=middle, 2=front
selected    : Integer
Width/Height: Single       — from loaded texture dimensions
```

### Key view state
```
zoomFactor      Single      — current zoom (gMinZoom=0.0625 to 16, powers of 2)
scrollCoords(2) D3DVECTOR2  — world coordinate of viewport top-left corner
inc             Integer     — grid spacing in world units
snapRadius      Single
```

### Key selection state
```
numSelectedPolys       Integer
selectedPolys()        Integer     — indices of selected polygons
numSelectedScenery     Integer
numSelSpawns/Colliders/Waypoints/Lights  Integer
selRect(0..3)          D3DVECTOR2  — bounding box of selection in world coords
rCenter                D3DVECTOR2  — rotation/scale center
vertexList(i).vertex(j)            — per-vertex selection flag
```

### Map metadata (`TOptions` — saved to PMS header)
```
MapName     String (39 bytes, Pascal byte-array)
Texture     String (25 bytes)
Background1/2  Long  (ARGB)
StartJet, GrenadePacks, Medikits, Weather, Steps  Byte
MapRandomID    Long  — −1=PolyWorks native; >0=compiled; 0=new/blank
```

---

## 3. PMS / Map File Format

Sequential binary records, little-endian (VB6 native binary I/O).

### Header
| Field | Type | Notes |
|---|---|---|
| Version | Long | Always 11 |
| Map name | Byte[39] | Pascal length-prefixed string |
| Texture name | Byte[25] | Pascal length-prefixed string |
| Background color 1 | Long | ARGB top |
| Background color 2 | Long | ARGB bottom |
| StartJet | Byte | |
| GrenadePacks | Byte | |
| Medikits | Byte | |
| Weather | Byte | |
| Steps | Byte | |
| MapRandomID | Long | See below |

### Polygon section
```
Long       — polygon count
Per polygon:
  TCustomVertex × 3    (X,Y,Z,rhw,color,tu,tv — 32 bytes each)
  TPolyHit             (3 × TCustomVertex — normals)
  Byte                 — polygon type
```

### Sector table
```
Long       — sectorsDivision (world units per sector)
Long       — SECTOR_NUM (always 25)
51×51 cells, variable length:
  Integer  — polygon count in this cell
  Integer × count  — polygon indices
```

### Entity sections
```
Long       — scenery prop count
Per prop:  TProp (Bool active, Int style, Long width/height,
                  Single x/y/rotation/scaleX/scaleY, Long alpha/color/level)

Long       — scenery element (texture name) count
Per element: Byte[51] name (Pascal string), Long date

Long       — collider count
Per collider: Long active, Single x, Single y, Single radius

Long       — spawn count
Per spawn:  Long active, Long x, Long y, Long team

Long       — waypoint count
Per waypoint: TNewWaypoint (active/id/x/y Long, movement Byte,
              pathNum/special Byte, 5 pad bytes,
              connectionsNum Long, Connections(1..20) Long)
```

### PolyWorks extension (only when `MapRandomID = −1`)
```
Long       — light count
Per light: TLightSource (Long x/y, Long color, Single radius, Integer selected)

Integer    — sketch line count
Per sketch: TSketchLine
```

### Compiled game format (when `MapRandomID > 0`)
- Terminates with 4 × `Integer` zero after waypoints
- No extension data written
- All coordinates are **centered** (see §3.1 below)

### 3.1 SaveFile vs SaveAndCompile

| | `SaveFile` (.pwn) | `SaveAndCompile` (game PMS) |
|---|---|---|
| MapRandomID | −1 | Random 10000–1009999 |
| Coordinates | Raw world coords | Offset by map centroid |
| Sector table | All zeros | Fully computed |
| Extension data | Written | Not written |
| Trailing zeros | None | 4 × Integer zero |

**Coordinate centering** (`SaveAndCompile` only):  
Computes `midX = Midpoint(maxX, minX)`, `midY = Midpoint(maxY, minY)` over all polygon vertices, then subtracts these from every X/Y coordinate before writing. The game requires centered maps.

---

## 4. Polygon Representation and Geometry

### Normal vectors (`Perp` field)
For the edge from vertex `j` to vertex `(j mod 3)+1`:
```
xDiff = nextVert.X − currVert.X
yDiff = currVert.Y − nextVert.Y   ← Y axis is flipped
length = sqrt(xDiff² + yDiff²)
Perp.vertex(j).X = yDiff / length   (sin θ)
Perp.vertex(j).Y = xDiff / length   (cos θ)
Perp.vertex(j).Z = 1.0              (bounciness; > 1 for type 18)
```

### Winding order
All polygons must be **clockwise** (screen Y-down convention). Enforced on creation and on vertex snap. `IsCW(i)` works by computing the centroid via `Midpoint` of all three vertices and testing `PointInPoly` — if the centroid isn't inside, the polygon is CCW. If CCW, vertices 2 and 3 are swapped.

### `PointInPoly` algorithm
Half-space test using each edge's outward normal:
```
For each edge j → j+1:
  xDist = point.X − vertex[j].X
  yDist = point.Y − vertex[j].Y
  xDiff = vertex[j+1].X − vertex[j].X
  yDiff = vertex[j].Y   − vertex[j+1].Y   ← Y flipped
  length = sqrt(xDiff² + yDiff²)
  D = (yDiff/length)*xDist + (xDiff/length)*yDist
  if D < 0: return false
return true
```
Works only for CW polygons (normals point inward in Y-down coords).

### `SnapVertexToGrid`
```
offset := (inc * zoom) − ((scrollX mod inc) * zoom)
target := floor(coord / (inc * zoom)) * (inc * zoom) + offset
if coord − target >= (inc * zoom / 2): target += inc * zoom
return target
```

### Intersection tests
- `SegXSeg(p1,p2,p3,p4)` — parametric t/u ∈ [0,1]
- `SegXHorizSeg` / `SegXVertSeg` — optimised for axis-aligned sector boundaries
- `SegmentsIntersect` — used for waypoint connection path crossing prevention

### Sector table
- 51×51 grid, cells −25..+25 on each axis
- `sectorsDivision = ceil(max(mapWidth, mapHeight) / 25)`
- `IsInSector(X, Y, polyIndex)`: bounding-box reject → vertex-in-sector → sector-corner-in-poly → edge-intersection tests
- Type 3 (DoesntCollide) polygons excluded from sector table

---

## 5. Scenery Representation and Manipulation

- **File format** (`TProp`): active, style, width/height, x/y, rotation, scaleX/scaleY, alpha, color, level
- **In-memory** (`TScenery`): adds `screenTr` (screen position), `selected`, texture dimensions
- `Scenery(0)` is the "cursor" — the in-progress scenery being placed; never saved
- **Levels**: 0=back (behind polygons), 1=middle, 2=front (in front of all)
- **Placement flow** (TOOL_SCENERY):
  - Click 1: set position (`numCorners = 1`)
  - Click 2: set rotation (only if `frmScenery.rotateScenery` is true; else auto-advance)
  - Click 3: set scale (only if `frmScenery.scaleScenery` is true; else auto-advance)
  - At `numCorners = 3`: commit to `Scenery(sceneryCount)`, call `SaveUndo`
- **Alpha quirk**: alpha=0 in file is remapped to 255 (fully opaque) on load
- **GIF support**: GIFs converted to TGA via GDI+ before DX8 load; temp file `appPath\Temp\gif.tga`
- **ClearUnused**: removes texture entries that have no active instances
- Scenery indices are **1-based** (`Style` 1..n maps to `SceneryTextures(1..n)`; `lstScenery` is 0-based)

---

## 6. Textures and Texture Management

### Map texture
- Stored in `MapOptions.Texture` (up to 25 chars, filename only, no path)
- Loaded from `soldatPath\Textures\`
- Single DX8 texture used for all polygon UV rendering
- `fixedTexture` mode: `tu = worldX / textureWidth`, `tv = worldY / textureHeight`

### Scenery textures
- Loaded from `soldatPath\Images-gfx\` or `soldatPath\Scenery-gfx\`
- `SceneryTextures(0)` = fallback "not found" (loaded from `appPath\data\missing.bmp`)
- Indices 1..`sceneryElements` correspond to `lstScenery` list entries
- `TextureData`: DX8 texture pointer, Width, Height, filename

### Color encoding
`ARGB(alpha, RGB(blue, green, red))` — note VB6's `RGB()` takes R,G,B but the DirectX color word is stored as 0xAARRGGBB (ARGB). The `GetRGB()` function parses from hex string. When saving vertex colors, `GetAlpha` of the current DX color is preserved but R/G/B come from `vertexList(i).color(j)` — the base color before light modification.

---

## 7. Spawn Points, Waypoints, and Map Entities

### Spawn points
- `active`: Long (0/1); `X`, `Y`: Long world position; `team`: Long
- Team values: 0=Any, 1=Alpha, 2=Bravo, 3=Charlie, 4=Delta, 5=Alpha Flag, 6=Bravo Flag, 7=Grenade, 8=Medkit, 9=Cluster, 10=Vest, 11=Flame, 12=Berserk, 13=Predator, 14=PointMatch, 15=RamboBow, 16=StatGun
- Team clamped to ≤31 on load

### Waypoints
- **File format** (`TNewWaypoint`): active/id/X/Y Long, movement Byte (flags), pathNum/special Byte, 5 pad bytes, connectionsNum Long, Connections(1..20) Long — per-waypoint adjacency list
- **Memory format** (`TWaypoint`): adds `selected`, `numConnections`, `tempIndex` (used during deletion remapping)
- **In-memory connections**: flat global `Connections()` array of `TConnection(point1, point2)` pairs
- On save: flat list distributed back into per-waypoint adjacency lists
- On load: per-waypoint lists expanded into flat array; `numConnections` recomputed
- `waypointPath` in frmWaypoints: which path number is shown (0=all paths)

### Colliders
- `active`: Long; `X`, `Y`: Single; `radius`: Single

### Lights (PolyWorks extension)
- `X`, `Y`: Long; `color`: Long ARGB; `radius`: Single; `selected`: Integer
- `ApplyLights`: iterates all polys × vertices × lights, dot-products light direction with polygon normal, blends into vertex color

---

## 8. Selection and Editing Behavior

### Selection modes
- **TOOL_VSELECT** (1): replace selection
- **TOOL_VSELADD** (14): add to selection
- **TOOL_VSELSUB** (15): subtract from selection

### Simultaneous selection
Polygons (by vertex), scenery, spawn points, colliders, waypoints, and lights can all be simultaneously selected. `selRect` tracks the bounding box in world coordinates; `rCenter` is the rotation/scale pivot.

### Per-vertex selection (`vertexList(i).vertex(j)`)
- 0 = unselected, 1 = selected
- Sum = 3: polygon fully selected (delete removes poly entirely)
- Sum = 1 or 2: polygon partially selected (vertices dragged; poly survives delete)

### Deletion (`DeletePolys`)
In-place compaction: iterates source array writing to `offset`, only incrementing `offset` for non-deleted items. `ReDim Preserve` shrinks arrays afterward. Waypoint deletion uses `tempIndex` to remap connection references before compaction.

### Region selection (`InSelRect`)
`selectedCoords(1)` and `selectedCoords(2)` define the rubber-band rectangle in screen coordinates. Test is exclusive (`>` and `<`, not `>=`/`<=`).

### Vertex snapping
1. **Grid snap** (`snapToGrid && showGrid`): snap to nearest grid intersection via `SnapVertexToGrid`
2. **Vertex snap** (`ohSnap`): find closest vertex in all polys within `snapRadius`; only works when exactly one vertex of one polygon is selected

---

## 9. Undo / Redo

### Mechanism
File-based ring buffer. Files: `appPath\undo\undo0.pwn` … `undo{maxUndo}.pwn`. Default depth: 16 (configurable in Preferences).

### State variables
```
currentUndo      Integer   — current file index (wraps 0..maxUndo)
numUndo          Integer   — undos available
numRedo          Integer   — redos available
maxUndo          Integer   — ring buffer depth
selectionChanged Boolean   — dirty flag: save undo on next edit
```

### `SaveUndo`
1. Increment `currentUndo` mod `maxUndo+1`
2. Reset `numRedo = 0`
3. Increment `numUndo` (capped at `maxUndo`)
4. Write current map state to `undo{currentUndo}.pwn` (same binary format as `SaveFile`)

### `LoadUndo(redo: Boolean)`
- **Undo**: decrement `currentUndo` (wrapping), decrement `numUndo`, increment `numRedo`
- **Redo**: increment `currentUndo`, decrement `numRedo`
- If `selectionChanged` is true, saves current state first
- Reads binary undo file into all map arrays

### Included in undo state
All polygon, scenery, collider, spawn, light, waypoint, and connection data; selection state (`selectedCoords`, `selectedPolys`, `vertexList`); `selRect`, `rCenter`.

### NOT included
- Texture list (scenery element names)
- Map options (name, texture, colors, etc.)
- Sketch lines — **notable bug**: sketch IS written in `SaveFile` format (used for undo files) but `LoadUndo` does not read the sketch data section
- Zoom/scroll position

---

## 10. Rendering and Coordinate Systems

### The critical dual-array invariant
```
PolyCoords(i).vertex(j).X/Y  ← world space (persisted in file)
Polys(i).vertex(j).X/Y       ← screen space = (worldX − scroll.X) × zoom
```
These must always be kept in sync. Every vertex-modifying operation must update both arrays atomically.

### Zoom levels
`gMinZoom = 0.0625` (1/16×). Always a power of 2: 0.0625, 0.125, 0.25, 0.5, 1.0, 2.0, 4.0, 8.0, 16.0. `GetZoomDir` snaps zoom changes to the nearest power-of-2 boundary; zoom is centred on the mouse cursor position.

### DirectX 8 setup
- Pre-transformed vertex format: `D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1`
- `rhw = 1.0` always; no view/projection matrix needed for polygon vertices
- Sprites (scenery) use `D3DXSprite` with world matrix via `D3DXMatrixLookAtLH` / `D3DXMatrixPerspectiveLH`
- Render target: `PictureBox picViewport` (HWND passed to DX8)
- Transparency colour key: `COLOR_KEY = &HFF00FF00` (bright green)

### Render order (one frame)
1. Clear to black
2. Background gradient quad (4 vertices, triangle strip, colour 1 top → colour 2 bottom)
3. Type-24 (Background) and type-25 (BackgroundTransition) polygons
4. Scenery `level = 0` (back)
5. Normal polygons — all other types, textured
6. Scenery `level = 1` (middle)
7. Scenery `level = 2` (front)
8. Wireframe overlays (`showWireframe`)
9. Vertex dots (`showPoints`)
10. Grid lines (`showGrid`)
11. Objects: spawn icons, collider circles (`showObjects`)
12. Waypoints and connections (`showWaypoints`)
13. Sketch lines (`showSketch`)
14. Light circles (`showLights`)

---

## 11. Mouse and Keyboard Interaction

### Mouse events on `picViewport`
- `MouseDown`: tool dispatch based on `currentFunction`; left button initiates, right button context-menu or alternative
- `MouseMove`: drag operations (move, stretch, rotate, rubber-band select, background pan)
- `MouseUp`: commit operation, call `SaveUndo`
- Mouse wheel via `MBMouse.ocx` → `MouseWheel` event → zoom centred on cursor
- Middle-button drag (`Button = 4`): pan the viewport by updating `scrollCoords(2)`

### Keyboard
DirectInput 8 keyboard buffer (`BUFFER_SIZE = 10`), polled by a VB6 timer. Key bindings (some configurable in frmTools/frmPreferences):

| Key | Action |
|---|---|
| Delete | `DeletePolys` |
| Arrow keys | Nudge selected by `QUICK_MOVE_DELTA = 90000` world units |
| Escape | Deselect / cancel operation |
| Ctrl+Z / Ctrl+Y | Undo / Redo |
| Ctrl+C / V / X | Copy / Paste / Cut (prefab-based) |
| Space | Alternate tool momentarily |

### `currentFunction` vs `currentTool`
`currentTool` is the base tool from the toolbar. `currentFunction` may be a modifier variant (e.g. holding Shift switches to add-to-selection). Tool constants:
```
TOOL_MOVE=0, TOOL_VSELECT=1, TOOL_POLY=2, TOOL_SCENERY=3,
TOOL_SPAWNS=4, TOOL_WAYPOINTS=5, TOOL_CONNECTION=6, TOOL_TEXTURE=7,
TOOL_DEPTH=8, TOOL_COLOR=9, TOOL_SKETCH=10, TOOL_COLLIDER=11,
TOOL_LIGHT=12, TOOL_DEPTHMAP=13,
TOOL_VSELADD=14, TOOL_VSELSUB=15, TOOL_ROTATE=16, TOOL_SCALE=17,
TOOL_STRETCHX=18, TOOL_STRETCHY=19, ...
```

---

## 12. Menus, Toolbars, Dialogs, and Major UI Workflows

### Custom window chrome
Entire window is borderless (VB6 form with no caption). Custom title bar (`picTitle`), custom min/max/close buttons (PictureBoxes), custom resize grip (`picResize`, bottom-right). Window dragging implemented manually via `MouseDown`/`MouseMove` on `picTitle`.

### Menu system
- VB6 menus (`mnuFile`, `mnuEdit`, etc.) are `Visible = False` — they exist only to be popped via `PopupMenu`
- Custom picture-box menu bar (`picMenu()` array) renders button graphics; clicks call `PopupMenu`
- All menu logic is in standard VB6 `mnuXxx_Click` handlers

### Floating panels (modeless)
| Panel | Purpose |
|---|---|
| `frmScenery` | Scenery texture list (TreeView), level selector (back/mid/front), rotate/scale flags |
| `frmWaypoints` | Waypoint type flags (movement bits), path number, special action, path filter |
| `frmDisplay` | Show/hide toggles: polys, wireframe, points, grid, objects, waypoints, lights, sketch |
| `frmTools` | Tool palette buttons + shortcut key display |
| `frmInfo` | Live properties of selected entity |
| `frmPalette` | Colour palette grid (saved to `appPath\palettes\current.txt`) |
| `frmTaskBar` | Minimal floating shortcut toolbar |

### Modal dialogs
| Dialog | Purpose |
|---|---|
| `frmMap` | Map options: texture, bg colours, jet packs, grenade packs, medikits, weather, steps |
| `frmColor` | HSV/RGB colour picker |
| `frmTexture` | UV offset adjustment for selected vertices |
| `frmPreferences` | Undo depth, snap radius, zoom limits, grid size, auto-save, theme, key bindings |

### Major workflows

**Create polygon**: TOOL_POLY → 3 left-clicks (with vertex snapping) → CW enforcement on third click → `SaveUndo`.

**Edit polygon vertices**: TOOL_VSELECT → rubber-band or click select → drag updates both `Polys` and `PolyCoords`.

**Place scenery**: TOOL_SCENERY → select texture in frmScenery → 1–3 clicks in viewport (position, optional rotation, optional scale) → commit → `SaveUndo`.

**Compile and launch**: File → Save and Compile → centres map, recomputes normals and sector table, writes game PMS → optionally launches OpenSoldat via `ShellExecute`.

**Undo/Redo**: Ctrl+Z / Ctrl+Y (or Edit menu) → `LoadUndo(False)` / `LoadUndo(True)`.

---

## 13. External Dependencies

| Component | What it does | Notes |
|---|---|---|
| `dx8vb.dll` | DirectX 8 COM wrapper for VB6 | Rendering, textures, keyboard input |
| `MBMouse.ocx` | Mouse wheel events in VB6 | Not available natively in VB6 |
| `COMDLG32.OCX` | CommonDialog (file open/save) | `dlgFile.ShowOpen`, `ShowSave` |
| `mscomctl.ocx` | TreeView, ImageList | Scenery tree, cursor icon storage |
| `gdiplus.dll` | GDI+ image processing | GIF → PNG/BMP conversion pipeline |
| `olepro32.dll` | `OleCreatePictureIndirect` | Convert GDI+ bitmap to VB IPicture |
| `kernel32.dll` | INI file R/W, file ops | `GetPrivateProfileString`, `WritePrivateProfileSection` |
| `user32.dll` | Window management | `SetWindowPos`, message hooks |
| `gdi32.dll` | GDI drawing | Region hit testing |
| `shell32.dll` | Folder browser, launch game | `SHBrowseForFolder`, `ShellExecute` |
| `advapi32.dll` | Registry access | Soldat installation path detection |

All OCX and DX8 dependencies are Windows-only and require COM registration.

---

## 14. Tight Coupling to VB6 UI

The following are deeply entangled with the VB6 runtime and cannot be cleanly separated as-is:

- **All map data arrays** live as `Private` fields on `frmOpenSoldatMapEditor` — no separate model class
- `LoadFile`/`SaveFile`/`SaveAndCompile` reference UI controls directly (`lblFileName.Caption`, `picProgress.Width`)
- `Render()` writes to `picViewport` via a DX8 handle targeting that control's HWND
- `ApplyLights` modifies `Polys()` and calls `Render()`
- `DeletePolys` calls `GetInfo` and `Render` and updates `frmInfo.lblCount`
- `ConfirmExists` queries `tvwScenery.Nodes` (a live TreeView control)
- `CreateSceneryTexture` appends to `frmScenery.lstScenery`
- All error handling uses `MsgBox`
- Settings are `Public` fields on satellite forms (e.g. `frmDisplay.showPolys`, `frmWaypoints.showPaths`)
- Undo files are written relative to `appPath` via VB6 `Open…For Binary`

---

## 15. What Can Be Cleanly Separated (Headless Core)

| Component | Separability |
|---|---|
| All `Type` definitions (`modOpenSoldatMap.bas`) | **Trivially portable** — pure data |
| `LoadFile` / `SaveFile` / `SaveAndCompile` | **Mostly separable** — strip UI progress bar and error MsgBoxes |
| `PointInPoly`, `IsCW`, `SegXSeg`, `SnapVertexToGrid`, normal computation | **Trivially portable** — pure math |
| Sector table computation | **Separable** — pure algorithm |
| `ApplyLights` | **Separable** after removing UI coupling |
| `DeletePolys` logic | **Separable** — pure array manipulation |
| Undo/redo ring buffer logic | **Separable** — file I/O + data copy |
| `modUtils.bas` (`ARGB`, `GetRGB`, `Midpoint`, `GetAngle`, `IsBetween`, `Clamp`) | **Trivially portable** |
| `modConfig.bas` settings load/save | **Separable** — replace Win32 INI API |
| `SavePrefab` / `LoadPrefab` | **Separable** |

---

## 16. Undocumented and Surprising Behavior to Preserve

1. **`MapRandomID = −1`** is the PolyWorks native-format sentinel. Any file with this value has extension data (lights, sketch) after the waypoints. Positive non-zero = compiled game map. Zero = new unsaved map.

2. **Sector table is all zeros in `SaveFile`** — the game regenerates the sector table on load. PolyWorks only writes a valid sector table in `SaveAndCompile`.

3. **Alpha = 0 → 255 for scenery props** — zero alpha in the file means fully opaque; zero is remapped on load. Do not write zero to mean transparent.

4. **Coordinate centering on compile** — all world coordinates are shifted by `−midX`, `−midY` before writing in `SaveAndCompile`. The game requires centered maps; PolyWorks's own `.pwn` format skips this.

5. **Winding-order enforcement** — all polygons must be clockwise. The sector table, normal computation, and `PointInPoly` all depend on this. Enforced at creation time and on vertex snap.

6. **Bounciness in vertex Z** — for polygon type 18 (Bouncy), `Perp.vertex(j).Z` stores a bounciness multiplier (> 1.0). For all other polygon types Z must be 1.0 when saving. Do not zero out Z unconditionally.

7. **`vertexList(i).color(j)` is the base color** — `Polys(i).vertex(j).color` is the light-modified DX color. Saved vertex colors come from `vertexList` (the base color) with only the alpha channel taken from the DX color.

8. **`QUICK_MOVE_DELTA = 90000`** — arrow-key nudge amount, in world-space units.

9. **GIF conversion via temp file** — all GIFs are converted to `appPath\Temp\gif.tga` before loading into DX8. The same temp file is overwritten on each GIF load; there is no caching.

10. **`IsCW` uses centroid-in-poly, not cross-product sign** — `IsCW(i)` computes the centroid as nested `Midpoint` calls and passes it to `PointInPoly`. This is unusual but correct. The reimplementation must use the same algorithm.

11. **Waypoint connections stored twice** — in the file, connections are per-waypoint adjacency lists inside `TNewWaypoint`. In memory they are a flat `TConnection` array. `numConnections` per waypoint is a derived count, recomputed during load from the flat list; it is not authoritative.

12. **Scenery indices are 1-based** — `Scenery(0)` is the cursor; `Scenery(i).Style` is 1-based pointing into `SceneryTextures(1..n)`; `lstScenery` is 0-based. These offsets must be preserved exactly.

---

# Proposed Architecture: Free Pascal / Lazarus Reimplementation

> **Historical.**  This section is the original 2025 design proposal and is not
> what was built.  The shipping implementation is C++17 with Dear ImGui, GLFW
> and OpenGL; see `docs/cpp-architecture.md`.  It is kept because the layering
> argument and the VB6 behavioural notes in it still apply, and because the
> reverse-engineering above it is what both designs were derived from.

## Design Principles

1. **Strict layer separation**: the core map model compiles and tests with zero Lazarus/LCL units
2. **Single source of truth**: all mutable map state in one `TMapDocument` object
3. **OpenGL for the viewport only**: all other UI uses native LCL controls
4. **In-memory undo/redo**: eliminate filesystem dependency; use deep-copy snapshots
5. **No global mutable state** outside `TMapDocument`
6. **Exact behavioral fidelity**: preserve every quirk listed in §16

---

## Package / Unit Layout

```
polyworks/
├── core/                         ← headless; no LCL, no OpenGL
│   ├── pw.types.pas              — all packed record types (replaces modOpenSoldatMap.bas)
│   ├── pw.pms.pas                — PMS binary file parser and serializer
│   ├── pw.geometry.pas           — PointInPoly, IsCW, SegXSeg, sector table, normals
│   ├── pw.map.pas                — TMapDocument: all map state + editing operations
│   ├── pw.undo.pas               — TUndoStack: in-memory snapshot ring buffer
│   ├── pw.lights.pas             — ApplyLights algorithm (pure math, no render calls)
│   └── pw.utils.pas              — ARGB, Midpoint, GetAngle, clamp, etc.
│
├── gui/                          ← Lazarus/LCL layer
│   ├── main/
│   │   ├── frmmain.pas           — TMainForm (TForm subclass)
│   │   ├── viewport.pas          — TMapViewport (TOpenGLControl subclass)
│   │   └── renderer.pas          — TRenderer: OpenGL 2.1 render pipeline
│   ├── tools/
│   │   └── tools.pas             — ITool interface + all tool implementations
│   ├── panels/
│   │   ├── frmscenery.pas
│   │   ├── frmwaypoints.pas
│   │   ├── frmdisplay.pas
│   │   └── frmtools.pas
│   ├── dialogs/
│   │   ├── frmmap.pas
│   │   ├── frmpreferences.pas
│   │   ├── frmcolor.pas
│   │   └── frmtexture.pas
│   └── pw.config.pas             — INI read/write (FPC IniFiles unit)
│
└── tests/                        ← FPCUnit; headless
    ├── test_pms.pas              — round-trip load/save/compile tests
    ├── test_geometry.pas         — PointInPoly, IsCW, sector table, snapping
    ├── test_undo.pas             — undo/redo correctness and depth limit
    └── test_lights.pas           — light blending math
```

---

## Core Layer Detail

### `pw.types.pas`
Direct translation of all `Type` declarations from `modOpenSoldatMap.bas`. Use `packed record` throughout to guarantee binary layout matches the original VB6 structs. Export named constants for all polygon types, team IDs, and tool IDs.

```pascal
type
  TVertex = packed record
    X, Y, Z : Single;    { Z = bounciness for poly type 18, else 1.0 }
    rhw      : Single;   { always 1.0 }
    Color    : LongWord; { ARGB 0xAARRGGBB }
    tu, tv   : Single;
  end;

  TPolyNormal = packed record
    X, Y, Z : Single;    { sin θ, cos θ, bounciness }
    rhw      : Single;
    Color    : LongWord;
    tu, tv   : Single;
  end;

  TMapPoly = packed record
    V    : array[1..3] of TVertex;
    Perp : array[1..3] of TPolyNormal;
    Kind : Byte;
  end;

  { ... TProp, TMapFile_Scenery, TCollider, TSaveSpawnPoint,
         TNewWaypoint, TConnection, TLightSource, TSketchLine,
         TMapOptions — all translated verbatim }
```

### `pw.pms.pas`
```pascal
{ Returns true on success; on failure sets ErrMsg }
function LoadPMS(const Filename: string; out Doc: TMapData;
                 out ErrMsg: string): Boolean;

{ Saves in PolyWorks native format (MapRandomID = -1) }
function SavePMS(const Filename: string; const Doc: TMapData;
                 out ErrMsg: string): Boolean;

{ Saves game-compatible format: centres coords, builds sector table }
function CompilePMS(const Filename: string; const Doc: TMapData;
                    out ErrMsg: string): Boolean;
```
No UI calls. Caller handles progress reporting and error display.

### `pw.geometry.pas`
All geometry functions are standalone, parameterised on data (not global arrays):

```pascal
function  PointInPoly(X, Y: Single; const V: array of TVertex): Boolean;
function  IsPolyClockwise(const V: array of TVertex): Boolean;
procedure EnforceClockwise(var V: array of TVertex);
procedure ComputeNormals(var Poly: TMapPoly);
function  SegmentsIntersect(x1,y1,x2,y2,x3,y3,x4,y4: Single): Boolean;
procedure BuildSectorTable(const Polys: array of TMapPoly;
                           MapW, MapH: Single; out Table: TSectorTable);
function  SnapToGrid(Coord, GridSize, ScrollOffset, Zoom: Single): Single;
function  NearCoord(MouseCoord, PolyCoord, Range: Single): Boolean;
```

### `pw.map.pas`
`TMapDocument` owns all mutable state and exposes editing operations:

```pascal
TMapDocument = class
public
  { Map data }
  Polys       : TMapPolyArray;
  Scenery     : TSceneryArray;
  Spawns      : TSpawnArray;
  Colliders   : TColliderArray;
  Waypoints   : TWaypointArray;
  Connections : TConnectionArray;
  Lights      : TLightArray;
  Sketch      : TSketchArray;
  Options     : TMapOptions;

  { Editor-only state (not persisted) }
  VertexColors : TVertexColorArray;  { base colors before light blending }
  ScreenPolys  : TMapPolyArray;      { screen-space mirror of Polys }
  Selection    : TSelectionState;
  Zoom         : Single;
  ScrollX, ScrollY : Single;

  { Coordinate conversion }
  procedure WorldToScreen(wx, wy: Single; out sx, sy: Single);
  procedure ScreenToWorld(sx, sy: Single; out wx, wy: Single);
  procedure RebuildScreenCache;       { recalculate ScreenPolys from Polys + Zoom/Scroll }

  { Editing operations — all update both world and screen arrays atomically }
  procedure MoveSelected(dWorldX, dWorldY: Single);
  procedure DeleteSelected;
  procedure AddPoly(const Verts: array of TVertex; Kind: Byte);
  procedure AddScenery(const S: TScenery);
  procedure AddSpawn(X, Y: Integer; Team: Byte);
  procedure SetZoom(NewZoom: Single; CentreScreenX, CentreScreenY: Single);
  procedure SelectRegion(x1, y1, x2, y2: Single; Mode: TSelMode);
  procedure SelectVertex(PolyIdx, VertIdx: Integer; Mode: TSelMode);
  procedure ApplyLights;
  procedure ClearSelection;
end;
```

### `pw.undo.pas`
In-memory snapshot ring buffer; no filesystem dependency:

```pascal
TMapSnapshot = record
  Polys, ScreenPolys : TMapPolyArray;
  VertexColors       : TVertexColorArray;
  Scenery            : TSceneryArray;
  Spawns             : TSpawnArray;
  Colliders          : TColliderArray;
  Waypoints          : TWaypointArray;
  Connections        : TConnectionArray;
  Lights             : TLightArray;
  Selection          : TSelectionState;
end;

TUndoStack = class
  constructor Create(MaxDepth: Integer);
  procedure   Push(const Doc: TMapDocument);
  function    Undo(Doc: TMapDocument): Boolean;   { returns false if nothing to undo }
  function    Redo(Doc: TMapDocument): Boolean;
  property    CanUndo: Boolean;
  property    CanRedo: Boolean;
  procedure   Clear;
end;
```

### `pw.lights.pas`
```pascal
{ Modifies ScreenPolys vertex colors in-place based on Lights array.
  BaseColors provides the unlit per-vertex RGB (from VertexColors).
  Does not call Render; does not access any UI. }
procedure ApplyLights(var ScreenPolys: TMapPolyArray;
                      const Polys: TMapPolyArray;
                      const BaseColors: TVertexColorArray;
                      const Lights: TLightArray);

{ Restore base colors when lights are disabled }
procedure RestoreBaseColors(var ScreenPolys: TMapPolyArray;
                            const BaseColors: TVertexColorArray);
```

---

## GUI Layer Detail

### `viewport.pas` — `TMapViewport`
Subclasses `TOpenGLControl` (from Lazarus `lazarusopenglpackage`). Responsibilities:
- Owns an `TRenderer` instance
- Receives all mouse/keyboard events; converts screen→world via `Document.ScreenToWorld`
- Dispatches tool events to `TToolController`
- Calls `Document.SetZoom` on mouse-wheel
- On `Paint`: calls `Renderer.Render(Document, ViewSettings)`

### `renderer.pas` — `TRenderer`
OpenGL 2.1 render pipeline (VAO/VBO or immediate mode). Render order identical to §10. Textures loaded via `stb_image` (FPC binding) or FreeImage — no DX8, no GDI+.

```pascal
TRenderer = class
  procedure LoadMapTexture(const Filename: string);
  procedure LoadSceneryTexture(Index: Integer; const Filename: string);
  procedure Render(const Doc: TMapDocument; const VS: TViewSettings);
end;
```

### `tools/tools.pas` — Tool system
```pascal
ITool = interface
  procedure MouseDown(Doc: TMapDocument; X, Y: Single; Button: TMouseButton; Shift: TShiftState);
  procedure MouseMove(Doc: TMapDocument; X, Y: Single; Shift: TShiftState);
  procedure MouseUp  (Doc: TMapDocument; X, Y: Single; Button: TMouseButton);
  procedure KeyDown  (Doc: TMapDocument; Key: Word; Shift: TShiftState);
  procedure Activate (Doc: TMapDocument);
  procedure Deactivate(Doc: TMapDocument);
end;
```
One class per tool: `TMoveTool`, `TVertexSelectTool`, `TPolyTool`, `TSceneryTool`, `TSpawnTool`, `TWaypointTool`, `TConnectionTool`, `TTextureTool`, `TColorTool`, `TSketchTool`, `TColliderTool`, `TLightTool`. `TToolController` owns the active tool and pushes undo snapshots after operations complete.

---

## Image Loading (Replacing DX8 + GDI+)

- Use **stb_image** (FPC binding available) for PNG, BMP, TGA, JPEG, and **GIF** — eliminates the GDI+ intermediate TGA conversion entirely
- Alternatively, **FreeImage** (also has FPC bindings) if broader format support is needed
- Textures uploaded as `GLuint` via `glTexImage2D`; wrap mode `GL_REPEAT` for map texture, `GL_CLAMP_TO_EDGE` for scenery

---

## Configuration (Replacing Win32 INI API)

Free Pascal's `IniFiles` unit provides `TIniFile` — cross-platform, identical semantics to `GetPrivateProfileString`/`WritePrivateProfileSection`. Soldat installation path: on Windows, look up via `TRegistry` (same as original); on Linux/macOS, look in a config file or environment variable.

---

## Testing Strategy (FPCUnit)

### `test_pms.pas`
- Load each `.pms` fixture file → `SavePMS` → reload → compare all fields byte-for-byte
- Compile round-trip: load → `CompilePMS` → verify `MapRandomID > 0`, coordinates centered, sector table non-empty and consistent
- Edge cases: empty map, maximum polygon count (65535), all 26 polygon types, maps with zero waypoints

### `test_geometry.pas`
- `PointInPoly` with known inside / outside / on-edge cases for CW and CCW triangles
- `IsPolyClockwise` for CW, CCW, and degenerate (zero-area) triangles
- `SnapToGrid` at various zoom levels and scroll offsets, including negative coordinates
- `SegmentsIntersect` for parallel, crossing, co-linear, and endpoint-touching segments
- `BuildSectorTable` for maps with polygons in all 51×51 sectors; verify type-3 exclusion

### `test_undo.pas`
- N undos after N operations returns to the initial state exactly
- Redo after undo restores; new operation after undo clears the redo stack
- Undo at depth limit stops at the oldest available state (no crash, no wrap-around error)
- `selectionChanged` flag triggers an implicit snapshot before the first structural edit

### `test_lights.pas`
- `ApplyLights` with a single light and known polygon geometry produces expected vertex colors
- Zero lights: `RestoreBaseColors` restores exactly the base color values
- Light radius correctly attenuates influence at known distances

---

## Migration Sequence

| Step | Work | Validation |
|---|---|---|
| 1 | Port `pw.types.pas` | Assert record sizes match VB6 binary layout using `SizeOf` checks |
| 2 | Port `pw.pms.pas` | Round-trip every known `.pms` file byte-for-byte |
| 3 | Port `pw.geometry.pas` | Exhaustive FPCUnit geometry tests |
| 4 | Port `pw.map.pas` | Unit tests for each editing operation |
| 5 | Port `pw.undo.pas` | Undo/redo unit tests |
| 6 | Port `pw.lights.pas` | Light blending unit tests |
| 7 | Build minimal GUI shell | Load a file; render polygons read-only with OpenGL |
| 8 | Port rendering | Visual comparison against reference screenshots from VB6 version |
| 9 | Port tools one by one | Each tool: manual smoke test + integration test |
| 10 | Port all dialogs and panels | Manual QA |
| 11 | System test | Load reference map, perform known edit sequence, compile, compare output byte-for-byte against VB6 reference output |

---

## Key Risks and Mitigations

| Risk | Mitigation |
|---|---|
| VB6 `Single` IEEE 754 rounding differs from FPC | Use `Single` throughout (both use 32-bit IEEE 754); test coordinate round-trips with known values |
| Binary struct padding differs | `packed record` everywhere; verify with `SizeOf` assertions |
| Pascal string length encoding | VB6 uses length-prefixed byte arrays (not null-terminated); replicate the exact encoding |
| `IsCW` centroid algorithm | Port verbatim using `PointInPoly`; do not replace with cross-product sign |
| Color channel order | VB6 ARGB is 0xAARRGGBB; OpenGL typically uses RGBA — convert at texture upload only |
| Sector table precision | Port `IsInSector` verbatim, including the `div` corner-point test |
| GIF frame handling | stb_image returns only the first frame — matches VB6 behaviour |
| Bounciness in vertex Z | Poly type 18 must preserve Z; write Z=1.0 for all other types |
| Waypoint connection bidirectionality | Flat `Connections` list stores directed pairs; rebuild per-waypoint lists exactly as in `SaveAndCompile` |
| `selectionChanged` undo timing | Port the dirty-flag logic exactly: implicit snapshot before first structural edit after a selection change |
| Sketch-lines undo bug | Decide whether to fix (preserve sketch in undo) or faithfully replicate the bug — recommend fixing |
