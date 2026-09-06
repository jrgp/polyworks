# PolyWorks Rewrite — Implementation Plan

## Guiding constraints

- The GUI must never own application state.  
- Every milestone before M9 is headless and independently testable.  
- The dependency order is: types → file format → geometry → map model → undo → renderer → GUI.  
- Prefer simple packed records and direct translations over speculative abstractions.  
- Exact behavioral fidelity with the VB6 version for all file-format and geometry decisions.

---

## Source directory layout (target)

```
src-fp/
  core/
    pw.types.pas
    pw.utils.pas
    pw.pms.pas
    pw.geometry.pas
    pw.map.pas
    pw.lights.pas
    pw.undo.pas
    pw.prefab.pas
    pw.config.pas
  gui/
    renderer.pas
    viewport.pas
    tools.pas
    frmmain.pas
    panels/
      frmscenery.pas
      frmwaypoints.pas
      frmdisplay.pas
      frmtools.pas
      frminfo.pas
    dialogs/
      frmmap.pas
      frmcolor.pas
      frmpreferences.pas
      frmtexture.pas
  tests/
    test_types.pas
    test_pms.pas
    test_geometry.pas
    test_map.pas
    test_lights.pas
    test_undo.pas
    test_prefab.pas
  fixtures/
    maps/
      empty.pms        (hand-crafted minimal map)
      one_poly.pms     (single polygon, each type)
      all_types.pms    (one poly of each of 26 types)
      max_entities.pms (near-limit counts)
      with_lights.pms  (PolyWorks extension, MapRandomID=-1)
      compiled.pms     (SaveAndCompile output, MapRandomID>0)
    prefabs/
      simple.pwf
```

---

## Milestone 1 — Foundation types and utilities

### What it introduces
- `pw.types.pas` — all `packed record` types that map directly to PMS binary layout
- `pw.utils.pas` — color and math helpers

### Types in pw.types.pas

**Binary-compatible (saved to file):**
```pascal
TVertex        { X,Y,Z,rhw: Single; Color: LongWord; tu,tv: Single }  32 bytes
TVertexNormal  { X,Y,Z,rhw: Single; Color: LongWord; tu,tv: Single }  32 bytes
TPolyNormals   { V: array[1..3] of TVertexNormal }                    96 bytes
TMapPoly       { V: array[1..3] of TVertex; Perp: TPolyNormals; Kind: Byte }  289 bytes
TSketchVertex  { X,Y,Z: Single }                                        12 bytes
TSketchLine    { V: array[1..2] of TSketchVertex }                     24 bytes
TProp          { active: WordBool; Style: SmallInt; Width,Height: LongInt;
                 X,Y,rotation,ScaleX,ScaleY: Single;
                 alpha,color,level: LongInt }                           48 bytes
TMapSceneryEntry { Name: array[0..50] of Byte; Date: LongInt }         55 bytes
TCollider      { active,X,Y,radius: ... }
TSaveSpawnPoint { active,X,Y,Team: LongInt }                           16 bytes
TMapOptions    { mapName: array[0..38] of Byte;
                 textureName: array[0..24] of Byte;
                 bgColor1,bgColor2: LongWord;
                 StartJet: LongInt;   ← 4 bytes, not 1
                 GrenadePacks,Medikits,Weather,Steps: Byte;
                 MapRandomID: LongInt }
TNewWaypoint   { active,id,X,Y: LongInt; Left,Right,Up,Down,m2,
                 pathNum,special: Byte; crap: array[1..5] of Byte;
                 connectionsNum: LongInt; Connections: array[1..20] of LongInt }
TConnection    { point1,point2: SmallInt }
TLightSource   { selected: Byte; color: TColor; intensity: Single;
                 range: SmallInt; X,Y,Z: Single }

**Editor-only (not saved):**
TColor         { R,G,B: Byte }
TEditorPoly    { WorldV: array[1..3] of TVertex;  ← world coords
                 ScreenV: array[1..3] of TVertex; ← screen coords
                 BaseColor: array[1..3] of TColor; ← unlit
                 SelVertex: array[1..3] of Byte;
                 Kind: Byte }
TEditorScenery { Prop: TProp; ScreenX,ScreenY: Single;
                 TexW,TexH: Single; Selected: Boolean }
TEditorWaypoint { X,Y: Single; Left,Right,Up,Down,m2: Boolean;
                  PathNum,Special,NumConns: Byte;
                  Selected,TempIndex: Integer }
TEditorSpawn   { X,Y: Single; Team: Byte; Selected: Boolean }
TEditorCollider { X,Y,Radius: Single; Selected: Boolean }
TEditorLight   { X,Y: Single; Color: TColor; Intensity: Single;
                 Range: Single; Selected: Boolean }
TSelectionState { SelPolys: array of Integer; NumSelPolys: Integer;
                  NumSelScenery,NumSelSpawns,NumSelColliders,
                  NumSelWaypoints,NumSelLights: Integer;
                  SelRect: array[0..3] of TVector2;
                  RCenter: TVector2 }
```

### Utilities in pw.utils.pas
```pascal
function  ARGB(a: Byte; rgb: LongWord): LongWord;
function  GetAlpha(color: LongWord): Byte;
function  GetRed/Green/Blue(color: LongWord): Byte;
function  RGBToLong(r,g,b: Byte): LongWord;
function  GetRGB(hexStr: string): LongWord;
function  Midpoint(a,b: Single): Single;
function  GetAngle(x1,y1,x2,y2: Single): Single;
function  Clamp(v,lo,hi: Single): Single;
function  IsBetween(v,lo,hi: Single): Boolean;
```

### VB6 functionality replaced
`modOpenSoldatMap.bas` (all types), `modUtils.bas` (all helpers), the `TColor`/`D3DVECTOR2` types currently pulled in from DirectX headers.

### Dependencies
None.

