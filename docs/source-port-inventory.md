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
| `appPath` | Application data path | `appDir()` in ui/platform.cpp | PORTED | |
| `SoldatPath` | Soldat installation path | Preferences dialog + `IniFile` | PORTED | |

---

### `modConfig.bas` — Preferences and INI file

| Original Symbol | Functionality | C++ Equivalent | Status | Notes |
|---|---|---|---|---|
| `LoadSettings` | Load INI / registry config | Preferences dialog + `IniFile` | PORTED | |
| `SaveSettings` | Save INI / registry config | Preferences dialog + `IniFile` | PORTED | |
| `DetectSoldatPath` | Auto-detect Soldat install | Windows registry path in prefs | PARTIALLY PORTED | On non-Windows, path is manual only |
| Palette load/save | `appPath\palettes\current.txt` | `PalettePanel` reads/writes .pal | PORTED | |
| Recent files list | MRU file list | `Editor::recentFiles` | PARTIALLY PORTED | History not yet persisted between sessions |

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
| DirectInput keyboard | DI8 key buffer | ImGui key state in `App::handleShortcuts` | PORTED | ImGui key state replaces DI8 |
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
| `appPath` detection | Win32 `GetModuleFileName` | `appDir()` (GetModuleFileName / _NSGetExecutablePath / /proc/self/exe) | PORTED | |

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
| Recent file list | MRU list | `Editor::recentFiles` | PARTIALLY PORTED |
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
| Refresh [F5] | Force repaint | `View > Refresh` | PORTED |

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
| Del | Delete selected | `ImGuiKey_Delete` | PORTED |
| Backspace | Sever connections | `ImGuiKey_Backspace` | PORTED |
| Arrow keys | Nudge selected ±1 | `ImGuiKey_LeftArrow` etc. | PORTED |
| Shift+Arrow | Nudge selected ±10 | `OnKeyDown` | PORTED |
| Escape | Deselect all | `ID_EDIT_DESELECT` | PORTED |
| Ctrl+Z / Ctrl+Y | Undo / Redo | `App::handleShortcuts` | PORTED |
| Ctrl+A | Select All | `App::handleShortcuts` | PORTED |
| Ctrl+S / Ctrl+Shift+S | Save / Save As | `App::handleShortcuts` | PORTED |
| Ctrl+N / Ctrl+O | New / Open | `App::handleShortcuts` | PORTED |
| F9 | Compile | `ID_FILE_COMPILE` | PORTED |
| F5 | Refresh | `View > Refresh` | PORTED |
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

4. **Recent files list** — list is shown but not persisted between sessions (`Editor::recentFiles` is not saved to config)

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
| `MBMouse.ocx` borderless chrome | ImGui windows used instead |
| `COMDLG32.OCX` file dialogs | ImGui file browser in ui/dialogs.cpp used |
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

| 49 | wxWidgets dependency for the Windows build | — (build infrastructure, not an original source element) | The Windows executable was linked against a wxWidgets built from source, which is slow, unpinned and easy to get subtly wrong.  Worse, CMake's `FindwxWidgets` selects its search style with `if(WIN32 AND ... AND NOT CMAKE_CROSSCOMPILING)` (`FindwxWidgets.cmake:245`), so a cross build **always** takes the "unix" branch and runs the host's `wx-config` — silently linking Debian's wxGTK into a Windows target | **INCORRECT** | The build now uses the **official upstream wxWidgets Windows/MinGW-w64 binaries** (`wxWidgets/wxWidgets` release `v3.2.11`).  The wxWidgets version is pinned; the ABI is **detected**, not hardcoded: the script reads the GCC series from `x86_64-w64-mingw32-gcc` (from `__GNUC__`, because Debian rewrites the version string to `12-win32` and `__GNUC_MINOR__` to `0`), queries the upstream release index for the MinGW binaries that actually exist, and selects the artifact built by that series — dying with the list of published ABIs if none matches, and skipping the `gcc1030TDM` tag because TDM-GCC is a different toolchain.  Thread model is verified separately, since upstream's package name does not encode it: a posix-threads build would import `libwinpthread-1.dll`, and the official binaries do not.  `build_windows.sh` downloads the pinned archives, verifies recorded SHA-256 checksums, caches them under `.deps/`, extracts them with `cmake -E tar` (libarchive reads 7z, so no new tooling), refuses to run if `wxWidgets_CONFIG_EXECUTABLE` is set, and greps the linked executable's import table for `gtk\|gdk\|glib\|x11\|cygwin\|msys\|pango\|cairo`, failing the build if any appear.  `cmake/wxMSWPrebuilt.cmake` resolves the package explicitly instead of using `find_package`.  `-static-libgcc`/`-static-libstdc++` were removed: the wx DLLs import `libstdc++-6.dll`, and a second statically linked C++ runtime inside the executable would put two independent runtime states on either side of every wx call.  `make-windows-zip.sh` now walks the import closure of the executable and of every DLL it pulls in, bundling each non-system dependency and failing if one cannot be found.  Verified from a fresh extract under Wine with `env -i`: five DLLs bundled, all fourteen tool buttons drawn, `ctf_Ash.pms` opened with **0** unresolved assets, `polyworks.ini` written beside the executable |

