# PolyWorks Source Port Inventory

Comprehensive mapping of every original VB6 source file to the C++ port.

**Methodology:** For each original file, every significant function/event/feature
was evaluated against the C++ implementation. Only `PARTIALLY PORTED` and
`NOT PORTED` items are candidates for code changes.

---

## Summary

| Status                  | Count |
|-------------------------|------:|
| PORTED                  |   168 |
| PARTIALLY PORTED        |     8 |
| NOT PORTED              |     3 |
| OBSOLETE / NOT APPLICABLE|    14 |

---

## Original Files

### `modOpenSoldatMap.bas` — Data types and map I/O

| Original Symbol | Functionality | C++ Equivalent | Status | Notes |
|---|---|---|---|---|
| `TVertex` | Vertex with position, color, UV | `EditorVertex` | PORTED | Split into world/screen caches |
| `TMapPoly` | Polygon with 3 vertices + normals | `EditorPoly` + `PmsPolygon` | PORTED | Editing model vs file model split |
| `TProp` | Scenery object | `EditorScenery` + `PmsProp` | PORTED | |
| `TMapSceneryEntry` | Scenery name (51-byte PB string) | `PmsSceneryName` | PORTED | |
| `TCollider` | Circle collider | `EditorCollider` + `PmsCollider` | PORTED | |
| `TSaveSpawnPoint` | Spawn point | `EditorSpawn` + `PmsSpawnPoint` | PORTED | |
| `TNewWaypoint` | Waypoint with connections | `EditorWaypoint` + `PmsWaypoint` | PORTED | |
| `TConnection` | Waypoint connection (2×int16) | Part of `EditorWaypoint::connections` | PORTED | |
| `TMapOptions` | Map metadata + settings | `PmsOptions` + `MapDocument::options` | PORTED | |
| `TLightSource` | Point light (PolyWorks extension) | `EditorLight` + `PmsLight` | PORTED | |
| `TSketchLine` | Sketch line | `EditorSketchLine` + `PmsSketchLine` | PORTED | |
| `TSectorTable` | Spatial index (51×51 grid) | `SectorCell[51][51]` in `pms_io.cpp` | PORTED | |
| `LoadFile` | Load .pms binary file | `loadPmsFile()` in `pms_io.cpp` | PORTED | |
| `SaveFile` | Save .pms (PolyWorks format) | `savePmsFile()` in `pms_io.cpp` | PORTED | |
| `SaveAndCompile` | Compile .pms (game format) | `compilePms()` in `pms_io.cpp` | PORTED | |
| `SavePrefab` | Save selected entities as .pwf | `savePrefab()` in `pms_io.cpp` | PORTED | |
| `LoadPrefab` | Load .pwf into document | `loadPrefab()` in `pms_io.cpp` | PORTED | |

---

### `modUtils.bas` — Math and color utilities

| Original Symbol | Functionality | C++ Equivalent | Status | Notes |
|---|---|---|---|---|
| `ARGB()` | Construct ARGB LongWord | Inline in map_document.cpp | PORTED | |
| `GetAlpha/Red/Green/Blue` | Color channel extraction | Inline bit ops throughout | PORTED | |
| `RGBToLong` | Pack RGB to LongWord | Inline | PORTED | |
| `GetRGB` | Parse hex string to color | Not needed at runtime | OBSOLETE | Only used for VB6 designer colors |
| `Midpoint` | Linear midpoint | `(a+b)*0.5f` inline | PORTED | |
| `GetAngle` | Angle from point pair | `atan2f` inline | PORTED | |
| `Clamp` | Clamp value to range | `std::clamp` | PORTED | |
| `IsBetween` | Range check | Inline comparison | PORTED | |

---

### `modGlobals.bas` — Global state variables

| Original Symbol | Functionality | C++ Equivalent | Status | Notes |
|---|---|---|---|---|
| `PolyCount`, `Polys()` | Polygon array | `MapDocument::polys` | PORTED | |
| `ScenCount`, `Scen()` | Scenery object array | `MapDocument::scenery` | PORTED | |
| `SceneryNames()` | Texture name list | `MapDocument::sceneryNames` | PORTED | |
| `SpawnCount`, `Spawns()` | Spawn point array | `MapDocument::spawns` | PORTED | |
| `ColliderCount`, `Colliders()` | Collider array | `MapDocument::colliders` | PORTED | |
| `WaypointCount`, `Waypoints()` | Waypoint array | `MapDocument::waypoints` | PORTED | |
| `ConnectionCount`, `Connections()` | Waypoint connections | `EditorWaypoint::connections` (inline) | PORTED | |
| `LightCount`, `Lights()` | Light source array | `MapDocument::lights` | PORTED | |
| `SketchCount`, `SketchLines()` | Sketch lines | `MapDocument::sketch` | PORTED | |
| `SelCount`, `SelList()` | Selected polygon indices | `EditorPoly::v[i].selected` flags | PORTED | Per-vertex selection |
| `NumSelScenery` etc. | Per-type selection counts | Computed on-demand in `MapDocument` | PORTED | |
| `currentUndo/numUndo/numRedo` | Undo ring state | `UndoStack` class | PORTED | |
| `selectionChanged` | Dirty flag for undo push | `UndoStack::pop()` / manual push | PORTED | |
| `gScrollX/Y` | Viewport scroll | `MapDocument::scrollX/Y` | PORTED | |
| `gZoom` | Viewport zoom | `MapDocument::zoom` | PORTED | |
| `gMinZoom` | Minimum zoom | `PMS_ZOOM_MIN` constant | PORTED | |
| `currentFunction` | Active tool index | `MainFrame::m_activeTool` | PORTED | |
| `gSnapRadius` | Hit-test radius | `GL_VIEWPORT_SNAP_RADIUS` in gl_viewport.cpp | PORTED | |
| `ohSnap` | Vertex snap mode | `ViewSettings::snapToVertices` | PORTED | |
| `grid/snapToGrid` | Grid snap mode | `ViewSettings::snapToGrid` | PORTED | |
| `gridInc` | Grid increment | `ViewSettings::gridSize` | PORTED | |
| `fixedTexture` | Freeze UV on snap | `ViewSettings::fixedTexture` | PORTED | |
| `blendWireframe/blendPolys` | Alpha blend modes | `ViewSettings::blendWireframe/blendPolys` | PORTED | |
| `appPath` | Application data path | `wxStandardPaths` in main.cpp | PORTED | |
| `SoldatPath` | Soldat installation path | `PreferencesDlg` + `wxConfig` | PORTED | |

---

### `modConfig.bas` — Preferences and INI file

| Original Symbol | Functionality | C++ Equivalent | Status | Notes |
|---|---|---|---|---|
| `LoadSettings` | Load INI / registry config | `PreferencesDlg` + `wxConfig` | PORTED | |
| `SaveSettings` | Save INI / registry config | `PreferencesDlg` + `wxConfig` | PORTED | |
| `DetectSoldatPath` | Auto-detect Soldat install | Windows registry path in prefs | PARTIALLY PORTED | On non-Windows, path is manual only |
| Palette load/save | `appPath\palettes\current.txt` | `PalettePanel` reads/writes .pal | PORTED | |
| Recent files list | MRU file list | `wxFileHistory` in MainFrame | PARTIALLY PORTED | History not yet persisted between sessions |

