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
| Mouse wheel zoom | Zoom on wheel | `GlViewport::OnMouseWheel` | PORTED | |
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
| `picViewport_MouseWheel` | Zoom | `GlViewport::OnMouseWheel` | PORTED |
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