| 50 | Application icon (`installer/PW.ico`, referenced by `installer/pw.nsi` and the VB6 project) | The editor has always presented one icon, the green PolyWorks glyph, in 16x16 and 48x48 frames | `CMakeLists.txt` named `MACOSX_BUNDLE_ICON_FILE "PW.icns"` but **no such file existed and nothing generated one**, so the Mac bundle showed the generic wxWidgets application icon | **PARTIALLY PORTED** | `packaging/make-icns.py` converts the original `.ico` to `.icns` and CMake bakes it into the bundle's `Resources`.  Pure standard-library Python rather than `sips`/`iconutil` so it runs and can be tested off macOS, and it enlarges the 48x48 artwork by nearest-neighbour sampling rather than interpolation, which keeps the original pixels rather than blurring them.  Verified pixel-identical to ImageMagick's decode of the same file, for both the BMP frames the original uses and truecolour+alpha PNG frames |
| 51 | macOS build dependencies | — (build infrastructure) | `build-mac.sh` required `brew install wxwidgets`, and the resulting `.app` loaded wx dylibs from `/opt/homebrew`, so it ran only on the machine that built it | **INCORRECT** | The script now owns its dependencies, as the Windows one does: it downloads the pinned wxWidgets **source** release (checksummed, cached under `.deps/`) and builds it statically with wx's own libpng/libjpeg/zlib, so no Homebrew library can enter the link.  Source rather than binary because wxWidgets publishes no macOS binaries — its release assets are Windows builds, docs and source.  Afterwards every Mach-O in the bundle is walked and the build fails if one loads a library from outside it, and the `.app` is copied to a temporary directory and launched with Homebrew off `PATH`.  Skins are copied into `Contents/Resources`, with a matching lookup added to `getSkinsPath()`, so a bundle dragged to `/Applications` still finds its cursors and bitmaps |

| 52 | Release automation | — (build infrastructure) | Producing a release meant running both build scripts by hand on two machines; there was no macOS artifact at all, because `build-mac.sh` never packaged the `.app` | **MISSING** | `.github/workflows/release.yml` builds both platforms from a pushed `v*` tag and publishes the two ZIPs, named `polyworks-<version>-win-x64.zip` and `polyworks-<version>-macos-arm64.zip`.  The jobs run the existing scripts rather than restating them: Windows cross-compiles inside a `debian:bookworm` container — the toolchain the project is developed against, whose mingw GCC 12.2.0 resolves to the wxWidgets package with hand-verified checksums, and whose `update-alternatives` priorities (win32 60, posix 30) already select the thread model the official binaries use, where Ubuntu ranks them the other way round — and macOS builds natively on `macos-latest`, since it cannot be cross-compiled.  `build-mac.sh --package` was added, using `ditto` rather than `zip` so the bundle's symlinks and permissions survive.  Verified by running the Windows job in the `debian:bookworm` image against a clean `git archive` export with an empty download cache, which caught the image having no `make` |

| 53 | wxWidgets' bundled zlib and libpng on a current macOS SDK | — (build infrastructure) | The macOS build failed outright: three errors in `zutil.c` and a fatal `'fp.h' file not found` in every libpng source file.  Both come from one cause.  zlib 1.2.13 (`src/zlib/zutil.h:140`) and libpng 1.6.37 (`src/png/pngpriv.h:530`) test `defined(TARGET_OS_MAC)` to mean "Classic Mac OS, MPW or CodeWarrior", and take that branch to `#define fdopen(fd,mode) NULL` — which then collides with the real declaration in `<stdio.h>` — and to include `<fp.h>`, a header last seen in Carbon.  TargetConditionals.h defines `TARGET_OS_MAC` as 1 on every Apple platform today, and the macOS 26 SDK reaches it from `<stdio.h>`, so both branches now fire on a perfectly ordinary Mac | **INCORRECT** | `patch_wx_source()` corrects the two conditions in the extracted source before configure runs.  The macro is defined by a system header, so `-U` cannot reach it and there is no configure switch for either branch.  Both edits are exact-match, applied once, and fail the build if the surrounding text is not what is expected, so a wxWidgets upgrade cannot silently build unpatched.  Reproduced and confirmed outside CI by compiling the two libraries with clang 21 and `-DTARGET_OS_MAC`: pristine gives 3 + 1 + 1 errors, matching the runner exactly, and patched gives none.  wxWidgets was also moved from 3.2.6 to 3.2.11 on both platforms, the current 3.2 release, whose changelog carries "Fix build under macOS 26 Tahoe" and "Fix building third party libraries with Xcode 16.3"; the Windows build was re-verified against it in the container and under Wine.  libtiff is no longer built: PolyWorks reads BMP, PNG, JPEG and GIF, and nothing in the editor or in Soldat's assets is TIFF |