### Tests (test_types.pas)
- `SizeOf(TMapPoly)` = 289, `SizeOf(TProp)` = 48, `SizeOf(TMapOptions)` = 75 etc. — assert every binary struct size matches VB6 layout
- `SizeOf(TNewWaypoint)` = 4+4+4+4+7+5+4+(20*4) = 116 bytes
- ARGB encode/decode round-trips
- GetRGB parses known hex strings
- Midpoint, Clamp, IsBetween with boundary values

### Test fixtures
None needed; all tests use hard-coded values.

### Headless: YES | GUI: NO | OpenGL: NO

### Compatibility risks
- **VB6 `Boolean` in UDT is 2 bytes** — `TProp.active As Boolean` occupies 2 bytes in VB6. Use `WordBool` in Pascal.
- **VB6 `Integer` = 2 bytes, `Long` = 4 bytes** — use `SmallInt`/`LongInt` explicitly; never plain `Integer` in packed records.
- **No alignment padding** — VB6 UDTs are naturally packed (no implicit padding). All Pascal records must be `packed`.
- **TOptions.StartJet is `Long` (4 bytes)** — the VB6 source declares it as Long, so 4 bytes go in the file, not 1.
- The `TLightSource` struct uses `TColor` (3 bytes) inline — must not add padding between the Byte fields.

---

## Milestone 2 — PMS binary file format

### What it introduces
- `pw.pms.pas` with three public functions

### API
```pascal
type
  TPMSLoadResult = (plrOK, plrFileNotFound, plrVersionMismatch,
                    plrTruncated, plrCorrupt);
  TPMSData = record
    Version    : LongInt;
    Options    : TMapOptions;
    Polys      : array of TMapPoly;    { 1-based in VB6; 0-based here }
    SectDiv    : LongInt;
    Sectors    : TSectorTable;          { 51×51 cells }
    Props      : array of TProp;
    ScenNames  : array of TMapSceneryEntry;
    Colliders  : array of TCollider;
    Spawns     : array of TSaveSpawnPoint;
    Waypoints  : array of TNewWaypoint;
    Lights     : array of TLightSource;     { present if MapRandomID=-1 }
    Sketch     : array of TSketchLine;      { present if MapRandomID=-1 }
  end;

function LoadPMS(const Filename: string; out Data: TPMSData;
                 out Err: string): TPMSLoadResult;

{ Saves PolyWorks native format: MapRandomID=-1, raw coords, zero sector table }
function SavePMS(const Filename: string; const Data: TPMSData;
                 out Err: string): Boolean;

{ Saves game format: MapRandomID>0, centered coords, computed sector table }
function CompilePMS(const Filename: string; const Data: TPMSData;
                    out Err: string): Boolean;
```

`CompilePMS` calls `ComputeSectorTable` and `CentreCoordinates` from `pw.geometry.pas` internally.

### VB6 functionality replaced
`LoadFile`, `SaveFile`, `SaveAndCompile` (file I/O portions only, stripped of all UI).

### Dependencies
M1, M3 (for `CentreCoordinates`, `ComputeSectorTable`, `ComputeNormals`). *Milestone 2 can use stubs for M3 functions initially and be completed fully once M3 exists.*

### Tests (test_pms.pas)
- Load each fixture → assert `LoadResult = plrOK`
- Load `empty.pms` → assert PolyCount=0, Options fields match known values
- Load `one_poly.pms` → assert vertex coordinates, colour, and normal values exactly
- Load `with_lights.pms` (MapRandomID=-1) → assert lights and sketch lines present
- Load `compiled.pms` (MapRandomID>0) → assert no lights/sketch, trailing zeros present
- Round-trip SavePMS: load → save to temp → load again → deep-compare all fields
- Round-trip CompilePMS: load → compile → load compiled → verify MapRandomID>0, centred coords (bounding-box midpoint ≈ 0,0), sector table non-empty, type-3 polys absent from sectors
- Negative: truncated file → `plrTruncated`
- Negative: version ≠ 11 → `plrVersionMismatch`
- Negative: invalid scenery scale → silently skipped (replicates VB6 behaviour)
- Verify `StartJet` reads 4 bytes (not 1) — load a file with known StartJet byte pattern

### Test fixtures to create
All by hand in a small fixture-builder utility (can be a separate test helper that writes known binary patterns):
- `empty.pms` — version 11, no polys, no entities, MapRandomID=0
- `one_poly.pms` — one CW triangle, normal poly type
- `all_types.pms` — 26 polys, one of each type 0–25
- `with_lights.pms` — MapRandomID=-1, 2 lights, 1 sketch line
- `compiled.pms` — MapRandomID=1234, centered, valid sector table

### Headless: YES | GUI: NO | OpenGL: NO

### Compatibility risks
- **Pascal string encoding**: VB6 length-prefixed byte arrays (`mapName(0 To 38)` = byte 0 is the length, bytes 1..38 are chars). The length byte counts chars, not total bytes. Read/write using the same byte-array approach, not Pascal `string`.
- **Sector table in SavePMS is all zeros** — do not write anything meaningful; write 51×51 × (Integer 0) = 51×51×2 bytes = 5202 bytes of zeros. This matches VB6 `SaveFile`.
- **TProp.active**: WordBool — VB6 True = -1 (0xFFFF), False = 0. Write as 2-byte integer; preserve the sign when reading (any non-zero = True).
- **Scenery skipping on load**: props with `ScaleX/ScaleY` outside (−10000, 10000) or zero width/height are silently skipped. The offset counter must not corrupt subsequent reads.
- **Waypoint connections in file**: each `TNewWaypoint` has `connectionsNum` followed by `Connections(1..20)` — always write all 20 Long values regardless of `connectionsNum`.

---

## Milestone 3 — Geometry algorithms

### What it introduces
- `pw.geometry.pas` — all pure geometric functions, parameterised on data (no global state)

