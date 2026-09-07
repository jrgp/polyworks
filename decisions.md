# PolyWorks Rewrite — Implementation Decisions

## Format decisions

### PMS binary format sizes (validated against 97 real maps)
- `TPMSOptions` = 84 bytes (not 75 as originally stated in arch.md); StartJet is LongInt (4 bytes)
- `TPMSProp` = 44 bytes (not 48); no padding between fields
- Sector table in native format: 51×51 cells of zeros (VB6 SaveFile behavior)
- Native format: MapRandomID = -1; lights and sketch present
- Compiled format: MapRandomID > 0; sector table computed; 4×SmallInt trailing zeros

### Geometry quirks preserved intentionally
- `IsPolyClockwise` uses biased centroid: CX = V1.X/2 + V2.X/4 + V3.X/4 (NOT true centroid)
- `SnapToGrid` uses Floor() (VB6 Int()), NOT Trunc()
- `InSelRect` is exclusive: strictly > and < (edges not included)

## Architecture decisions

### No global state
TMapDocument is owned by TMainForm and passed by reference to all other components.
The GUI never owns application state.

### OpenGL 2.1 immediate mode
Using glBegin/glEnd for initial implementation. VBO upgrade deferred as optimization.
Orthographic projection: glOrtho(0, ViewW, ViewH, 0, -1, 1) matching VB6 D3DFVF_XYZRHW.

### stb_image for texture loading
Using stb_image.h via a thin C wrapper (pw_stb_image.c → libpw_stb_image.a).
Colour-key transparency: R<10, G>245, B<10 → alpha=0.

### No .lfm form files
All Lazarus forms built programmatically in code to avoid designer dependency
and simplify cross-platform portability.

### Undo intentionally excludes sketch and map options
Matching known VB6 behavior (sketch not in undo is a VB6 bug, preserved for compatibility).

### Scenery alpha=0 remapped to 255
VB6 behavior: on load, any scenery with alpha=0 is treated as fully opaque (255).

### 1-based entity indices in file; 0-based in TMapDocument arrays
File format uses 1-based waypoint connections; TMapDocument stores 0-based with
TEditorConnection.Point1/Point2 as 1-based indices (matching VB6 convention for
waypoint connection remapping on delete).

## Compatibility risks documented

- VB6 Boolean in UDT = 2 bytes (WordBool); used in TPMSProp.Active
- Waypoint connections: always 20 slots in file regardless of connectionsNum
- Type-3 (NoCollide) polygons excluded from sector table
- Poly type 24/25 drawn before scenery in render order (depth buffer polys)
- Scenery rotation origin = top-left (matches VB6 D3DXSprite behavior)