| 54 | wxWidgets' regex library on macOS | — (build infrastructure) | The published `v0.0.3` `.app` linked `@rpath/libpcre2-32.0.dylib` and carried a runpath of `/opt/homebrew/Cellar/pcre2/10.47_1/lib`: wxWidgets' configure had found Homebrew's PCRE2 and used it, so the bundle would have failed to launch on any Mac without that exact keg installed.  The dependency audit passed it because `otool -L` reports the string `@rpath/...`, which matches no filesystem prefix — the Homebrew path lives in a separate `LC_RPATH` load command that nothing was reading | **INCORRECT** | wxWidgets is now configured with `--with-regex=builtin`, alongside the image libraries and expat, so it uses the PCRE in its own `3rdparty/` tree and configure cannot reach outside the SDK for it.  The audit was the real defect and was fixed too: it now rejects any `@rpath` dependency (nothing in the bundle needs one — wxWidgets is static, and anything copied in is repointed at `@executable_path`) and reports every `LC_RPATH` that is not `@executable_path` or `@loader_path`.  Found by downloading the release asset and reading the strings in the shipped binary, which is the only check that sees what users actually get.  The stricter audit immediately caught a second baked-in developer path: CMake records the wxWidgets library directory as a build rpath even though the link is entirely static, so the build now configures with `CMAKE_SKIP_BUILD_RPATH` |