### API
```pascal
{ Polygon geometry }
function  PointInPoly(PX, PY: Single; const V: array of TVertex): Boolean;
function  IsPolyClockwise(const V: array of TVertex): Boolean;
procedure SwapVertices2And3(var V: array of TVertex);   { enforce CW }
procedure ComputePolyNormals(var Poly: TMapPoly);

{ Segment intersection }
function  SegmentsIntersect(x1,y1,x2,y2,x3,y3,x4,y4: Single): Boolean;
function  SegIntersectsHoriz(x1,y1,x2,y2, hx1,hx2,hy: Single): Boolean;
function  SegIntersectsVert(x1,y1,x2,y2, vx,vy1,vy2: Single): Boolean;

{ Sector table }
function  PolyInSector(SectorX, SectorY: Integer; const Poly: TMapPoly;
                       SectorDiv: Single): Boolean;
procedure BuildSectorTable(const Polys: array of TMapPoly;
                           MapW, MapH: Single; out Table: TSectorTable);

{ Coordinate helpers }
function  WorldToScreen(WorldX, WorldY, ScrollX, ScrollY, Zoom: Single;
                        out SX, SY: Single): Boolean;  { inline conversion }
function  ScreenToWorld(SX, SY, ScrollX, ScrollY, Zoom: Single;
                        out WX, WY: Single): Boolean;
function  SnapToGrid(Coord, GridSize, ScrollOffset, Zoom: Single): Single;
function  NearCoord(MouseCoord, PolyCoord, Range: Single): Boolean;
function  InSelRect(X, Y, RX1, RY1, RX2, RY2: Single): Boolean;

{ Bounding box / centering }
procedure PolyBoundingBox(const Polys: array of TMapPoly;
                          out MinX, MinY, MaxX, MaxY: Single);
procedure CentreCoordinates(var Polys: array of TMapPoly);

{ Zoom snapping }
function  SnapZoom(CurrentZoom, Direction, MinZoom: Single): Single;
```

### VB6 functionality replaced
`PointInPoly`, `IsCW`, `SegXSeg`, `SegXHorizSeg`, `SegXVertSeg`, `SegmentsIntersect`, `IsInSector` (the full inner test), sector table builder loop in `SaveAndCompile`, `SnapVertexToGrid`, `NearCoord`, `InSelRect`, `GetZoomDir`, bounding-box logic in `SaveAndCompile`.

### Dependencies
M1 only.

### Tests (test_geometry.pas)
**PointInPoly:**
- CW triangle: centroid → inside, external point → outside, vertex → boundary behaviour
- CCW triangle: confirm it returns false for centroid (correct: PointInPoly only works for CW)
- Degenerate zero-area triangle → does not crash

**IsPolyClockwise:**
- Explicit CW triangle → True
- Explicit CCW triangle → False
- After SwapVertices2And3: CCW → CW confirmed by IsPolyClockwise

**ComputePolyNormals:**
- For a unit right-triangle: assert Perp.V[1].X = sin of known angle, .Y = cos
- For type-18 polygon: assert .Z preserved (bounciness not clobbered)
- For all other types: assert .Z = 1.0

**SegmentsIntersect:**
- Crossing segments → True
- Parallel non-overlapping → False
- Co-linear overlapping → test VB6 behaviour (parametric t/u check)
- T-intersection (endpoint on segment) → True
- Endpoint touching → parametric boundary behaviour

**SnapToGrid:**
- At zoom=1, scroll=0, grid=10: coord 14 → 10, coord 16 → 20
- At zoom=2, scroll=5: verify offset propagation
- Negative coordinates

**PolyInSector / BuildSectorTable:**
- Single polygon centred in sector 0,0 → appears in sector (25,25) cell
- Type-3 polygon → absent from table
- Polygon overlapping sector boundary → appears in both sectors
- Max 256 polys per sector clamped

**InSelRect:**
- Point strictly inside → True; on edge → False (exclusive test)

**CentreCoordinates:**
- After centring, bounding-box midpoint X and Y both ≈ 0

### Test fixtures
None needed; all geometry tests use inline-constructed vertex arrays.

### Headless: YES | GUI: NO | OpenGL: NO