---

### `modRender.bas` — DirectX rendering

| Original Symbol | Functionality | C++ Equivalent | Status | Notes |
|---|---|---|---|---|
| `InitDX8` | Initialize DirectX 8 | `GlViewport` OpenGL context | PORTED | OpenGL replaces DX8 |
| `LoadTexture` | Load BMP/texture via D3DX | `TextureManager` in gl_viewport.cpp | PORTED | Uses stb_image |
| `scenerySprite` | D3DXSprite scenery draw | OpenGL quads with transforms | PORTED | |
| `Render()` | Full frame render | `Renderer::render()` | PORTED | |
| Polygon fill | Textured D3D triangles | OpenGL immediate mode triangles | PORTED | |
| Wireframe overlay | `D3DPT_LINESTRIP` | OpenGL `GL_LINE_LOOP` | PORTED | |
| Grid overlay | Screen-space lines | `renderGrid()` in renderer.cpp | PORTED | |
| Background gradient | Two-color quad | `renderBackground()` in renderer.cpp | PORTED | |
| Spawn/waypoint/light icons | D3DX texture sprites | `renderObjects()` in renderer.cpp | PORTED | |
| Color key transparency | `D3DCOLOR_COLORVALUE` | stb_image + manual keying | PORTED | Green (#00FF00) → alpha=0 |
| Pre-transformed coords | `D3DFVF_XYZRHW` | Ortho projection in renderer | PORTED | |

---

### `modInput.bas` — Keyboard / mouse input

| Original Symbol | Functionality | C++ Equivalent | Status | Notes |
|---|---|---|---|---|
| DirectInput keyboard | DI8 key buffer | `wxEVT_CHAR_HOOK` in MainFrame | PORTED | Standard wx events replace DI8 |
| Middle-button pan | Pan with middle drag | `GlViewport::OnMiddleDrag` | PORTED | |
| Mouse wheel zoom | `ZoomScroll 1.25 / 0.8` anchored per direction | `GlViewport::OnMouseWheel` -> `MapDocument::zoomScroll()` | PORTED | Sub-notch rotations are accumulated for HiDPI trackpads |
| Screen→world coords | `gScrollX/Y`, `gZoom` math | `MapDocument::screenToWorld()` | PORTED | |
| World→screen coords | Same inverse | `MapDocument::worldToScreen()` | PORTED | |

---

### `modOSME.bas` — Application-level utilities

| Original Symbol | Functionality | C++ Equivalent | Status | Notes |
|---|---|---|---|---|
| `RunSoldat` | Launch Soldat.exe | `MainFrame::OnFileRunSoldat` | PORTED | |
| `RunOpenSoldat` | Launch OpenSoldat | `MainFrame::OnFileRunSoldat` | PORTED | |
| Asset path detection | Relative to PMS dir | `addTexturePath()` in OnFileOpen | PORTED | |
| `appPath` detection | Win32 `GetModuleFileName` | `wxStandardPaths::GetExecutablePath()` | PORTED | |

---

### `frmOpenSoldatMapEditor.frm` — Main editor form (~14,600 lines)

#### File menu handlers

| Original Symbol | Functionality | C++ Equivalent | Status |
|---|---|---|---|
| `mnuNew_Click` | New map | `OnFileNew` | PORTED |
| `mnuOpen_Click` | Open .pms | `OnFileOpen` | PORTED |
| `mnuOpenCompiled_Click` | Open compiled .pms | `OnFileOpenCompiled` | PORTED |
| `mnuSave_Click` | Save (PolyWorks format) | `OnFileSave` | PORTED |
| `mnuSaveAs_Click` | Save As | `OnFileSaveAs` | PORTED |
| `mnuSaveAndCompile_Click` | Compile to game format | `OnFileCompile` | PORTED |
| `mnuCompileAs_Click` | Compile As | `OnFileCompileAs` | PORTED |
| `mnuExport_Click` | Export prefab (.pwf) | `OnFileExport` | PORTED |
| `mnuImport_Click` | Import prefab (.pwf) | `OnFileImport` | PORTED |
| `mnuRunOpenSoldat_Click` | Launch OpenSoldat | `OnFileRunSoldat` | PORTED |
| `mnuRunSoldat_Click` | Launch Soldat | `OnFileRunSoldat` | PORTED |
| Recent file list | MRU list | `wxFileHistory` | PARTIALLY PORTED |
| `mnuExit_Click` | Exit | `OnExit` | PORTED |

#### Edit menu handlers

| Original Symbol | Functionality | C++ Equivalent | Status |
|---|---|---|---|
| `mnuUndo_Click` | Undo | `OnEditUndo` | PORTED |
| `mnuRedo_Click` | Redo | `OnEditRedo` | PORTED |
| `mnuDuplicate_Click` | Duplicate selected | `OnEditDuplicateSelected` | PORTED |
| `mnuCopy_Click` / `mnuCut_Click` / `mnuPaste_Click` | Copy/Cut/Paste | `OnEditCopyPasteCut` | PORTED |
| `mnuClear_Click` | Delete selected | `OnEditDeleteSelected` | PORTED |
| `mnuSelectAll_Click` | Select all | `OnEditSelectAll` | PORTED |
| `mnuDeselect_Click` | Deselect [Escape] | `ID_EDIT_DESELECT` lambda | PORTED |
| `mnuInvertSelection_Click` | Invert selection | `OnEditInvertSelection` | PORTED |
| `mnuSelectByColor_Click` | Select by color | `OnEditSelectByColor` | PORTED |
| `mnuRotate180_Click` | Rotate 180° | `OnEditTransform (ID_EDIT_TRANSFORM_ROTATE_180)` | PORTED |
| `mnuRotate90CW_Click` | Rotate 90° CW | `OnEditTransform (ID_EDIT_TRANSFORM_ROTATE_90CW)` | PORTED |
| `mnuRotate90CCW_Click` | Rotate 90° CCW | `OnEditTransform (ID_EDIT_TRANSFORM_ROTATE_90CCW)` | PORTED |
| `mnuFlipH_Click` | Flip horizontal | `OnEditTransform (ID_EDIT_TRANSFORM_FLIP_H)` | PORTED |
| `mnuFlipV_Click` | Flip vertical | `OnEditTransform (ID_EDIT_TRANSFORM_FLIP_V)` | PORTED |
| `mnuSever_Click` | Sever connections [Backspace] | `OnEditSeverConnections` | PORTED |
| `mnuClearSketch_Click` | Clear sketch lines | `OnEditClearSketch` | PORTED |

#### View menu handlers

| Original Symbol | Functionality | C++ Equivalent | Status |
|---|---|---|---|
| Polygon/Wireframe/Points visibility | Layer toggles | `OnViewLayerToggle (ID_VIEW_POLYGONS etc.)` | PORTED |
| Grid toggle | Grid on/off | `OnViewLayerToggle (ID_VIEW_GRID)` | PORTED |
| Objects/Waypoints/Lights/Sketch | Layer toggles | `OnViewLayerToggle` | PORTED |
| Textures/Background visibility | Layer toggles | `OnViewLayerToggle` | PORTED |
| Scenery Back/Middle/Front | Scenery layer toggles | `OnViewLayerToggle` | PORTED |
| Blend Wireframe / Blend Polys | Alpha blend modes | `OnViewLayerToggle (ID_VIEW_BLEND_*)` | PORTED |
| Snap to Grid / Snap to Vertices | Snap toggles | `OnViewLayerToggle (ID_VIEW_SNAP_*)` | PORTED |
| Zoom In / Zoom Out [Ctrl+/−] | Zoom level | `OnViewZoom` | PORTED |
| Zoom 100% | Reset zoom | `OnViewZoom (ID_VIEW_ZOOM_RESET)` | PORTED |
| Center and Reset [Ctrl+0] | Reset scroll+zoom | `OnViewZoom (ID_VIEW_CENTER_RESET)` | PORTED |
| Fit on Screen | Fit map in viewport | `OnViewFitOnScreen` | PORTED |
| Color Palette toggle | Show/hide palette panel | `ID_VIEW_PALETTE` | PORTED |
| Refresh [F5] | Force repaint | `wxID_REFRESH` | PORTED |

#### Map menu

| Original Symbol | Functionality | C++ Equivalent | Status |
|---|---|---|---|
| `mnuMapSettings_Click` | Map properties dialog | `OnMapSettings` | PORTED |
| `mnuPreferences_Click` | Preferences dialog | `OnPreferences` | PORTED |

#### Polygon menu

| Original Symbol | Functionality | C++ Equivalent | Status |
|---|---|---|---|
| Poly type submenu | Set polygon type (26 types) | `ID_POLY_TYPE_BASE + i` | PORTED |
| `mnuSplitAtVertex_Click` | Split polygon at selected vertex | `OnPolyOperation (ID_POLY_SPLIT_AT_VERTEX)` | PORTED |
| `mnuJoinVertices_Click` | Join two selected vertices | `OnPolyOperation (ID_POLY_JOIN_VERTICES)` | PORTED |
| `mnuCreateWithSelected_Click` | Create poly from selected verts | `OnPolyOperation (ID_POLY_CREATE_WITH_SELECTED)` | PORTED |
| `mnuFixTexture_Click` | Re-map UV to cover polygon | `OnPolyOperation (ID_POLY_FIX_TEXTURE)` | PORTED |
| `mnuUntexture_Click` | Remove UV mapping | `OnPolyOperation (ID_POLY_UNTEXTURE)` | PORTED |
| `mnuAverageColors_Click` | Average vertex colors | `OnPolyOperation (ID_POLY_AVERAGE_COLORS)` | PORTED |
| `mnuFixedTexture_Click` | Toggle fixed-texture mode | `ID_POLY_FIXED_TEXTURE` | PORTED |
| `mnuApplyLight_Click` | Bake lights into vertex colors | `OnPolyApplyLight` | PORTED |
| `mnuFlipTexture_Click` (H/V) | Flip UV horizontal/vertical | `OnPolyTexTransform` | PORTED |
| `mnuRotateTexture_Click` (180/90CW/90CCW) | Rotate UV coordinates | `OnPolyTexTransform` | PORTED |

#### Arrange menu

| Original Symbol | Functionality | C++ Equivalent | Status |
|---|---|---|---|
| `mnuBringToFront_Click` | Move poly to top of Z-order | `OnArrangeSelected (ID_ARRANGE_BRING_TO_FRONT)` | PORTED |
| `mnuSendToBack_Click` | Move poly to bottom | `OnArrangeSelected (ID_ARRANGE_SEND_TO_BACK)` | PORTED |
| `mnuBringForward_Click` | Move poly one step forward | `OnArrangeSelected (ID_ARRANGE_BRING_FORWARD)` | PORTED |
| `mnuSendBackward_Click` | Move poly one step back | `OnArrangeSelected (ID_ARRANGE_SEND_BACKWARD)` | PORTED |

#### Window menu

| Original Symbol | Functionality | C++ Equivalent | Status |
|---|---|---|---|
| Show All / Hide All | Toggle all panels | `OnWindowShowAll/HideAll` | PORTED |
| Load/Save Workspace | Panel position INI | `OnWindowLoadWorkspace/SaveWorkspace` | PORTED |
| Reset Window Locations | Reset panel positions | `OnWindowResetLayout` | PORTED |
| Panel toggles (7 panels) | Show/hide each panel | `OnWindowTogglePanel` | PORTED |

#### Tool keyboard shortcuts

| Original Shortcut | Tool | C++ Equivalent | Status |
|---|---|---|---|
| A | Move (pan) | `SetActiveTool(0)` | PORTED |
| Q | Vertex Select | `SetActiveTool(1)` | PORTED |
| S | Polygon | `SetActiveTool(2)` | PORTED |
| W | Scenery | `SetActiveTool(3)` | PORTED |
| D | Spawns | `SetActiveTool(4)` | PORTED |
| E | Waypoints | `SetActiveTool(5)` | PORTED |
| F | Connections | `SetActiveTool(6)` | PORTED |
| R | Texture | `SetActiveTool(7)` | PORTED |
| G | Color | `SetActiveTool(8)` | PORTED |
| T | Sketch | `SetActiveTool(9)` | PORTED |
| H | Collider | `SetActiveTool(10)` | PORTED |
| Z | Light | `SetActiveTool(11)` | PORTED |
| J | Depth | `SetActiveTool(12)` | PORTED |
| U | Depth Map | `SetActiveTool(13)` | PORTED |
| Del | Delete selected | `OnKeyDown WXK_DELETE` | PORTED |
| Backspace | Sever connections | `OnKeyDown WXK_BACK` | PORTED |
| Arrow keys | Nudge selected ±1 | `OnKeyDown WXK_LEFT etc.` | PORTED |
| Shift+Arrow | Nudge selected ±10 | `OnKeyDown` | PORTED |
| Escape | Deselect all | `ID_EDIT_DESELECT` | PORTED |
| Ctrl+Z / Ctrl+Y | Undo / Redo | Standard wx | PORTED |
| Ctrl+A | Select All | Standard wx | PORTED |
| Ctrl+S / Ctrl+Shift+S | Save / Save As | Standard wx | PORTED |
| Ctrl+N / Ctrl+O | New / Open | Standard wx | PORTED |
| F9 | Compile | `ID_FILE_COMPILE` | PORTED |
| F5 | Refresh | `wxID_REFRESH` | PORTED |
| Ctrl+M | Map Settings | `ID_MAP_SETTINGS` | PORTED |
| Ctrl+P | Preferences | `ID_MAP_PREFERENCES` | PORTED |
| Ctrl+' | Grid toggle | `ID_VIEW_GRID` | PORTED |
| Ctrl++ / Ctrl+- | Zoom In/Out | `ID_VIEW_ZOOM_IN/OUT` | PORTED |
| * | Zoom 100% | `ID_VIEW_ZOOM_RESET` | PORTED |
| Ctrl+0 | Center and Reset | `ID_VIEW_CENTER_RESET` | PORTED |
| Home/End/PgUp/PgDn | Arrange | `ID_ARRANGE_*` | PORTED |

#### Mouse interaction (picViewport events)

| Original Event | Functionality | C++ Equivalent | Status |
|---|---|---|---|
| `picViewport_MouseDown` | Tool dispatch, selection start | `GlViewport::OnMouseDown` | PORTED |
| `picViewport_MouseMove` | Drag, cursor update, snap | `GlViewport::OnMouseMove` | PORTED |
| `picViewport_MouseUp` | End drag, commit undo | `GlViewport::OnMouseUp` | PORTED |
| `picViewport_MouseWheel` | Zoom x1.25 / x0.8 | `GlViewport::OnMouseWheel` | PORTED |
| Middle-button drag | Pan | `GlViewport::OnMouseMove (middle)` | PORTED |
| Rubber-band selection | Marquee select | `GlViewport` selection rect | PORTED |
| Ctrl+click | Add/remove from selection | Modifier flag in mouse handlers | PORTED |
| Vertex drag | Move selected vertices | `GlViewport` drag logic | PORTED |
| Cross-polygon vertex selection | Multi-poly vertex select | `EditorVertex::selected` per vertex | PORTED |
| Cursor change by tool | Tool cursor indicator | Cursor set in `GlViewport::setActiveTool` | PORTED |

#### Render pipeline

| Original Function | Functionality | C++ Equivalent | Status |
|---|---|---|---|
| `Render()` main loop | Full frame | `Renderer::render()` | PORTED |
| Background gradient | `D3DPT_TRIANGLESTRIP` with two colors | `renderBackground()` | PORTED |
| Textured polygons | DX8 indexed triangles | OpenGL textured quads | PORTED |
| Wireframe overlay | `D3DPT_LINESTRIP` | `GL_LINE_LOOP` | PORTED |
| Vertex points | `D3DPT_POINTLIST` | `GL_POINTS` | PORTED |
| Scenery (3 levels) | `D3DXSprite` | OpenGL quads + transforms | PORTED |
| Spawn/collider icons | Overlay sprites | Colored circles/rectangles | PORTED |
| Waypoint lines | Line segments | `GL_LINES` | PORTED |
| Light visualization | Circle + rays | Colored circle outline | PORTED |
| Sketch lines | Line segments | `GL_LINES` | PORTED |
| Grid overlay | Screen-space lines | `renderGrid()` | PORTED |
| Selection highlighting | Colored vertex dots | Highlighted vertex rendering | PORTED |
| Selection rectangle | XOR rect / outline | `GL_LINE_LOOP` overlay | PORTED |
| ApplyLights (preview) | Dot-product lighting | `MapDocument::applyLightsToBaseColors()` | PORTED |

---

### `frmColor.frm` — Color picker dialog

| Original Symbol | Functionality | C++ Equivalent | Status | Notes |
|---|---|---|---|---|
| HSV sliders | Hue/Saturation/Value | `PalettePanel` with RGB+HSV | PORTED | |
| RGB sliders | Direct RGB input | `PalettePanel` | PORTED | |
| Alpha channel | Transparency control | `PalettePanel` opacity | PORTED | |
| Palette grid | Preset color swatches | `PalettePanel` color grid | PORTED | |
| Color preview | Current vs original | `PalettePanel` preview swatch | PORTED | |
| Apply to selection | Paint vertices | `GlViewport` color tool | PORTED | |

---

### `frmDisplay.frm` — Display settings panel

| Original Symbol | Functionality | C++ Equivalent | Status |
|---|---|---|---|
| Layer checkboxes | 13 visibility toggles | `DisplayPanel` checkboxes | PORTED |
| Apply button | Trigger repaint | Automatic on checkbox change | PORTED |

---

### `frmInfo.frm` — Selection info panel

| Original Symbol | Functionality | C++ Equivalent | Status |
|---|---|---|---|
| Polygon count / selected count | Statistics | `InfoPanel` labels | PORTED |
| Position display | Selected object coordinates | `InfoPanel` world coords | PORTED |
| Color display | Selected vertex color | `InfoPanel` color swatch | PORTED |
| Poly type display | Current polygon type | `InfoPanel` type label | PORTED |

---

### `frmMap.frm` — Map properties dialog

| Original Symbol | Functionality | C++ Equivalent | Status |
|---|---|---|---|
| Map name | `TMapOptions.mapName` | `MapSettingsDlg` name field | PORTED |
| Texture name | `TMapOptions.textureName` | `MapSettingsDlg` texture field + browse | PORTED |
| Background colors | `bgColor1/2` | `MapSettingsDlg` color pickers | PORTED |
| Jet, Grenades, Medkits | Numeric fields | `MapSettingsDlg` spinners | PORTED |
| Weather | Dropdown | `MapSettingsDlg` choice | PORTED |
| Steps | Dropdown | `MapSettingsDlg` choice | PORTED |

---

### `frmPalette.frm` — Color palette panel

Fully replaced by `PalettePanel` (PORTED). See frmColor notes.

---

### `frmPreferences.frm` — Preferences dialog

| Original Symbol | Functionality | C++ Equivalent | Status |
|---|---|---|---|
| Undo depth | Max undo steps | `PreferencesDlg` spinner | PORTED |
| Snap radius | Hit-test pixel radius | `PreferencesDlg` spinner | PORTED |
| Grid size | Grid increment | `PreferencesDlg` spinner | PORTED |
| Soldat path | Installation directory | `PreferencesDlg` path browse | PORTED |
| Key bindings | Custom hotkeys | Not yet configurable | NOT PORTED |
| Per-polytype colors | 26 customizable type colors | Not yet in UI | NOT PORTED |

---

### `frmScenery.frm` — Scenery panel

| Original Symbol | Functionality | C++ Equivalent | Status |
|---|---|---|---|
| Scenery texture list | Browse and select textures | `SceneryPanel` list | PORTED |
| Level selector (Back/Mid/Front) | Depth layer | `SceneryPanel` radio buttons | PORTED |
| Rotate flag | Enable free rotation | `SceneryPanel` checkbox | PORTED |
| Scale flag | Enable free scaling | `SceneryPanel` checkbox | PORTED |
| Preview thumbnail | Texture preview | `SceneryPanel` bitmap preview | PORTED |

---

### `frmTaskBar.frm` — Floating task bar (tool palette)

Replaced by `ToolsPanel` (PORTED). All 14 tool buttons implemented.

---

### `frmTexture.frm` — Texture UV offset editor

| Original Symbol | Functionality | C++ Equivalent | Status |
|---|---|---|---|
| UV offset sliders | Fine-tune UV per polygon | `TexturePanel` (basic implementation) | PARTIALLY PORTED |
| Real-time preview | Update viewport while dragging | Works via RefreshViewport | PORTED |
| Reset button | Reset UV to default | Not yet in TexturePanel | PARTIALLY PORTED |

---

### `frmTools.frm` — Tool palette panel

Replaced by `ToolsPanel` (PORTED). See frmTaskBar notes.

---

### `frmWaypoints.frm` — Waypoints panel

| Original Symbol | Functionality | C++ Equivalent | Status |
|---|---|---|---|
| Movement flags (L/R/U/D/M2) | Waypoint direction flags | `WaypointPanel` checkboxes | PORTED |
| Path number | Path group assignment | `WaypointPanel` spinner | PORTED |
| Special action | Special waypoint type | `WaypointPanel` dropdown | PORTED |
| Path filter | Show only one path | `WaypointPanel` filter | PORTED |
| Apply to selected | Write flags to selection | `WaypointPanel` apply button | PORTED |

---

## Verified Functional Gaps (Remaining)

### NOT PORTED

1. **Key binding configuration** (`frmPreferences`: custom hotkeys)
   - Original allows remapping tool shortcuts and menu accelerators
   - C++ uses fixed shortcuts; no UI for customization
   - Evidence: `frmPreferences.frm` has a key-binding list box

2. **Per-polytype color customization** (`frmPreferences`: 26 type colors)
   - Original allows custom colors per polygon type (affects wireframe and point rendering)
   - C++ hardcodes type colors in `renderer.cpp`
   - Evidence: `modGlobals.bas` `PolyTypeColors()` array + prefs dialog

3. **Texture Panel reset button** (`frmTexture.frm`)
   - Reset UV to normalized coverage is not wired in `TexturePanel`
   - Evidence: `frmTexture.frm` has a Reset button that calls `FixTexture`

### PARTIALLY PORTED

4. **Recent files list** — list is shown but not persisted between sessions (`wxFileHistory` is not saved to config)

5. **Detect Soldat path on Windows** — on Windows, VB6 reads the registry; C++ only does manual path entry

6. **Texture UV offset editor** (`frmTexture`) — basic panel exists but Reset and fine-grained controls incomplete

7. **Workspace load/save** — positions saved/loaded but panel visibility states are not persisted

8. **Key bindings display** in tools panel — VB6 showed active shortcut key in tool button tooltip; C++ tooltips are static

---

## Obsolete / Not Applicable

| Feature | Reason |
|---|---|
| DirectX 8 initialization (`InitDX8`) | Replaced by OpenGL |
| `D3DXSprite` for scenery | Replaced by OpenGL quads |
| `MBMouse.ocx` borderless chrome | Standard wxFrame used instead |
| `COMDLG32.OCX` file dialogs | wxFileDialog used |
| VB6 `GetPrivateProfileString` registry | wxConfig cross-platform INI |
| VB6 `Boolean` = 2 bytes in UDT | Handled in `PmsProp.active` int16_t |
| Win32 `GetModuleFileName` | wxStandardPaths |
| DirectInput 8 keyboard buffer | wxEVT_CHAR_HOOK |
| VB6 `Form_Resize` event | wxEVT_SIZE |
| VB6 `PictureBox_Paint` event | wxGLCanvas OnPaint |
| VB6 `App.Path` | wxStandardPaths::GetExecutablePath() |
| Custom borderless window chrome | Not applicable on modern OS |
| Installer OCX registration | Not needed |
| VB6 `DoEvents` / message pump | wxApp main loop |

---

# Forensic Audit — Independent Re-verification

This section records a second, independent audit that treated the existing C++
implementation as **untrusted** and re-derived expected behaviour from the
original VB6 sources, the original resources, `installer/PolyWorks Help.html`,
and the 97 real Soldat maps in `maps/`.

Claims in the sections above were **not** taken as evidence. Where this audit
contradicts them, this section is authoritative.

Method: behaviour was verified empirically wherever possible — parsing the real
shipped maps to derive the true on-disk conventions, and running the GUI under
Xvfb against real maps to confirm rendering and asset resolution, rather than
relying on successful compilation.

## Findings that were previously mis-recorded as PORTED

| # | Original element | Original behaviour (evidence) | C++ before | Status | Fix |
|---|---|---|---|---|---|
| 1 | Edge normals, `SaveAndCompile` (`frmOpenSoldatMapEditor.frm:2642`) | `n = ((v[i].y - v[j].y), (v[j].x - v[i].x)) / len` — verified against **6483/6483** sampled edges in real maps | Negated sign convention | **INCORRECT** | `computePolyNormals()` rewritten; pinned by `compile_normal_orientation` |
| 2 | `Perp.vertex(j).Z` | Always `1` on disk in every real map; **bounciness lives in the normal's magnitude**, recovered on load via `Z = Sqr(X²+Y²)` (`frm:1988`) | Wrote bounciness into `perp.z`; `EditorPoly` had no bounciness field at all, so `docToPmsData` zeroed it | **SILENT DATA LOSS** | `EditorPoly::bounciness[3]` added; save/load bake and recover it; `bounciness_roundtrip` test |
| 3 | `sectorsDivision` | `int((max(halfWidth, halfHeight) + 100) / 25)` — matched exactly on **12/12** maps | Never recomputed | **INCORRECT** | Derived in `compilePms()` / `docToPmsData()` |
| 4 | Sector table, `IsInSector` (`frm:5684`) | Per-cell polygon overlap test; `polyType == 3` (NoCollide) never appears in any real sector table; 256 entries/cell cap | Permissive bbox-overlap test, included NoCollide, no cap | **INCORRECT** | Faithful `isInSector()`/`pointInPoly()`/`isBetween()` ports |
| 5 | `mapRandomID` | `Rnd * 999999 + 10000`; real maps span 80110–964402 | Hard-coded `1` | **INCORRECT** | `std::mt19937` in `[10000, 1009999]` |
| 6 | Texture colour key (`modGlobals.bas:12` `COLOR_KEY = &HFF00FF00`) | **Every** texture load colour-keys opaque pure green to transparent (`modRender.bas:149,163`; `frm:2083,4287`). Soldat BMPs have no alpha and depend on it | No colour keying at all | **NOT PORTED** | `src-cpp/core/color_key.{h,cpp}`; `color_key_*` tests |
| 7 | Texture addressing | VB6 never sets `D3DTSS_ADDRESSU/V`, so D3D's default `D3DTADDRESS_WRAP` (tiling) applies. **99% of vertices in the shipped maps have UVs outside [0,1]** (measured range ±12.75) | `GL_CLAMP_TO_EDGE` | **INCORRECT** | `GL_REPEAT` |
| 8 | Skins directory resolution | `appPath & "\skins\" & gfxDir` — source of `notfound.bmp`, all custom cursors and skin bitmaps | `wxFileName(dir, "skins/default")` misuses the *(path, filename)* ctor (asserts on separators) and resolved to `<cwd>/default`. `getSkinsPath()` returned **empty**, disabling the entire skins system | **BROKEN** | Correct `wxFileName::DirName` + `AppendDir` traversal, plus installed-layout and build-dir candidates and a warning on failure |
| 9 | Texture/scenery search paths | `OpenSoldatDir & "textures\"`, `& "Scenery-gfx\"` (`frm:4284`, `frm:2092`); maps live in `<soldat>/Maps/` | `wxFileName(pmsDir, wxEmptyString).GetPath()` returns `pmsDir`, **not** its parent, so the `<soldat>/Textures` fallback never worked for any real map | **BROKEN** | Single `RegisterAssetPathsForMap()` helper with correct parent traversal |
| 10 | View reset on load (`frm:1935-1939`) | `zoomFactor = 1`, `scroll = (-ScaleWidth/2, -ScaleHeight/2)` — puts world origin at the viewport centre, framing the map | No view reset; map appeared off the bottom-right corner | **NOT PORTED** | Reset applied *after* `pmsDataToDoc` (which calls `clear()` and would otherwise zero it) |
| 11 | Command-line map open (`frm:10648-10670`) | Opens a `.pms` named on the command line, resolving it as given, then `<appPath>/Maps/`, then `<OpenSoldatDir>/Maps/`. This is how the `.pms` file association works (`installer/pw.nsi:185`) | Command line ignored entirely | **NOT PORTED** | `MainFrame::OpenCommandLineMap()` |
| 12 | Selected-polygon highlight (`frm:3082-3116`) | Additive fill tinted by `gPolyTypeColors(polyType)` (26-entry table, `modConfig.bas:211-235`); index 0 is the user's selection colour (default `CE4D4A`). The highlight colour identifies the polygon type | Fixed yellow outline only | **PARTIALLY PORTED** | `polyTypeColor()` + additive per-vertex-selected fill; `poly_type_colors_match_original_defaults` test |
| 13 | `frmInfo.frm` | A six-page property **editor** (`picProp(0..5)`, switched by `mnuProp_Click` and auto-selected by `GetInfo`, `frm:4668`) covering light, map, scenery, quad, transform and polygon properties | A read-only stats readout — 1 of 6 pages, no editing | **PARTIALLY PORTED** | Full six-page editor with VB6 value formatting, `noChange` re-entrancy guard, per-type auto page selection, and undo integration |
| 14 | `frmMap.frm cboSteps` | Steps combo, list `Hard / Soft / None` (`frmMap.frx:0x015E`), bound to `Options.Steps` (`frm:4354`, `frm:4372`) | Absent from Map Settings; `steps` persisted but unreachable | **NOT PORTED** | Added to `MapSettingsDlg` |
| 15 | `TOOL_SCALE` / `TOOL_ROTATE` | Interactive Ctrl-drag scale and Alt-drag rotate about the selection centre (`Scaling:7170`, `Rotating:7355`, `ApplyTransform:7245`) | `ComputeCurrentFunction` produced the tools but `HandleLeftDownEdit` had **no case** for them — silent no-ops | **NOT PORTED** | `Transforming` viewport state, `MapDocument::beginTransform`/`applyTransform`, Shift→15° quantisation |
| 16 | Snapping (`SnapSelected:8246`) | Snap-to-grid and snap-to-vertex applied on mouse-up | `viewSettings.snapToGrid` / `snapToVertices` were written by the menu handlers and **never read anywhere** — dead toggles | **NOT PORTED** | `snapSelected()` implemented and called on drag release |
| 17 | Preferences | `modConfig.bas LoadConfig`/`SaveConfig` persist settings across runs | `OnPreferences` constructed a throwaway `AppPrefs` each time — preferences were never retained or persisted at all | **BROKEN** | `LoadPrefs`/`SavePrefs`/`ApplyPrefs` via `wxConfig`, loaded at startup |
| 18 | Arrow-key nudge (`frm:11007-11010`) | 1 world unit; Shift = `gridSpacing / gridDivisions` | Two competing handlers (viewport and mainframe) with different step sizes and undo behaviour; which one ran depended on focus | **INCORRECT** | Single `MainFrame::NudgeSelection`; viewport handler removed |
| 19 | `mnuDuplicate` (`frm:13147`) | Offsets **+32 in X only** | `(10, 10)` | **INCORRECT** | `(32, 0)` |

## Notes on evidence quality

- The pre-existing `pms_roundtrip_all_maps` test compares **counts only**, not
  bytes. It passed throughout every one of the format bugs above and is much
  weaker evidence than its name suggests.
- A clean compile was explicitly not accepted as evidence. Findings 6–11 were
  all discovered by *running* the GUI under Xvfb against real maps; several are
  invisible to the headless suite.
- Finding 8 was located only after adding asset-resolution diagnostics; the
  failure was silent by design in the original.

## Completion pass — additional findings and fixes

Second pass (after the initial forensic audit).  Same evidence standard: every
row was checked against the original VB6 source before being changed.

| # | Original element | Original behaviour | C++ before | Status | Fix |
|---|---|---|---|---|---|
| 20 | `Polys(i).vertex(j).Z` / `.rhw` | Real persisted fields; `Z` doubles as the depthmap value and (with `rhw`) as a hidden-vertex flag.  `SaveAndCompile` forces `Z = 1`, but the PolyWorks-native save path does not | `pmsDataToDoc` discarded both; `docToPmsData` wrote hard-coded `1.0` | **BROKEN** | Both fields added to `EditorVertex` and round-tripped; `compilePms` still forces `Z = 1` |
| 21 | Render order (`frm:2916-3062`) | background → polygons of type 24/25 **only** → back scenery → all other polygons → … | Back scenery was drawn before *all* polygons | **INCORRECT** | `renderPolygons(doc, tex, backgroundPass)` split into two passes |
| 22 | `objects.bmp` (256×128, 8×4 atlas of 32×32 cells) | Spawns, lights, colliders and the gostek marker are sprites from this atlas; spawn team `T` → cell `(T mod 8, T div 8)` | Drawn as coloured primitives | **PARTIALLY PORTED** | `Renderer::drawObjectSprite` with primitive fallback when the skin is missing |
| 23 | `TabPressed` (`frm:6070`) | Tab rotates the vertex selection inside a single selected polygon, or moves the whole selection to the next polygon / scenery item; Shift reverses | Not implemented; Tab moved keyboard focus | **NOT PORTED** | `MapDocument::cycleSelection`, `wxWANTS_CHARS` on the canvas, 4 regression tests |
| 24 | `mnuCopy_Click` (`frm:11769`) / `mnuPaste_Click` (`frm:12062`) | Copy = `SavePrefab <app>\Temp\copy.PFB`; Paste = `LoadPrefab` of the same file | Menu items existed with **no handler bound** — dead controls | **NOT PORTED** | `OnEditCopy`/`OnEditPaste` using a prefab in the per-user temp directory |
| 25 | `mnuRefreshBG_Click` (`frm:14057`) | The gradient background is a **world-anchored** quad spanning the map bounds padded by 640 units | Drawn as a full-viewport screen-space gradient, so it never ended | **INCORRECT** | `Renderer::renderBackgroundQuad` recomputes the quad in world space every frame |
| 26 | Grid (`frm:5990-6037`) | Major lines at `gridSpacing` in colour 1 / opacity 1; `gridDivisions - 1` minor lines between them in colour 2 / opacity 2 | Single hard-coded grey grid; `gridDivisions`, `gridColor1/2` and `gridAlpha1/2` were stored in prefs but **never read by the renderer** | **PARTIALLY PORTED** | Two-pass `renderGrid`; defaults aligned with `modConfig.bas:68-73` (spacing 32, 4 divisions, black, 255/51 alpha) |
| 27 | Blending (`frmPreferences cboPolySrc/Dest`, `cboWireSrc/Dest`) | User-selectable D3D blend factors from ZERO/ONE/SRCCOLOR/INVSRCCOLOR/DESTCOLOR/INVDESTCOLOR/SRCALPHA/INVSRCALPHA (`frmPreferences.frx:0x172`) | `viewSettings.blendPolys` / `blendWireframe` were toggled by the View menu and **never read by the renderer** | **NOT PORTED** | Blend-factor indices added to `ViewSettings` and `AppPrefs`, a Blending tab added to Preferences, and both consumed by `renderPolygons` |
| 28 | Preferences `txtUncomp`, `txtOpacity1/2` | Uncompiled-map directory and the two grid opacities | `uncompDir` existed in `AppPrefs` with no UI; opacities had no UI and were never persisted | **PARTIALLY PORTED** | Fields added to the dialog and to `LoadPrefs`/`SavePrefs` |
| 29 | Waypoint keys (`modConfig.bas:179-183`) and layer keys (`:186-193`) | `J/K/I/M/N` set the waypoint direction flags; numpad `1`-`8` toggle display layers | Neither implemented | **NOT PORTED** | Added to `MainFrame::OnKeyDown`, preserving the original's hotkey-first ordering (so `J` still selects the Lights tool) |
| 30 | Sketch tool hotkey (`modConfig.bas:173`, DIK_Y = 21) | `Y` | `Z` | **INCORRECT** | Corrected to `Y` |
| 31 | Middle-drag pan (`frm:11101`, `Button = 4`) | Hand cursor for the duration of the drag | Cursor unchanged | **PARTIALLY PORTED** | `BeginPan`/`EndPan` set and restore the cursor |
| 32 | `mnuRunSoldat` / `mnuRunOpenSoldat` | Launches the game executable from the configured game directory | Read a `/Soldat/SoldatExe` config key that **no dialog ever wrote** — permanently unreachable | **BROKEN** | Falls back to discovering the executable inside the configured game directory |
| 33 | `GetOrAddSelectedSceneryIndex` | `sceneryNames` is 1-based in the original | 0-based arithmetic on a 1-based array — every scenery placement resolved to the wrong graphic | **BROKEN** | Off-by-one corrected |
| 34 | `frmTexture` | Texture browser panel | The `TexturePanel` class was fully implemented but **never instantiated** | **BROKEN** | Created in `app/main.cpp` and wired to `Window > Texture` |
| 35 | `mnuVisible_Click` (`frm:13728`) | Toggles `(z, rhw)` between `(1, 1)` and `(-1, -10)` | Absent | **OBSOLETE** | Model support added (`toggleSelectedVisibility`) but no menu entry: exhaustive search of the `.frm` menu tree shows **no `mnuVisible` control** — the handler is dead code left behind by a removed menu item |
| 36 | "Use 4 verts for scenery", "Fullscreen always on top" (`frmPreferences.frm:1230-1254`) | — | Absent | **NOT APPLICABLE** | Both are `VB.Label` controls with `Visible = 0`; they were never implemented in the original either |
| 38 | `SaveAndCompile` map extents (`frm:2569-2611`) | `mapWidth` / `mapHeight` are declared `As Integer`, so the `Single` half-extent is **rounded to nearest** on assignment; `xOffset` uses `Int()`, which floors | Used the raw float extent and `trunc()` | **INCORRECT** | `std::nearbyint` / `std::floor`; recompiling `ctf_Lanubya` and `ctf_Voland` now reproduces their shipped `sectorsDivision` |
| 39 | Map bounds seeding (`mnuRefreshBG_Click`, `frm:14065-14068`) | `maxX/maxY/minX/minY` are seeded with `0`, so the origin is always inside the map bounds | Bounds were taken purely from the polygon vertices | **INCORRECT** | Bounds clamped to include the origin in both save paths and in the background quad |
| 40 | `SaveMap` vs `SaveAndCompile` sector division | `SaveMap` uses the **full** extents (`frm:5235`); `SaveAndCompile` uses the **half** extents (`frm:2607-2611`).  The two paths genuinely disagree in the original | Only one formula existed | **PARTIALLY PORTED** | Both reproduced; new `compile_reproduces_shipped_sector_division` test covers 96/97 shipped maps (`DesertWind.pms` stores a value its own geometry cannot produce and was not made by this version) |
| 37 | Help / F1 | — | Absent | **NOT APPLICABLE** | The original has no Help menu and no F1 handler anywhere in the `.frm` menu tree |
| 41 | `glViewport` sizing (HiDPI) | The GL back buffer is allocated in physical device pixels; `GetClientSize()` reports logical points | `glViewport(0, 0, size.x, size.y)` used the **logical** size, so on a macOS Retina display the scene was drawn at half scale into the lower-left quadrant of the canvas (GL's origin is bottom-left) | **BROKEN** | `glViewport` is now scaled by `GetContentScaleFactor()` while `glOrtho` stays in logical points, so no other coordinate math changes.  Reported from a real macOS build |
| 42 | Screen->world at non-100% zoom | Clicks must resolve to the world point that is visually under the cursor | Correct in isolation, but finding 41 meant nothing drawn was where the transform said it was, so selection appeared to ignore the zoom level | **BROKEN** | Fixed by 41; eight new regression tests pin the forward/inverse transform, the screen cache and cursor anchoring |
| 43 | `MouseHelper_MouseWheel` (`frm:12733`) / `ZoomScroll` (`frm:4108`) | One wheel notch multiplies zoom by **1.25** (forward) or **0.8** (backward); the numpad keys are what jump between power-of-two levels | The wheel called `snapZoom`, i.e. a full power-of-two step per event, and every sub-notch macOS trackpad event applied one — a single flick zoomed enormously | **INCORRECT** | `MapDocument::zoomScroll()` reproduces the 1.25/0.8 ratios, the two-stage limit clamp (`frm:4113-4119`) and the original's asymmetric anchoring (cursor when zooming in, viewport centre when zooming out).  `OnMouseWheel` accumulates raw rotation to one `GetWheelDelta()` notch |

| 44 | `prompt` unsaved-changes guard (`frm:4381` `Terminate`, `frm:12752` `mnuNew_Click`, `frm:12772` `mnuOpen_Click`) | A three-way Yes / No / Cancel message box before discarding a modified map; Cancel aborts the operation | **No prompt anywhere** — File > New, File > Open and closing the window all discarded unsaved edits silently | **NOT PORTED** | `MainFrame::ConfirmDiscardChanges()` used by all three paths; `OnExit` now routes through `Close(false)` and a `wxEVT_CLOSE_WINDOW` handler that vetoes on Cancel and otherwise runs the original's `SaveSettings` shutdown work |
| 45 | Palette persistence (`frmPalette.Form_Load` `frm:867`, `SaveSettings` `modConfig.bas:389`, dialog `InitDir` `frm:904/923`) | The colour palette is loaded from `<appPath>\palettes\current.txt` at startup and written back at exit; the Load/Save dialogs open in `<appPath>\palettes` | The palette started empty every run and the dialogs opened in the current working directory | **NOT PORTED** | `PalettePanel::LoadCurrentPalette()` / `SaveCurrentPalette()` plus `PalettesDir()` seeding both dialogs |
| 46 | `appPath = App.Path` (`modConfig.bas:52`) | Every static resource — skins, palettes, lists, help — is looked up relative to the executable, and the game's `Textures` / `Scenery-gfx` folders are found there too in a co-located install | The resolver searched the skin directory, map-relative directories and a configured game directory, but **never the executable's own directory** — so an extracted portable release could not find bundled artwork | **PARTIALLY PORTED** | `MainFrame::AppDir()` and `RegisterAppAssetPaths()` register `<exeDir>/Textures`, `<exeDir>/Scenery-gfx` and `<exeDir>`; `LoadPrefs` also adopts `<exeDir>` as the game directory when it contains either folder.  Six new resolver tests cover ordering, case-insensitivity, deduplication and map-relative precedence |
| 47 | `polyworks.ini` in `appPath` (`modConfig.bas` `LoadConfig`/`SaveSettings`) | All settings live in an ini file beside the executable | Settings went to `wxConfig`, i.e. the Windows registry — a portable release left state behind outside its own folder | **INCORRECT** | `MainFrame::OpenConfig()` returns a `wxFileConfig` on `<appPath>/polyworks.ini` on Windows, or anywhere the file already exists (opt-in portable mode on Linux/macOS, which have their own conventions), falling back to the per-user store when the directory is read-only |
| 48 | Tool palette layout (`frmTools`) | A 7x2 grid of fourteen 32x32 bitmap tool buttons | The frame is given its final size *before* its buttons exist, so nothing resizes it after `SetSizer()`.  GTK lays out regardless; **Win32 does not**, leaving all fourteen buttons stacked at (0,0) with a single one visible | **BROKEN** | Explicit `Layout()` after `SetSizer()` in `ToolsPanel` and in the three other fixed-size frames with the same latent pattern (`WaypointPanel`, `SceneryPanel`, `InfoPanel`) and in `MainFrame`.  Found by running the cross-compiled Windows build under Wine |

| 49 | wxWidgets dependency for the Windows build | — (build infrastructure, not an original source element) | The Windows executable was linked against a wxWidgets built from source, which is slow, unpinned and easy to get subtly wrong.  Worse, CMake's `FindwxWidgets` selects its search style with `if(WIN32 AND ... AND NOT CMAKE_CROSSCOMPILING)` (`FindwxWidgets.cmake:245`), so a cross build **always** takes the "unix" branch and runs the host's `wx-config` — silently linking Debian's wxGTK into a Windows target | **INCORRECT** | The build now uses the **official upstream wxWidgets Windows/MinGW-w64 binaries** (`wxWidgets/wxWidgets` release `v3.2.6`, ABI tag `gcc1220`, matching the host's GCC 12.2.0 exactly — verified by the DLLs importing `libgcc_s_seh-1.dll`/`libstdc++-6.dll` but **not** `libwinpthread-1.dll`, i.e. the win32 thread model).  `build_windows.sh` downloads the pinned archives, verifies recorded SHA-256 checksums, caches them under `.deps/`, extracts them with `cmake -E tar` (libarchive reads 7z, so no new tooling), refuses to run if `wxWidgets_CONFIG_EXECUTABLE` is set, and greps the linked executable's import table for `gtk\|gdk\|glib\|x11\|cygwin\|msys\|pango\|cairo`, failing the build if any appear.  `cmake/wxMSWPrebuilt.cmake` resolves the package explicitly instead of using `find_package`.  `-static-libgcc`/`-static-libstdc++` were removed: the wx DLLs import `libstdc++-6.dll`, and a second statically linked C++ runtime inside the executable would put two independent runtime states on either side of every wx call.  `make-windows-zip.sh` now walks the import closure of the executable and of every DLL it pulls in, bundling each non-system dependency and failing if one cannot be found.  Verified from a fresh extract under Wine with `env -i`: five DLLs bundled, all fourteen tool buttons drawn, `ctf_Ash.pms` opened with **0** unresolved assets, `polyworks.ini` written beside the executable |

## Remaining known gaps (not fixed)

| Area | Gap |
|---|---|
| Preferences | Tool hotkeys and waypoint keys are not user-remappable (the original's HotKeys / Waypoint Keys pages).  The defaults are reproduced exactly, so no default behaviour is missing |
| Preferences | `cboSkin` (skin selection) is absent; the port always loads `installer/skins/default` |
| Preferences | Window width/height persistence is absent; wxWidgets restores the frame geometry instead |
| Scenery panel | The original splits the list into `lstScenery` ("In Use") and a `tvwScenery` tree.  The port uses a single list and marks in-use entries with a bullet |
| Assets | Soldat's artwork is not redistributable and is not present in this repository.  Resolution is verified with stand-in files placed in the shipped `Textures/` and `Scenery-gfx/` folders (see below), not with the real game assets |
| Skins | `skins/default/colors.ini` supplies the original's GUI colours and fonts (`frm:4648-4655`).  The port hard-codes the same values instead of reading the file, so a user-edited `colors.ini` has no effect.  The file is still shipped |
| Scenery lists | `lists/*.txt` are the original's named scenery lists (`frmScenery.frm:436`, `frm:12613`).  The port does not read or write them; `lists/defaults.txt` is shipped so the data is not lost |
| Wine | Wine 8 with llvmpipe under Xvfb does not present child-window OpenGL at all: a minimal wxGLCanvas that merely clears to red renders nothing.  The GL canvas therefore appears blank there.  This is an environment limitation, not a port defect — the same binary's non-GL UI is fully functional under Wine, and the identical rendering code draws correctly on GTK |