| 55 | `frmScenery.ListScenery` (`frmScenery.frm:400-435`) | `Form_Load` fills the scenery list from `OpenSoldatDir & "Scenery-gfx\\"` and selects the first entry, which draws it into the 65x65 `picScenery` preview | `SceneryPanel::ListScenery()` was implemented but **never called from anywhere**, and the panel was constructed with the skins path rather than the game directory, so the list was always empty and no scenery could be placed | **BROKEN** | The list is filled from `m_prefs.soldatDir` once `LoadPrefs()` has run, and re-filled whenever the Preferences game directory actually changes.  `OnScenerySelect` — an empty handler — now loads the highlighted image with `RGB(0,255,0)` masked out, scales it into the 65x65 box and falls back to the skin's `notfound.bmp` |
| 56 | Scenery list "in use" marker | — (port-only detail; the original uses a separate list box) | `static const char* const kInUseMark = "\u2022 "` was converted to `wxString` through the **C locale**, which fails under `LC_ALL=C`, giving an *empty* string.  `StartsWith("")` is then always true and the code stripped four characters from every entry, so the list showed `mp`, `mp`, `mp` and `GetSelectedScenery()` returned a corrupted filename that could not be placed | **BROKEN** | `wxString::FromUTF8` at both sites, measuring with `.length()` (characters) instead of a byte count.  Non-ASCII narrow literals must never be handed to `wxString` implicitly |
| 57 | `frmPreferences` folder buttons (`picFolder`, `picUncomp`, `picPrefabs` -> `SelectFolder`) and Apply validation (`frm:2121-2158`) | Each path box has a folder button opening a directory picker, and Apply rejects a changed game directory that does not exist or does not contain `Maps`, `Textures` and `Scenery-gfx` | The three path boxes had to be typed into by hand and nothing was validated, so a typo silently produced an editor with no textures and no scenery | **PARTIALLY PORTED** | A `...` button beside each box opens `wxDirDialog`; `ValidatePaths()` reproduces the original's checks and keeps the dialog open on failure.  The subdirectory test is case-insensitive, matching the asset resolver, so an install unpacked as `scenery-gfx` on a case-sensitive filesystem is still accepted |
| 58 | `frmColor` (`frmPalette.picColor_Click`, `frm:998`) | Clicking the current-colour swatch opens the colour picker (`frmColor.InitColor` + `ChangeColor`) and the chosen colour becomes the painting colour | `ColorDlg` — a complete 342-line port of `frmColor` — was **never instantiated by anything**.  The swatch click just re-emitted the colour that was already set, so there was no way to reach any colour outside the palette grid | **BROKEN** | The swatch opens `ColorDlg` modally (the portable equivalent of the original's pseudo-modal `ChangeColor`, which disables every other window) and applies the result through `CheckPalette`.  The dialog also re-fits itself on first show: GTK only knows a text control's height once realised, so the initial `Fit()` came out 38px short and the hex field was hidden behind the button bar |
| 59 | `frmPalette` visibility and painting state (`Form_Load` `frm:10608`, `installer/Workspace/current.ini`, `modConfig.bas:146-150`) | The Palette window is open from startup (`[Palette] Visible=True`); `CurrentColor` (default `FFFFFF`), `ColorRadius` (16), `Opacity`, `BlendMode` and `ColorMode` (1 = Normal) are persisted in `[ToolSettings]` and restored | The panel was hidden by default, so the only way to change the fill colour was invisible; the painting colour was never pushed to the viewport at startup, leaving the tools on a built-in default; none of the five settings were persisted; `RefreshPalette`'s radius argument was ignored; and the Window menu's tick marks all started clear regardless of which panels were on screen | **PARTIALLY PORTED** | The panel is shown at startup, the `[ToolSettings]` block round-trips through `polyworks.ini`, the palette pushes its colour and mode into the viewport immediately, and `SyncWindowMenu()` makes the Window menu agree with reality.  Workspace files now record `Visible` per window as the original's do |
| 60 | `picColorMode` (`frmPalette.frm:620-633`) | Three radio-style buttons select Precision / Normal / Dynamic vertex colouring, drawn pressed for the active mode | The three 16x16 indicators were plain panels filled with the window background colour — invisible in both states — and `onColorModeChanged` was never wired to anything, so the mode could not be seen or used | **BROKEN** | The indicators are drawn explicitly (bordered, with a filled centre when active), default to Normal as the config does, and the callback now reaches `GlViewport::setColorMode()`, which implements all three original behaviours: precision colours the single nearest vertex on click only, normal masks each vertex once per stroke, dynamic repaints continuously.  Normal mode also tints scenery within the radius, as `VertexColoring` does (`frm:7624-7652`) |
| 61 | Floating tool window placement | The VB6 layout offsets assume a large desktop | Panels were positioned with fixed offsets from the main window and could land wholly off a smaller screen — on a 1600x1200 display the Palette and Properties windows were both partly or entirely outside it, which looks exactly like a missing feature | **INCORRECT** | `PlacePanelOnScreen()` nudges each panel back inside the display's client area, at startup and when loading a workspace file |

| 62 | Scenery drawing anchor and rotation (`frm:2027-2035`, `frm:2879`, `PointInProp` `frm:9511`, outline `frm:3383-3441`) | `D3DXSprite.Draw` is given a rotation centre of `(0,0)` and a screen translation of `(Prop.X - scroll) * zoom`, so a prop's map coordinate is the **top-left corner of its sprite** and the sprite rotates about that corner.  `PointInProp` confirms it independently: it rotates the click by `+Rotation` about the prop's coordinate and tests `0..Width*ScaleX`, `0..Height*ScaleY`.  So does the outline code, whose corner formulae are the matrix `[[cos, sin], [-sin, cos]]` in the y-down screen frame, i.e. a rotation by `-Rotation` | The renderer drew each prop **centred** on its coordinate and rotated it by `+rotation`, so every piece of scenery in every map was displaced by half its own size and spun the wrong way.  Nothing in the port implemented `PointInProp` at all, so scenery could not be clicked | **INCORRECT** | `renderScenery()` emits the quad `(0,0)..(w,h)` at the map coordinate and rotates by `-rotation`; `MapDocument::pointInScenery()` is a direct port of `PointInProp`.  Both derivations agree, which is why the sign is stated with confidence rather than guessed |
| 63 | `SelNearest` (`frm:6746-6870`) | The Move tool's click-pick, in a fixed order: polygon vertices within 8px (**every** coincident vertex is taken, which is how vertices shared between polygons move together), then the nearest vertex within 64px of a polygon that contains the click, then the first scenery whose sprite contains the click, then the nearest spawn within 8 world units, then a collider whose own `radius/2` contains the click, then the nearest waypoint within 8 world units.  Each stage is skipped when its layer is hidden.  The resulting selection is *transient*: `mnuDeselect_Click` drops it on mouse-up (`frm:11734`) | The Move tool ran the same code as the vertex-selection tool.  It could not pick scenery, spawns, colliders or waypoints, ignored the display flags, and left its selection behind after the drag | **PARTIALLY PORTED** | `MapDocument::selectNearestObject()` ports the search exactly, including the mixed tolerance spaces (the polygon passes are in screen pixels, the object passes in world units) and the layer gating.  `GlViewport` uses it for `TOOL_MOVE` only when nothing is already selected, and clears such a pick on button-up |
| 64 | Rectangle selection (`VertexSelBox`/`VertexSelSelect` `frm:8840-9145`, `RegionSelPolys` `frm:8515`) | Dragging a box selects polygon vertices **and** scenery, spawns, colliders, waypoints and lights inside it, each gated on its display flag.  `RegionSelPolys` — the fallback pick — is capped at 64 world units, so a click far from any polygon selects nothing | The box selected polygon vertices only, so there was no way to select several props or spawns at once, and the region pick had no distance cap and would grab an arbitrarily distant polygon | **PARTIALLY PORTED** | `selectVerticesInRect()` covers all six classes with the original's display gating, and `selectVertexAt()` applies the 64-unit cap.  Scenery contributes its anchor corner, plus the other three corners when the `SceneryVerts` preference is on, exactly as `frm:9010` does |
| 65 | `SceneryVerts` (`frmPreferences` "Use 4 verts for scenery", `modConfig.bas:82`) | A preference deciding whether a prop exposes four corner points or only its anchor, both when drawing points (`frm:3436`) and when selecting (`frm:9010`) | Absent | **MISSING** | Added to the Preferences Snap page, persisted in `[Preferences]`, and honoured by both the point overlay and rectangle selection |
| 66 | Prop loading validation (`frm:2019-2050`) | Props are rejected when `\|X\|` or `\|Y\|` exceeds 32766, width or height is negative, `Int(Scale*1000)` is 0, `\|Scale\|` exceeds 10000, or `Style` is below 1; alpha outside 1..255 is forced to 255 and level outside 0..255 to 0.  The `Active` byte is never consulted | The loader **dropped every prop whose `active` byte was 0** and performed none of the original's validation.  Maps written by tools that leave `Active` clear lost all their scenery silently | **INCORRECT** | `pms_io.cpp` implements the original's checks and clamps and ignores `Active`, as the original does |
| 67 | `frmDisplay` layout and the master scenery toggle (`frmDisplay.frm`, `frm:6800`, `frm:7624`) | Eleven toggles in two columns; the left column ends with a single **Scenery** switch that hides all scenery everywhere — picking, painting and drawing alike.  The per-layer back/middle/front switches live in View > Scenery Layers | There was no master scenery flag anywhere in the port.  The Display panel offered the three layer switches instead, in one tall column that overlapped the neighbouring Scenery window | **PARTIALLY PORTED** | `ViewSettings::showScenery` was added and is honoured by the renderer, by `SelNearest`, by rectangle selection and by the colour tool's scenery tinting.  The panel is two-column, matching the original, and a matching View menu item was added.  `SyncViewMenu()` and `DisplayPanel::Sync()` keep the two in step, since the original's checkboxes and menu are two views of one flag |
| 68 | `frmScenery` placement options (`frmScenery.frm:363-365`) | The Level radios and the Rotate/Scale checkboxes decide how the next placed prop is created | The panel wrote them to private members that nothing read, while the viewport read a *second*, unrelated copy on `MainFrame`.  Changing any of the four had no effect | **BROKEN** | `MainFrame`'s accessors now delegate to the panel, so there is one copy of the state.  Newly placed props also record the texture's pixel size in `width`/`height`, as the original's save path does (`frm:2737`) |
| 69 | `frmMap` texture list and preview (`LoadTextures`/`LoadTextures2` `frm:550-612`, `cboTexture_Click` `frm:780`) | The texture combo lists `<OpenSoldatDir>/textures/*.bmp` and `*.png`, and selecting one draws it into a 128x128 preview immediately | The dialog enumerated the **skins** directory, so once a game directory was configured the list still did not contain the game's textures, and there was no preview.  Two colour-swatch handlers were declared and never defined or bound | **PARTIALLY PORTED** | The list is read from the configured game directory (the skin directory remains a fallback for a fresh install), the 128x128 preview was added and updates as the selection changes, and the dead declarations were removed.  The dialog stays modal rather than the original's modeless live-apply; see below |

| 70 | `cboBlendMode` (`frmPalette.frx`:0x16, `ApplyBlend` `frm:9653`) | Six blend modes -- Normal, Multiply, Screen, Darken, Lighten, Difference -- each with its own formula, mixed against the destination by opacity | The core implemented all six correctly, but the combo offered only **three** items named "Normal", "Additive" and "Subtractive".  Indices 3-5 (darken, lighten, difference) were unreachable from the UI, and the two extra names were wrong for the modes they selected: choosing "Additive" ran multiply and "Subtractive" ran screen | **INCORRECT** | The combo is filled from the six names in the `.frx`.  `Refresh()` also clamps an out-of-range index from a hand-edited ini, which would otherwise assert in wxWidgets.  Two tests pin all six formulae and the out-of-range case |
| 71 | `mnuNewColor` / `NewPaletteColor` (`frmPalette.frm:527`, `:743`, `:982`) | Right-clicking a palette cell pops a one-item **Add to Palette** menu; choosing it writes the current RGB into that cell and moves the selection there | The right-click **overwrote the cell immediately**, with no menu and no way to decline, so a stray right-click silently destroyed a palette entry | **INCORRECT** | The popup menu is shown, and the write also saves the palette so the change survives a restart |
| 72 | `frmPalette` layout and `lblPal(6)` (`frmPalette.frm:20-449`) | Two columns: the 63x63 swatch over R/G/B on the left; the "Vertex Color:" heading, the three colour-mode buttons, then Radius, Opacity and Mode on the right; the 192x96 grid beneath | One tall single column of eight rows, and the "Vertex Color:" heading -- which is what tells the user the three mode buttons apply to vertex painting -- was absent | **PARTIALLY PORTED** | Rebuilt in the original's two columns with the heading restored.  Sizers rather than absolute positions, since a desktop font is not Arial 8.25 at 96dpi |
| 73 | Palette selection marker (`shpSel1`/`shpSel2`, `frmPalette.frm:278-292`) | Two overlaid rectangles: a 16x16 black one on the cell and a 14x14 white one inset by a pixel.  The pair is what keeps the marker visible over both light and dark swatches | A single white rectangle, invisible on a white palette entry | **PARTIALLY PORTED** | Both rectangles are drawn |
| 74 | Numeric field focus behaviour (`txtRadius`/`txtRGB`/`txtOpacity` `_GotFocus`/`_LostFocus`, `frm:1014-1106`) | Each box selects its text on focus, and on focus loss clamps to its range (radius 4..128, RGB 0..255, opacity 0..100) or restores the pre-edit text if left non-numeric | Only the `_Change` half was ported.  Out-of-range input was ignored *silently*: the box kept showing "500" while the value actually in force was still the old one, so the control lied about the editor's state | **PARTIALLY PORTED** | Focus in selects all; focus out clamps or restores, writes the corrected text back, and re-emits the change so the clamped value reaches the viewport |

| 75 | `SetMapTexture` (`frm:4279`) | On failure the routine falls into its `ErrorHandler` and returns with `mapTexture` left unset, so the polygons draw in their vertex colours.  `notfound.bmp` is substituted **only** for scenery (`frm:2083`, `frm:2409`, `frm:2468`) | `TextureManager::loadTexture` returned the placeholder for every caller, including the map texture, so a PolyWorks without Soldat's artwork tiled a "missing image" cross over all 209 polygons of a map and hid it completely.  The renderer already handled `texId == 0` correctly; it was simply never given one | **INCORRECT** | `loadTexture` takes `fallbackToNotFound`, passed `false` at the two map-texture call sites and left `true` for scenery.  Unresolvable names are also remembered, so the renderer neither rescans every search directory nor reprints the diagnostic once per frame - verified under Wine, where a map missing 26 assets logs 26 lines rather than 26 per frame |
| 76 | `frmTools` window (`frmTools.frm:5`, `:271`, `:467`) | A `Fixed Single` form with `ControlBox = 0`, carrying a 17px `picTitle` strip whose `MouseDown` does `ReleaseCapture` + `SendMessage WM_NCLBUTTONDOWN` -- the strip exists so the window can be dragged -- and a `picHide` button beside it that closes it | The port used `wxBORDER_SIMPLE` with no `wxCAPTION`, so on Windows the window had no title bar and **could not be moved at all**: there was nothing to grab.  Its size was also fixed at the original's 64x240 before the buttons existed, and a `wxBitmapButton` is not 32x32 on MSW, so the 2x7 grid overflowed the client area and was clipped | **INCORRECT** | `wxCAPTION | wxCLOSE_BOX` (keeping `wxFRAME_TOOL_WINDOW` for the slim caption) is the portable equivalent of a strip whose whole purpose is to forward `WM_NCLBUTTONDOWN`, and `SetSizerAndFit` sizes the frame to its buttons on every platform.  Verified under Wine from the extracted portable ZIP: the caption and all fourteen buttons are present |
| 77 | macOS code signature | — (build infrastructure) | The `.app` was signed only inside `if [[ -d "$frameworks" ]]`, and wxWidgets is linked statically so `Contents/Frameworks` is never created: **the bundle was never signed at all**.  On Apple silicon the kernel refuses to execute an unsigned binary, and Finder reports that as "the application is damaged and can't be opened", so every published Mac release looked like a corrupt download.  The call was also `2>/dev/null || true`, so a signing failure could not have been noticed either | **BROKEN** | `sign_bundle` runs unconditionally after the bundle is final, fails loudly, and verifies the result with `--deep --strict`.  The signature is re-verified after the bundle is copied out of the build tree and again after the release ZIP has been unpacked, since an archive that does not preserve it produces exactly the same "damaged" symptom.  `ditto` replaces `cp -R` in the smoke test for the same reason.  `--timestamp` is chosen from the identity: ad-hoc signatures cannot carry one, Developer ID signatures must |
| 78 | GUI toolkit | — (architecture) | The interface was built on wxWidgets.  Four defects came out of that and none of them were visible to the test suite: on Windows the UI rendered proportionally tiny; the map filled only part of the viewport on a scaled display; clicks did not follow the zoom; and macOS trackpad zoom was violently oversensitive.  The first three all trace to the same root — the GL rectangle, the projection and the cursor mapping were each computed independently from a different notion of "the window size" | **REPLACED** | The GUI is now Dear ImGui + GLFW + OpenGL, drawing every menu, panel, dialog, context menu and control itself.  `core/viewport_geometry.h` is the single description of the viewport: `glRect()` produces the GL rectangle in framebuffer pixels while `glOrtho` is set up in logical units, and `windowToViewport()` translates cursor positions without ever scaling them.  `consumeWheelNotches()` normalises high-resolution trackpad scroll into whole notches.  All four are pinned by tests that need no display.  `uiScale = contentScale / framebufferScale` handles the Windows-measures-pixels / macOS-measures-points difference in one place.  The ImGui style is built from the skin's `colors.ini`, so the result does not look like default ImGui.  wxWidgets is gone from the sources, the CMake files and all three build scripts; with it went the wx and GCC runtime DLLs, so the Windows ZIP now contains the executable and no redistributable DLL at all |
| 79 | `mnuQuad` / `TOOL_QUAD` (`frm:11113`, `frm:11146`) | Right-clicking with Create or Textured Quad active raises `mnuPolyTypes`; `TOOL_QUAD` then places a four-vertex textured quad on left-click, the same code path as `TOOL_CREATE` | The wxWidgets `HandleLeftDownEdit` had a `case TOOL_CREATE` and no `case TOOL_QUAD`, so selecting Textured Quad from the right-click menu left the tool active but **every left-click did nothing** | **BROKEN** | `Interaction::onLeftDown` handles both, and `drawViewportContextMenu` reproduces the original's own test — `currentFunction == TOOL_CREATE || currentFunction == TOOL_QUAD` — rather than inventing one |
| 80 | `frmWaypoints.showPaths` (`frm:3618`, `frm:8717`, `frm:9126`) | A filter restricting the editor to one waypoint path.  The original uses **two different tests**: drawing and connection-making require `pathNum` to be exactly 1 or 2 and to match the filter, so a waypoint on no path is never drawn, while picking and region-select accept `filter == 0 \|\| filter == pathNum` | The filter was absent: every waypoint was always drawn, picked and connectable regardless of path | **NOT PORTED** | `ViewSettings::waypointPathFilter` with the two predicates kept deliberately distinct (`waypointPathDrawn` / `waypointPathVisible`), honoured by the renderer, `SelNearest`, `VertexSelWaypoints` and `connectWaypointAt`, and exposed by the Waypoints panel's Show radio group.  `renderWaypoints` also now draws the real `objects.bmp` atlas sprites (row 2, columns 3/4 unselected and 5/6 selected) instead of plain grey squares |
| 81 | `LoadPalette` / `SavePalette` (`frmPalette.frm:669`, `frmPalette.frm:711`) | `Print #1, red & ", " & green & ", " & blue` writes one cell per line and `Input #1` reads it back, treating the comma as a separator.  `installer/palettes/current.txt` is in that format, and `mnuOpen`-time startup loads it (`frm:867`).  The grid is filled with Y (row) as the outer loop | The port parsed the file with `in >> r >> g >> b`, which stops at the first comma: exactly one number was read and the load reported failure, so **every swatch in the palette was black** on every run.  The directory was wrong as well — `appDir()/palettes` is right for a packaged build but not for a development checkout, where the palettes live beside the skins — and saving wrote whitespace-separated triplets the original could not read back | **BROKEN** | `load()` splits on commas and skips blank lines, `save()` emits the original's `"r, g, b"` spelling, and `palettesDir()` hangs off the new `appDataDir()` (the directory containing `skins/`, which is what VB6's `appPath` is).  Writes go to `userPalettePath()`, the same directory in a portable install and the per-user one when the application directory is read-only, so a macOS bundle's signature is not invalidated.  Verified by screenshot: the shipped greyscale ramp now appears, from the development tree and from the extracted Windows ZIP under Wine |
| 82 | `Workspace\current.ini` default panel layout | The shipped workspace puts Tools, Properties and Waypoints in the left column (`Left=-1`) and Palette, Display and Scenery in the right (`Left=1071`), in that vertical order | The port had Tools, Display and Palette on the left and Scenery, Waypoints and Properties on the right, and the bottom panels ran off the window into the status bar, which hid the cursor position, the file name and the zoom | **INCORRECT** | The defaults follow the shipped workspace's own column assignment, and `beginPanel` gained bottom-edge anchoring (a negative y, pivoted on the window's own bottom edge so it works for the auto-resizing panels) so the lowest panel in each column sits just above the status bar whatever the window size |
| 83 | `CreateScenery` (`frm:8182`, `frm:6940`, `frmScenery.frm` `rotateScenery`/`scaleScenery`) | Placing scenery is a **three-click gesture**, not one click: click 1 anchors the position, mouse movement then sets the rotation (Shift quantises to 15 degrees), click 2 fixes it, movement then sets the scale (`cos/sin(angle - rotation) * len / spriteSize`, Shift scaling uniformly along the sprite diagonal), and click 3 creates the instance.  The panel's Rotate and Scale checkboxes *remove* their step by advancing `numCorners` past it (`frm:8211`) | The port placed the sprite immediately on the first click at rotation 0 and scale 1.  The Rotate and Scale checkboxes were stored but **never read by anything**: scenery could only ever be placed unrotated and unscaled | **PARTIALLY PORTED** | `InteractionState::PlacingScenery` with `advanceSceneryPlacement` / `updateSceneryPlacement` / `commitScenery`, driven by the same `numCorners` skipping rule so the checkboxes behave as the original's do.  Escape and a tool change abort the placement.  `drawViewport` draws the pending sprite as a translucent rotated quad so the rotation and scale steps are visible while they are being chosen |
| 84 | Waypoint creation (`frm:11315-11351`) | Creating a waypoint turns the Waypoints layer on, resets the Show filter if it would hide the new path, deselects, sets `pathNum` from the panel, and assigns `currentWaypoint` **only inside the `If currentWaypoint > 0` branch** -- so the Waypoint tool alone never chains; only a chain begun with the Connect tool continues | The port advanced `currentWaypoint` unconditionally, so every waypoint the user placed was wired to the previous one, producing a single connected path the user never asked for.  `pathNum` was never set from the panel, and a new waypoint could land on a hidden layer or behind the path filter and appear not to have been created at all | **INCORRECT** | The chaining assignment moved inside the branch, `pathNum` comes from the panel, and the layer/filter are forced exactly as the original forces them.  `setActiveTool` clears `currentWaypoint` when leaving the waypoint tools (`frm:4229`) |
| 85 | `mnuWayType_Click` (`frm:12396`) | Toggling a waypoint direction clears its opposite: 0 clears 1, 1 clears 0, 2 clears 3, 3 clears 2.  Fly (index 4) is independent.  A waypoint cannot tell a bot to go both left and right | Both the context menu and the Waypoints panel toggled the five flags independently, so contradictory pairs could be set | **INCORRECT** | Both routes go through `Editor::toggleWaypointType`, which applies the exclusion once |
| 86 | `fixedTexture` (`frm:7106`, `frm:8325`) | With Fixed Texture on, moving selected vertices advances their UVs by the same world distance, so the texture stays pinned to the background rather than travelling with the polygon | `viewSettings.fixedTexture` was settable from the Texture menu but **no code ever read it**: the option had no effect | **NOT PORTED** | `MapDocument::moveSelected` advances `tu`/`tv` by the world delta divided by the texture dimensions.  The size comes from the loaded map texture through `Editor::selectedTextureSize`, cached in `textureW`/`textureH`; when it is unknown the UVs are left alone rather than corrupted.  Covered by `TEST(map_document_fixed_texture_move)` |
| 87 | `cboJet` (`frmMap.frm:725`, `GetJets`) | Jet fuel is chosen from nine named presets -- None/Minimal/Very low/Low/Normal/High/Maximum/Infinite mapping to `0, 12, 45, 95, 190, 320, 800, 32766` -- and the number box is **disabled** unless Custom is selected.  An existing value is reverse-mapped back onto its preset | The port offered only a bare number field, so the preset names the original's users work in terms of were unavailable | **PARTIALLY PORTED** | The preset combo with the original's values and order; the number field is disabled unless Custom, and the current value is reverse-mapped on every frame the way `GetJets` does |
| 88 | `picTexture` (`frmMap.frm:64`, `cboTexture_Click` at `:790`) | Map Settings shows the selected texture, updating as the combo changes, so a wrong choice is obvious before OK | The dialog showed only the texture's file name | **PARTIALLY PORTED** | The dialog draws the resolved texture through `App::previewTexture`, scaled to fit, and says so plainly when the name does not resolve |
| 89 | Zoom range validation (`modConfig.bas:101-123`) | `LoadConfig` does not trust the stored zoom range: equal limits fall back to the defaults, reversed limits are swapped, and the reset zoom is clamped into whatever range survives | The port read all three values straight out of the ini and the Preferences dialog only clamped Max against Min, so a hand-edited or reversed range produced an editor that could not zoom | **NOT PORTED** | `AppPrefs::sanitiseZoom()` applies the original's three rules, and is called both when the ini is loaded and when the Preferences dialog commits.  Covered by `TEST(prefs_sanitise_zoom)` |