### Compatibility risks
- **`IsCW` uses centroid-in-poly, not cross-product sign** — must replicate exactly; a cross-product replacement would give different results for degenerate cases. The centroid is `Midpoint(v1.X, Midpoint(v2.X, v3.X))` (nested, not averaged over three — note: nested Midpoint gives the same result as `(a+b+c)/2` only when the outer Midpoint picks the inner result, so it's actually `(a + (b+c)/2) / 2 = a/2 + b/4 + c/4` — this is NOT the centroid. It is biased toward vertex 1. Must replicate this exactly.)
- **`SnapToGrid` offset formula** — uses `Int()` (floor toward negative infinity in VB6), not `Trunc` (truncate toward zero). In Pascal, use `Floor()` not `Trunc()`.
- **`InSelRect` is exclusive** — uses `>` and `<`, not `>=`/`<=`. Points exactly on the selection rectangle border are NOT selected.
- **Sector count clamped to 256** — when `polysInSector > 256`, cap at 256. Do not write more than 256 indices per sector cell.

---

## Milestone 4 — Map document model

### What it introduces
- `pw.map.pas` — `TMapDocument` class: owns all mutable map state and exposes atomic editing operations

### API (key surface)
```pascal
TMapDocument = class
public
  { State }
  Polys       : array of TEditorPoly;
  PolyCount   : Integer;
  Scenery     : array of TEditorScenery;
  SceneryCount: Integer;
  SceneryNames: array of string;   { texture filenames, 1-based }
  Spawns      : array of TEditorSpawn;
  SpawnCount  : Integer;
  Colliders   : array of TEditorCollider;
  ColliderCount: Integer;
  Waypoints   : array of TEditorWaypoint;
  WaypointCount: Integer;
  Connections : array of TConnection;
  ConnCount   : Integer;
  Lights      : array of TEditorLight;
  LightCount  : Integer;
  Sketch      : array of TSketchLine;
  SketchCount : Integer;
  Options     : TMapOptions;
  Modified    : Boolean;

  { View state — owned by document so undo can restore scroll/zoom }
  Zoom        : Single;
  ScrollX, ScrollY : Single;

  { Selection }
  Selection   : TSelectionState;

  { Lifecycle }
  constructor Create;
  procedure   NewMap;
  procedure   LoadFromPMS(const Data: TPMSData);
  procedure   SaveToPMS(out Data: TPMSData);

  { Coordinate conversion (delegates to pw.geometry) }
  procedure WorldToScreen(WX, WY: Single; out SX, SY: Single);
  procedure ScreenToWorld(SX, SY: Single; out WX, WY: Single);
  procedure RebuildScreenCache;   { recalc all ScreenV from WorldV }

  { Editing operations — all call RebuildScreenCache internally }
  procedure SetZoom(NewZoom, CentreScreenX, CentreScreenY: Single);
  procedure Scroll(DX, DY: Single);
  procedure AddPoly(const WorldV: array of TVertex; Kind: Byte);
  procedure DeleteSelected;
  procedure MoveSelectedWorld(DX, DY: Single);
  procedure RotateSelected(AngleDeg: Single; CX, CY: Single);
  procedure ScaleSelected(SX, SY: Single; CX, CY: Single);
  procedure SetSelectedPolyType(Kind: Byte);
  procedure SetSelectedVertexColor(Color: LongWord);
  procedure AddScenery(NameIndex: Integer; WorldX, WorldY: Single);
  procedure AddSceneryName(const Filename: string): Integer;
  procedure AddSpawn(WorldX, WorldY: Single; Team: Byte);
  procedure AddCollider(WorldX, WorldY, Radius: Single);
  procedure AddWaypoint(WorldX, WorldY: Single);
  procedure AddConnection(WP1, WP2: Integer);
  procedure AddLight(WorldX, WorldY: Single; Color: TColor;
                     Intensity: Single; Range: Single);
  procedure ClearSelection;
  procedure SelectByRegion(SX1, SY1, SX2, SY2: Single; Mode: TSelMode);
  procedure SelectVertex(PolyIdx, VertIdx: Integer; Mode: TSelMode);
  procedure SnapSelectedToGrid;
  procedure SnapSelectedToVertex;
end;
```

`LoadFromPMS` converts a `TPMSData` (file representation) into the editor arrays, building `ScreenV` caches, unpacking waypoint connection lists into the flat `Connections` array, and handling the alpha=0→255 scenery remapping.

`SaveToPMS` is the inverse: packs editor arrays back into `TPMSData`. Called by `pw.pms.SavePMS` and `pw.pms.CompilePMS`.

### VB6 functionality replaced
All `Private` field declarations on `frmOpenSoldatMapEditor` that hold map data; `NewMap`; the editing operations: `DeletePolys`, `MoveSelected`, `SnapSelected`, `ApplyScale`, `ApplyRotate`, `CreatePolys`, `CreateScenery`, `CreateSpawn`, `CreateCollider`, `CreateWaypoint`, `CreateConnection`; coordinate conversion math scattered across every mouse handler.

### Dependencies
M1, M2, M3.

### Tests (test_map.pas)
- `NewMap` → all counts = 0, zoom = 1.0
- `LoadFromPMS` with `empty.pms` data → all arrays empty
- `LoadFromPMS` with `one_poly.pms` → PolyCount=1, WorldV matches file, ScreenV = (WorldV − 0) × 1.0 at default zoom/scroll
- `LoadFromPMS` with `with_lights.pms` → LightCount > 0
- `LoadFromPMS` → `SaveToPMS` → `LoadFromPMS` → assert all fields equal (round-trip via document)
- `AddPoly` with CCW vertices → vertices 2 and 3 swapped (CW enforced)
- `MoveSelectedWorld(10, 0)` on selected poly → WorldV.X each increased by 10
- `DeleteSelected` (whole poly) → PolyCount decreased; remaining poly indices still valid
- `DeleteSelected` (waypoint) → ConnCount decreased; connections to deleted WP removed; remaining connection indices remapped
- `SetZoom` at cursor → ScrollX/ScrollY adjusted so cursor world position unchanged
- `RebuildScreenCache` → ScreenV.X = (WorldV.X − ScrollX) × Zoom
- Scenery alpha=0 on load → remapped to 255
- `AddSceneryName` returns 1-based index; `SceneryNames[0]` is reserved (fallback)
- Scenery `Scenery[0]` is always the cursor (not included in SceneryCount)

### Test fixtures
Uses TPMSData values assembled in-test (no file I/O required for most tests; PMS load tests use M2 fixtures).

### Headless: YES | GUI: NO | OpenGL: NO

### Compatibility risks
- **1-based vs 0-based arrays**: VB6 arrays are 1-based for entity arrays but 0-based for the cursor scenery. `TMapDocument` uses 0-based Pascal arrays but `Scenery[0]` must remain the cursor slot.
- **`TEditorPoly.ScreenV` must always mirror `WorldV`**: any code path that modifies `WorldV` without calling `RebuildScreenCache` is a bug. Consider making WorldV read-only and exposing only mutation methods.
- **Waypoint connection packing**: `TNewWaypoint.Connections` always has 20 slots in the file; only `connectionsNum` slots are valid. On load, only valid connections are unpacked. On save, exactly 20 slots must be written (padded with 0).
- **`DeleteSelected` partial-poly semantics**: when only some vertices of a polygon are selected, the polygon is NOT deleted — only its selection state changes. When all 3 vertices are selected (sum=3), the polygon is deleted. This distinction must be implemented exactly.

---

## Milestone 5 — Light blending

### What it introduces
- `pw.lights.pas` — light-to-vertex colour blending; pure math, no rendering

### API
```pascal
{ Applies all lights in Doc.Lights to Doc.Polys[].ScreenV.Color in place.
  Reads base colors from Doc.Polys[].BaseColor.
  Does not touch WorldV. Does not call Render. }
procedure ApplyLights(var Doc: TMapDocument);

{ Restores ScreenV.Color from BaseColor for all polygons (lights-off mode) }
procedure RestoreBaseColors(var Doc: TMapDocument);

{ Apply lights to a single vertex (used after editing one vertex) }
procedure ApplyLightsToVertex(var Doc: TMapDocument; PolyIdx, VertIdx: Integer);
```

### VB6 functionality replaced
`ApplyLights`, `ApplyLightsToVert`, `SetLightsMode`.

### Dependencies
M1, M3 (for dot product via normal vectors), M4.

### Tests (test_lights.pas)
- No lights → `ApplyLights` is a no-op; colours unchanged
- One light directly above a horizontal polygon → vertex colours brightened
- Light outside radius → vertex colours unchanged
- `RestoreBaseColors` after `ApplyLights` → colours equal BaseColor
- After deleting last light → `RestoreBaseColors` called automatically
- Known dot-product case: light at known position, polygon at known angle → assert specific ARGB output
- Intensity = 0 → vertex colour = BaseColor (no blending)

### Test fixtures
None; all tests use programmatically constructed `TMapDocument` instances.

### Headless: YES | GUI: NO | OpenGL: NO

### Compatibility risks
- **Light colour blend formula** must replicate VB6 exactly. The VB6 code uses integer dot-product arithmetic with specific clamping. Verify against known inputs/outputs before declaring done.
- **Alpha channel preservation**: light blending must not alter the alpha component of vertex colours; only R/G/B are modified.

---

## Milestone 6 — Undo / redo

### What it introduces
- `pw.undo.pas` — in-memory snapshot ring buffer

### API
```pascal
TMapSnapshot = record
  Polys, Scenery, Spawns, Colliders,
  Waypoints, Connections, Lights, Sketch : { deep copies of TMapDocument arrays }
  Selection : TSelectionState;
  Options   : TMapOptions;
  { Note: zoom/scroll NOT included, matching VB6 behaviour }
end;

TUndoStack = class
  constructor Create(MaxDepth: Integer);  { default 16 }
  { Call before any destructive edit }
  procedure   Push(const Doc: TMapDocument);
  { Returns True and restores state if possible }
  function    Undo(Doc: TMapDocument): Boolean;
  function    Redo(Doc: TMapDocument): Boolean;
  procedure   Clear;
  property    CanUndo   : Boolean;
  property    CanRedo   : Boolean;
  property    UndoDepth : Integer;
  property    RedoDepth : Integer;
end;
```

`TMapDocument` gets a `procedure MarkDirty` which sets `Modified := True` and the `SelectionChanged` flag. The consumer (tool layer) calls `UndoStack.Push(Doc)` before the first destructive edit after a selection-only change.

The ring buffer uses a fixed-size array of `TMapSnapshot`. No file I/O.

### VB6 functionality replaced
`SaveUndo`, `LoadUndo`, the `undo\undoN.pwn` file ring buffer, the `currentUndo`/`numUndo`/`numRedo`/`selectionChanged` variables.

### Dependencies
M1, M4.

### Tests (test_undo.pas)
- Push 1 → Undo → state restored, CanRedo=True
- Push N (N < MaxDepth) → N undos → initial state exactly
- Push N > MaxDepth → only MaxDepth undos available; oldest state is oldest retained
- Undo, then new Push → RedoDepth = 0 (redo stack cleared)
- Redo with nothing to redo → returns False, state unchanged
- Undo with nothing to undo → returns False, state unchanged
- Undo does NOT restore zoom/scroll (matching VB6 behaviour)
- Multiple consecutive Push calls without intervening undo → each creates a new snapshot
- `Clear` → CanUndo = False, CanRedo = False

### Test fixtures
None; build programmatic `TMapDocument` instances.

### Headless: YES | GUI: NO | OpenGL: NO

### Compatibility risks
- **Sketch lines are NOT restored on undo** — matching the known VB6 bug. Intentionally omit sketch from `TMapSnapshot` unless a deliberate decision is made to fix this (document the choice).
- **Map options are NOT included in undo** — matching VB6. Options (texture name, bg colours, etc.) are not snapshotted.
- **`selectionChanged` timing** — in VB6, the first destructive edit after a selection change triggers an implicit snapshot. The tool layer must replicate this by calling `Push` before the edit when `SelectionChanged` is true.

---

## Milestone 7 — Prefab format

### What it introduces
- `pw.prefab.pas` — save and load polygon/entity subsets as `.pwf` files

### API
```pascal
function SavePrefab(const Filename: string; const Doc: TMapDocument;
                    out Err: string): Boolean;
function LoadPrefab(const Filename: string; Doc: TMapDocument;
                    out Err: string): Boolean;
```
`LoadPrefab` appends to the current document (deselects all, then appends and selects new items). `SavePrefab` saves only the currently selected entities. Both functions use the same sequential binary format as VB6: poly count, polys, scenery count, scenery+name, collider count, spawn count, waypoint count, connection count.

### VB6 functionality replaced
`SavePrefab`, `LoadPrefab`.

### Dependencies
M1, M2, M4.

### Tests (test_prefab.pas)
- Save 2 polys and 1 spawn → file length matches expected
- Load into fresh document → PolyCount=2, SpawnCount=1
- Round-trip: save subset → load into blank doc → field-for-field compare
- Waypoint connections in prefab are remapped to new local indices on load

### Test fixtures
- `simple.pwf` — hand-crafted prefab with 1 poly and 1 spawn

### Headless: YES | GUI: NO | OpenGL: NO

### Compatibility risks
- Prefab files carry raw world coordinates (no centering).
- Scenery names are written as Pascal byte-length arrays (same as PMS format).

---

## Milestone 8 — Configuration

### What it introduces
- `pw.config.pas` — `TAppConfig` record + load/save via `TIniFile`

### Key settings
```pascal
TAppConfig = record
  UndoDepth     : Integer;   { default 16 }
  SnapRadius    : Single;    { default 8.0 }
  OhSnap        : Boolean;   { vertex snapping on/off }
  SnapToGrid    : Boolean;
  GridSize      : Integer;   { inc; default 10 }
  MinZoom       : Single;    { gMinZoom; default 0.0625 }
  MaxZoom       : Single;
  SoldatPath    : string;
  RecentFiles   : array[0..9] of string;
  Theme         : TTheme;
  { display toggles }
  ShowPolys, ShowWireframe, ShowPoints,
  ShowGrid, ShowObjects, ShowWaypoints,
  ShowLights, ShowSketch : Boolean;
  { per-polytype colours (26 entries) }
  PolyTypeColors : array[0..25] of LongWord;
end;

procedure LoadConfig(const IniPath: string; out Cfg: TAppConfig);
procedure SaveConfig(const IniPath: string; const Cfg: TAppConfig);
function  DetectSoldatPath: string;   { registry on Win; config file elsewhere }
```

### VB6 functionality replaced
`modConfig.bas` (`LoadSettings`, `SaveSettings`), the registry-based Soldat path detection in `modOSME.bas`.

### Dependencies
M1.

### Tests
- Load an INI file with known content → assert field values
- Round-trip: save → load → compare all fields
- Missing INI → defaults applied (not a crash)
- `DetectSoldatPath` on Windows with known registry key (mock or skip if no registry)

### Headless: YES | GUI: NO | OpenGL: NO

### Compatibility risks
- VB6 uses `GetPrivateProfileString`/`WritePrivateProfileSection` (Win32). `TIniFile` in FPC is cross-platform but writes UTF-8 whereas VB6 writes ANSI. INI files from the VB6 version will parse correctly (ASCII content only).
- Palette is saved separately to `appPath\palettes\current.txt`; include a `LoadPalette`/`SavePalette` pair.

---

## Milestone 9 — OpenGL renderer

### What it introduces
- `renderer.pas` — `TRenderer` class: translates `TMapDocument` state into OpenGL draw calls

### API
```pascal
TViewSettings = record
  ShowPolys, ShowWireframe, ShowPoints : Boolean;
  ShowGrid, ShowObjects, ShowWaypoints : Boolean;
  ShowLights, ShowSketch, ShowTexture  : Boolean;
  ShowBackground                       : Boolean;
  GridSize                             : Integer;
end;

TRenderer = class
  constructor Create;
  destructor  Destroy; override;
  procedure   LoadMapTexture(const Path: string);
  procedure   LoadSceneryTexture(Index: Integer; const Path: string);
  procedure   FreeTextures;
  { Called from TMapViewport.OnPaint with a valid GL context active }
  procedure   Render(const Doc: TMapDocument; const VS: TViewSettings;
                     ViewW, ViewH: Integer);
end;
```

Render order matches VB6 exactly (see §10 of arch.md). Uses OpenGL 2.1 with `glBegin`/`glEnd` (immediate mode) for initial implementation; VAO/VBO upgrade is a later optimisation.

Image loading via **stb_image** (single-header library; FPC bindings exist or write a thin wrapper). Supports PNG, BMP, TGA, JPEG, and GIF (first frame only — matching VB6 GIF behaviour).

Scenery is drawn using `glPushMatrix` + rotation/scale transforms (replacing `D3DXSprite`).

Colour key transparency (`&HFF00FF00` = bright green) must be applied at texture upload: scan the pixel data and set alpha=0 for any pixel matching the key.

### VB6 functionality replaced
`modRender.bas` (`InitDX8`, `LoadTexture`, `scenerySprite`), the `Render()` procedure in `frmOpenSoldatMapEditor.frm`, the entire DirectX 8 dependency.

### Dependencies
M1, M3, M4.

### Tests
Visual smoke tests only at this stage (no automated pixel comparison). Build a minimal headless test harness that creates an offscreen OpenGL context (via `glfw` or platform-specific) and calls `Render` without crashing.

### Test fixtures
- A sample PNG and a sample GIF for texture loading tests.

### Headless: NO (requires OpenGL context) | GUI: YES | OpenGL: YES

### Compatibility risks
- **Colour key transparency** — must replicate the bright-green keying. VB6 used DX8's `D3DXCreateTextureFromFileEx` with `D3DCOLOR_COLORVALUE` colour key. Implement by post-processing pixel data: any pixel with R<10, G>245, B<10 (approximate) → alpha=0.
- **Pre-transformed coordinates** — VB6 used `D3DFVF_XYZRHW` (screen space, rhw=1). In OpenGL, use an orthographic projection matrix matching `glOrtho(0, ViewW, ViewH, 0, -1, 1)` so world→screen coordinates from `TMapDocument.ScreenV` go directly to NDC without additional transformation.
- **Scenery rotation origin** — VB6 uses the sprite's top-left as the origin for rotation. Match this in OpenGL: translate to position, then rotate, then draw quad from (0,0) to (W,H).
- **Background gradient** — render as two-vertex coloured triangle strip; no texture.

---

## Milestone 10 — Viewport control

### What it introduces
- `viewport.pas` — `TMapViewport`: a `TOpenGLControl` subclass that owns the GL context and dispatches input events

### Responsibilities
- Holds references to `TMapDocument`, `TRenderer`, `TToolController`, `TUndoStack`
- On `Paint`: activates context, calls `Renderer.Render(Doc, ViewSettings, Width, Height)`
- On `MouseDown/Move/Up`: converts screen→world via `Doc.ScreenToWorld`; forwards to `ActiveTool`
- On `MouseWheel`: calls `Doc.SetZoom`; triggers repaint
- On `KeyDown`/`KeyUp`: forwards to `ActiveTool`; handles global shortcuts (Ctrl+Z, Ctrl+Y, Delete)
- Middle-button drag: pans `Doc.ScrollX/ScrollY`
- Exposes `procedure Repaint` and `procedure SetActiveTool(Tool: ITool)`

### VB6 functionality replaced
`picViewport` PictureBox + all `picViewport_MouseDown/MouseMove/MouseUp/MouseWheel` event handlers; the `currentFunction`/`currentTool` dispatch logic; DirectInput 8 keyboard buffer (replaced by standard LCL `KeyDown`/`KeyUp`).

### Dependencies
M4, M6, M9.

### Tests
No automated tests (requires display). Manual smoke test: load a map, see polygons rendered.

### Headless: NO | GUI: YES | OpenGL: YES

---

## Milestone 11 — Tool system

### What it introduces
- `tools.pas` — `ITool` interface + one implementation per tool; `TToolController`

### ITool interface
```pascal
ITool = interface
  procedure MouseDown(Doc: TMapDocument; WX, WY: Single;
                      Button: TMouseButton; Shift: TShiftState);
  procedure MouseMove(Doc: TMapDocument; WX, WY: Single;
                      Shift: TShiftState);
  procedure MouseUp  (Doc: TMapDocument; WX, WY: Single;
                      Button: TMouseButton);
  procedure KeyDown  (Doc: TMapDocument; Key: Word; Shift: TShiftState);
  procedure Activate (Doc: TMapDocument);
  procedure Deactivate(Doc: TMapDocument);
  function  Cursor: TCursor;
end;
```

### Tool implementations
| Class | Tool constant | VB6 replacement |
|---|---|---|
| `TMoveTool` | TOOL_MOVE | middle-click pan, background drag |
| `TVertexSelectTool` | TOOL_VSELECT + VSELADD + VSELSUB | rubber-band + click select |
| `TPolyTool` | TOOL_POLY | `CreatePolys` |
| `TSceneryTool` | TOOL_SCENERY | `CreateScenery` |
| `TSpawnTool` | TOOL_SPAWNS | spawn placement |
| `TWaypointTool` | TOOL_WAYPOINTS | waypoint placement |
| `TConnectionTool` | TOOL_CONNECTION | `CreateConnection` |
| `TTextureTool` | TOOL_TEXTURE | `StretchingTexture` UV edit |
| `TColorTool` | TOOL_COLOR | vertex colour painting |
| `TSketchTool` | TOOL_SKETCH | sketch line drawing |
| `TColliderTool` | TOOL_COLLIDER | collider placement |
| `TLightTool` | TOOL_LIGHT | light placement |
| `TDepthTool` | TOOL_DEPTH | depth sorting |
| `TDepthMapTool` | TOOL_DEPTHMAP | depth map mode |

`TToolController` holds the active `ITool` and the `TUndoStack`. After any operation that modifies the document, the tool calls `UndoStack.Push(Doc)`.

### Dependencies
M4, M6, M10.

### Tests
Unit tests for individual tool state machines where possible (e.g., `TPolyTool`: after 3 `MouseDown` calls with CW vertices, `Doc.PolyCount` increased by 1, winding is CW). These can be tested headlessly by calling `MouseDown` directly without a display.

### Headless: PARTIAL (state machine logic only) | GUI: YES (for full integration)

---

## Milestone 12 — Main window

### What it introduces
- `frmmain.pas` — `TMainForm`: main application window; contains `TMapViewport`, menu bar, status bar, toolbar

### Responsibilities
- Hosts `TMapViewport`, `TToolController`, `TUndoStack`, `TAppConfig`
- Implements File menu: New, Open, Save, Save As, Save and Compile, recent files list
- Implements Edit menu: Undo, Redo, Select All, Deselect, Delete, Copy, Paste, Cut
- Implements View menu: delegates to `TViewSettings` on the viewport
- Status bar: polygon count, scenery count, selected count
- Toolbar: tool buttons (delegates to `TToolController`)
- No custom borderless chrome — use standard LCL TForm decoration

### VB6 functionality replaced
`frmOpenSoldatMapEditor.frm` skeleton (chrome, menus, toolbar); eliminates `MBMouse.ocx`, `COMDLG32.OCX`, all custom window-management Win32 calls.

### Dependencies
M8, M10, M11.

### Tests
No automated tests. Smoke test: open, load map, save map, close.

### Headless: NO | GUI: YES | OpenGL: YES

---

## Milestone 13 — Floating panels

### What it introduces
- `panels/frmscenery.pas` — scenery texture list, level selector, rotate/scale flags
- `panels/frmwaypoints.pas` — waypoint type flags, path number, special action, path filter
- `panels/frmdisplay.pas` — visibility toggles (delegates to `TViewSettings`)
- `panels/frmtools.pas` — tool palette buttons
- `panels/frminfo.pas` — live properties of selected entity

### VB6 functionality replaced
`frmScenery.frm`, `frmWaypoints.frm`, `frmDisplay.frm`, `frmTools.frm`, `frmInfo.frm`, `frmTaskBar.frm`.

### Notes
- These panels read/write state through `TMapDocument` and `TToolController`, never through global variables.
- `frmScenery` loads the scenery texture list from `Doc.SceneryNames`; does not own any array state.
- `frmDisplay` toggles fields in `TViewSettings` which is owned by `TMapViewport`.

### Dependencies
M12.

### Tests
No automated tests.

### Headless: NO | GUI: YES | OpenGL: NO (panels themselves)

---

## Milestone 14 — Dialogs

### What it introduces
- `dialogs/frmmap.pas` — map options (texture, bg colours, jet/grenade/medkit/weather/steps)
- `dialogs/frmcolor.pas` — HSV/RGB colour picker
- `dialogs/frmpreferences.pas` — preferences (undo depth, snap settings, paths, key bindings)
- `dialogs/frmtexture.pas` — UV coordinate offset editor for selected vertices

### VB6 functionality replaced
`frmMap.frm`, `frmColor.frm`, `frmPreferences.frm`, `frmTexture.frm`, `frmPalette.frm`.

### Notes
- `frmmap` reads/writes `Doc.Options` directly; calls `Doc.Modified := True` on OK.
- `frmpreferences` reads/writes `TAppConfig`; does not touch `TMapDocument`.
- `frmcolor` is a reusable modal dialog; takes initial colour, returns new colour.

### Dependencies
M8, M12.

### Tests
No automated tests. Smoke test: open each dialog, change a value, confirm change reflected in document.

### Headless: NO | GUI: YES

---

## Numbered Implementation Roadmap

Each step is small enough to be completed and tested independently. Steps within the same milestone can be done in any order.

```
Step  1  pw.types.pas — all packed record types
Step  2  pw.utils.pas — colour and math helpers
Step  3  test_types.pas — SizeOf assertions and colour round-trips     [M1 complete]

Step  4  test fixture builder — write known .pms binary files to fixtures/
Step  5  pw.pms.pas — LoadPMS (reader only)
Step  6  test_pms.pas load tests                                        [M2a]
Step  7  pw.pms.pas — SavePMS (writer; zero sector table)
Step  8  pw.pms.pas — CompilePMS stub (copies + sets MapRandomID; no centering yet)
Step  9  test_pms.pas round-trip tests                                  [M2b partial]

Step 10  pw.geometry.pas — PointInPoly, IsPolyClockwise, SwapVertices2And3
Step 11  pw.geometry.pas — ComputePolyNormals
Step 12  pw.geometry.pas — SnapToGrid, NearCoord, InSelRect, WorldToScreen, ScreenToWorld
Step 13  pw.geometry.pas — SegmentsIntersect + axis-aligned variants
Step 14  pw.geometry.pas — PolyBoundingBox, CentreCoordinates
Step 15  pw.geometry.pas — PolyInSector, BuildSectorTable
Step 16  test_geometry.pas — full geometry test suite                   [M3 complete]

Step 17  pw.pms.pas — CompilePMS full (centering + sector table from M3)
Step 18  test_pms.pas — compile round-trip tests                        [M2 complete]

Step 19  pw.map.pas — TMapDocument data fields, NewMap, LoadFromPMS, SaveToPMS
Step 20  pw.map.pas — WorldToScreen/ScreenToWorld, RebuildScreenCache, SetZoom, Scroll
Step 21  pw.map.pas — AddPoly, DeleteSelected (polys only)
Step 22  pw.map.pas — AddScenery, DeleteSelected (scenery)
Step 23  pw.map.pas — AddSpawn, AddCollider, DeleteSelected (objects)
Step 24  pw.map.pas — AddWaypoint, AddConnection, DeleteSelected (waypoints + connection remapping)
Step 25  pw.map.pas — AddLight, DeleteSelected (lights)
Step 26  pw.map.pas — MoveSelectedWorld, RotateSelected, ScaleSelected
Step 27  pw.map.pas — SelectByRegion, SelectVertex, ClearSelection, SnapSelectedToGrid, SnapSelectedToVertex
Step 28  test_map.pas — full map model test suite                       [M4 complete]

Step 29  pw.lights.pas — ApplyLights, RestoreBaseColors, ApplyLightsToVertex
Step 30  test_lights.pas                                                [M5 complete]

Step 31  pw.undo.pas — TUndoStack, TMapSnapshot
Step 32  test_undo.pas                                                  [M6 complete]

Step 33  pw.prefab.pas — SavePrefab, LoadPrefab
Step 34  test_prefab.pas                                                [M7 complete]

Step 35  pw.config.pas — TAppConfig, LoadConfig, SaveConfig, DetectSoldatPath
         (+ LoadPalette/SavePalette)                                    [M8 complete]

Step 36  renderer.pas — stb_image integration + LoadMapTexture, LoadSceneryTexture
Step 37  renderer.pas — background gradient + polygon rendering (textured, wireframe, points)
Step 38  renderer.pas — scenery rendering (3 layers, rotation, scale, colour key)
Step 39  renderer.pas — overlay rendering (grid, objects, waypoints, sketch, lights)
         Manual smoke test: load a map, confirm visual output           [M9 complete]

Step 40  viewport.pas — TMapViewport: GL context, Paint, mouse events, zoom/pan
Step 41  viewport.pas — keyboard shortcuts (Ctrl+Z/Y, Delete, arrows), middle-button pan
         Manual smoke test: pan, zoom, click polys                     [M10 complete]

Step 42  tools.pas — ITool interface, TToolController
Step 43  tools.pas — TVertexSelectTool (rubber-band + click; integrates with TUndoStack)
Step 44  tools.pas — TPolyTool (3-click creation, snap, CW enforcement)
Step 45  tools.pas — TMoveTool (drag selected; background pan)
Step 46  tools.pas — TSceneryTool (1–3 click placement)
Step 47  tools.pas — TSpawnTool, TColliderTool, TLightTool (single-click placement)
Step 48  tools.pas — TWaypointTool, TConnectionTool
Step 49  tools.pas — TTextureTool, TColorTool, TSketchTool, TDepthTool, TDepthMapTool
         Headless state-machine tests for TPolyTool, TVertexSelectTool  [M11 complete]

Step 50  frmmain.pas — TMainForm skeleton, File/Edit/View menus, toolbar, status bar
Step 51  frmmain.pas — File Open/Save/SaveAs/Compile (wires pw.pms into UI)
Step 52  frmmain.pas — Undo/Redo menu items, recent files list
         Smoke test: open map, edit, undo, compile                     [M12 complete]

Step 53  panels/frmdisplay.pas — visibility toggles
Step 54  panels/frmtools.pas — tool palette
Step 55  panels/frmscenery.pas — scenery list + level/rotate/scale flags
Step 56  panels/frmwaypoints.pas — waypoint flags and path filter
Step 57  panels/frminfo.pas — live selection properties                 [M13 complete]

Step 58  dialogs/frmmap.pas — map options dialog
Step 59  dialogs/frmcolor.pas — colour picker
Step 60  dialogs/frmpreferences.pas — preferences
Step 61  dialogs/frmtexture.pas — UV offset editor                     [M14 complete]

Step 62  System test: load reference map → perform known edit sequence →
         compile → compare output byte-for-byte against VB6 reference   [done]
```

Total: 62 steps across 14 milestones. Steps 1–35 are headless and can be completed before the GUI work begins.