## Remaining known gaps (not fixed)

| Area | Gap |
|---|---|
| Preferences | Tool hotkeys and waypoint keys are not user-remappable (the original's HotKeys / Waypoint Keys pages).  The defaults are reproduced exactly, so no default behaviour is missing |
| Preferences | `cboSkin` (skin selection) is absent; the port always loads `installer/skins/default` |
| Preferences | Window width/height persistence is absent; ImGui restores the tool-window geometry from `imgui.ini` instead |
| Scenery panel | The original splits the list into `lstScenery` ("In Use") and a `tvwScenery` tree.  The port uses a single list and marks in-use entries with a bullet |
| Assets | Soldat's artwork is not redistributable and is not present in this repository.  Resolution is verified with stand-in files placed in the shipped `Textures/` and `Scenery-gfx/` folders (see below), not with the real game assets |
| Scenery lists | `lists/*.txt` are the original's named scenery lists (`frmScenery.frm:436`, `frm:12613`).  The port does not read or write them; `lists/defaults.txt` is shipped so the data is not lost |
| Map Settings | The original `frmMap` is modeless and applies each change as it is made, with Cancel restoring the previous values.  The port's dialog is modal and applies on OK.  No setting is missing, and the texture preview now behaves as the original's does |
| Wine | Resolved by the move to GLFW.  Wine 8 with llvmpipe would not present *child-window* OpenGL, which is what a `wxGLCanvas` is; GLFW draws into the top-level window, and the Windows build now renders a real map correctly under Wine on Xvfb |
