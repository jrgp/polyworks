#include "editor.h"

#include "ini_file.h"
#include "platform.h"
#include "pms_io.h"
#include "geometry.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>

namespace fs = std::filesystem;

namespace pw {
namespace {

struct ToolInfo {
    const char* name;
    const char* hotkey;
};

/* The fourteen tools frmTools offers, with the hotkeys modConfig.bas assigns
   them (modConfig.bas:165-178). */
constexpr ToolInfo kToolInfo[kSelectableTools] = {
    {"Move", "A"},          {"Create", "Q"},       {"Vertex Select", "S"},
    {"Polygon Select", "W"},{"Vertex Color", "D"}, {"Polygon Color", "E"},
    {"Texture", "F"},       {"Scenery", "R"},      {"Waypoint", "G"},
    {"Objects", "T"},       {"Color Picker", "H"}, {"Sketch", "Y"},
    {"Lights", "J"},        {"Depth Map", "U"},
};

/* VB6 SetTool assigns each ImageList entry a Tag which becomes the caption of
   lblCurrentTool (frm:1655-1683, frm:4256).  These are the exact strings. */
constexpr const char* kFunctionNames[TOOL_COUNT] = {
    "Move Selection", "Create Polygons", "Select Vertices", "Select Polygons",
    "Color Vertices", "Color Polygons", "Transform Texture", "Create Scenery",
    "Create Waypoints", "Place Spawn Points or Colliders",
    "Pick a Vertex Color", "Sketch", "Create Lights", "Edit Depth Map",
    "Scroll Map", "Add to Selection", "Subtract from Selection",
    "Add to Selection", "Subtract from Selection", "Scale Selection",
    "Rotate Selection", "Connect Waypoints", "Create Quad",
    "Pick a pixel color", "Pick a Lit Vertex Color", "Erase Lines",
    "Move Lines",
};

std::string baseName(const std::string& path) {
    return path.empty() ? std::string() : fs::path(path).filename().string();
}

std::string joinPath(const std::string& a, const std::string& b) {
    return (fs::path(a) / b).string();
}

}  // namespace

const char* toolName(int tool) {
    return (tool >= 0 && tool < kSelectableTools) ? kToolInfo[tool].name
                                                  : "Unknown";
}

const char* toolHotkey(int tool) {
    return (tool >= 0 && tool < kSelectableTools) ? kToolInfo[tool].hotkey : "";
}

const char* functionName(int fn) {
    return (fn >= 0 && fn < TOOL_COUNT) ? kFunctionNames[fn] : "Unknown";
}

/* ---- PaletteState ------------------------------------------------------- */

std::string PaletteState::palettesDir() {
    return joinPath(appDataDir(), "palettes");
}

std::string PaletteState::currentPalettePath() {
    /* The original reads and writes appPath\palettes\current.txt (frm:867,
       modConfig.bas:389).  That is exactly what happens in a portable
       install.  When the application directory is not writable -- a macOS
       bundle, or Program Files -- the palette follows polyworks.ini to the
       per-user location, and the shipped palette is still read from beside
       the application if the user has not saved one of their own. */
    const std::string mine = userPalettePath();
    if (fileExists(mine)) {
        return mine;
    }
    const std::string shipped = joinPath(palettesDir(), "current.txt");
    if (fileExists(shipped)) {
        return shipped;
    }
    return mine;
}

std::string PaletteState::userPalettePath() {
    return joinPath(joinPath(fs::path(IniFile::preferredPath("polyworks.ini"))
                                 .parent_path()
                                 .string(),
                             "palettes"),
                    "current.txt");
}

/* SavePalette (frmPalette.frm:711) writes `red & ", " & green & ", " & blue`,
   one cell per line, with Y (the row) as the outer loop; LoadPalette reads it
   back with `Input #1`, which treats a comma as a separator.  The commas are
   part of the format: a whitespace-only parse reads the first number of the
   file and then stops, leaving the whole palette black. */
bool PaletteState::load(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        return false;
    }
    std::size_t i = 0;
    std::string line;
    while (i < cells.size() && std::getline(in, line)) {
        for (char& ch : line) {
            if (ch == ',') {
                ch = ' ';
            }
        }
        std::istringstream fields(line);
        int r = 0, g = 0, b = 0;
        if (!(fields >> r >> g >> b)) {
            continue;   /* Input #1 skips blank lines too */
        }
        cells[i].r = static_cast<uint8_t>(std::clamp(r, 0, 255));
        cells[i].g = static_cast<uint8_t>(std::clamp(g, 0, 255));
        cells[i].b = static_cast<uint8_t>(std::clamp(b, 0, 255));
        ++i;
    }
    return i > 0;
}

bool PaletteState::save(const std::string& path) const {
    std::error_code ec;
    fs::create_directories(fs::path(path).parent_path(), ec);
    std::ofstream out(path, std::ios::trunc);
    if (!out) {
        return false;
    }
    for (const PaletteColor& c : cells) {
        out << static_cast<int>(c.r) << ", " << static_cast<int>(c.g) << ", "
            << static_cast<int>(c.b) << '\n';
    }
    return static_cast<bool>(out);
}

void PaletteState::checkPalette(uint8_t cr, uint8_t cg, uint8_t cb) {
    for (int row = 0; row < kRows; ++row) {
        for (int col = 0; col < kCols; ++col) {
            const PaletteColor& c = cells[static_cast<size_t>(row * kCols + col)];
            if (c.r == cr && c.g == cg && c.b == cb) {
                selCol = col;
                selRow = row;
                return;
            }
        }
    }
    selCol = -1;
    selRow = -1;
}

/* ---- Editor ------------------------------------------------------------- */

Editor::Editor() {
    renderer.setTextureManager(&texMgr);
}

void Editor::initialise(const std::string& skins) {
    skinsPath = skins;
    texMgr.setBasePath(skins);
    registerAppAssetPaths();
    loadPrefs();
    applyPrefs();
    palette.load(PaletteState::currentPalettePath());
    palette.r = static_cast<uint8_t>((prefs.paintColor >> 16) & 0xFF);
    palette.g = static_cast<uint8_t>((prefs.paintColor >> 8) & 0xFF);
    palette.b = static_cast<uint8_t>(prefs.paintColor & 0xFF);
    palette.radius    = prefs.colorRadius;
    palette.opacity   = prefs.colorOpacity;
    palette.blendMode = prefs.colorBlendMode;
    palette.colorMode = prefs.colorMode;
    palette.checkPalette(palette.r, palette.g, palette.b);
    refreshSceneryList();
    interaction.setActiveTool(activeTool);
}

/* VB6 Terminate (frm:4381) calls SaveSettings (modConfig.bas:265), which
   persists the preferences *and* writes the working palette to
   <appPath>/palettes/current.txt (:389). */
void Editor::shutdown() {
    savePrefs();
    palette.save(PaletteState::userPalettePath());
}

/* ---- messages ----------------------------------------------------------- */

void Editor::showMessage(MessageBox::Kind kind, const std::string& title,
                         const std::string& text) {
    messageBox.show(kind, title, text);
}

/* ---- file --------------------------------------------------------------- */

std::string Editor::documentName() const {
    return currentFilePath.empty() ? "Untitled" : baseName(currentFilePath);
}

std::string Editor::windowTitle() const {
    return documentName() + (doc.modified ? "*" : "") + " - PolyWorks";
}

std::string Editor::statusToolLabel() const {
    /* VB6 labels the *effective* function, then appends the polygon type when
       creating and the waypoint direction when placing waypoints
       (frm:4256-4266). */
    std::string label = functionName(interaction.currentFunction());
    if (activeTool == TOOL_CREATE) {
        label += " (";
        label += polyTypeName(creationPolyType);
        label += ")";
    } else if (activeTool == TOOL_WAYPOINT) {
        static const char* const kWayNames[5] = {"Left", "Right", "Up", "Down",
                                                 "Fly"};
        for (int i = 0; i < 5; ++i) {
            if (waypointState.type[i]) {
                label += " (";
                label += kWayNames[i];
                label += ")";
            }
        }
    }
    return label;
}

/* VB6 guards mnuNew_Click (frm:12752), mnuOpen_Click (frm:12772) and Terminate
   (frm:4387) with the same three-way prompt driven by the global `prompt`
   flag.  ImGui has no nested modal loop, so instead of blocking, the caller's
   work is parked in `pendingAfterPrompt` and run when the box is answered.

   When there is nothing to save the original simply falls through to the
   action, so the continuation runs here and now.  This used to report that
   through a return value instead, which every caller discarded -- so File >
   Exit, the window close button, File > New and Open Recent all did nothing
   at all on an unmodified map, which is the usual case. */
void Editor::confirmDiscardChanges(std::function<void()> continuation) {
    if (!doc.modified) {
        if (continuation) {
            continuation();
        }
        return;
    }
    showMessage(MessageBox::Kind::ConfirmCancel, "PolyWorks",
                "Save changes to " + documentName() + "?");
    pendingAfterPrompt = std::move(continuation);
}

void Editor::newMap() {
    doc.clear();
    undo.clear();
    currentFilePath.clear();
    needsRedraw = true;
}

void Editor::resetViewForLoadedMap() {
    /* VB6 LoadFile resets the view (frm:1935-1939): zoomFactor = 1 and
       scrollCoords = -ScaleWidth/2, -ScaleHeight/2, which puts the world
       origin at the centre of the viewport.  Soldat maps are built around the
       origin, so this frames the map. */
    doc.zoom = 1.0f;
    doc.scrollX = -viewport.width * 0.5f;
    doc.scrollY = -viewport.height * 0.5f;
    doc.rebuildScreenCache();

    /* A map given on the command line is loaded before the window has been
       laid out, so the viewport is still zero-sized and the centring above
       lands the origin in the top-left corner.  Remembering that the reset is
       owed lets the frame loop redo it as soon as the real size is known. */
    viewResetPending = !viewport.valid();
}

/*
 * VB6 resolves textures from `OpenSoldatDir & "textures\"` and scenery from
 * `OpenSoldatDir & "Scenery-gfx\"` (frm:4284, frm:2092).  Maps normally live
 * in <soldat>/Maps/, so the parent of the .pms directory is the natural
 * equivalent when no game directory has been configured.  The configured
 * directory is added separately by applyPrefs().
 */
void Editor::registerAssetPathsForMap(const std::string& mapPath) {
    std::error_code ec;
    const fs::path full = fs::weakly_canonical(fs::path(mapPath), ec);
    const fs::path pmsDir = (ec ? fs::path(mapPath) : full).parent_path();
    const fs::path parent = pmsDir.parent_path();

    texMgr.addSearchPath(pmsDir.string());
    texMgr.addSearchPath((pmsDir / "Textures").string());
    texMgr.addSearchPath((pmsDir / "Scenery-gfx").string());
    if (!parent.empty() && parent != pmsDir) {
        texMgr.addSearchPath((parent / "Textures").string());
        texMgr.addSearchPath((parent / "Scenery-gfx").string());
        texMgr.addSearchPath(parent.string());
    }
}

/* Textures and scenery shipped alongside the executable.  A portable PolyWorks
   directory has the same shape as a Soldat installation (`Textures/`,
   `Scenery-gfx/`), so the application directory is searched exactly like a
   configured game directory would be. */
void Editor::registerAppAssetPaths() {
    const std::string dir = appDir();
    if (dir.empty()) {
        return;
    }
    texMgr.addSearchPath(joinPath(dir, "Textures"));
    texMgr.addSearchPath(joinPath(dir, "Scenery-gfx"));
    texMgr.addSearchPath(dir);
    const std::string res = bundleResourcesDir();
    if (!res.empty()) {
        texMgr.addSearchPath(joinPath(res, "Textures"));
        texMgr.addSearchPath(joinPath(res, "Scenery-gfx"));
        texMgr.addSearchPath(res);
    }
}

bool Editor::loadMap(const std::string& path) {
    PmsData data;
    std::string error;
    if (loadPmsFile(path, data, error) != PmsLoadResult::OK) {
        showMessage(MessageBox::Kind::Error, "Open failed", error);
        return false;
    }

    pmsDataToDoc(data, doc);
    resetViewForLoadedMap();  /* after pmsDataToDoc, which clears the document */
    doc.clearModified();
    currentFilePath = path;
    registerAssetPathsForMap(path);
    addToRecentFiles(path);
    undo.clear();
    refreshTextureWindow();
    refreshSceneryInUse();
    needsRedraw = true;
    return true;
}

bool Editor::saveMap(const std::string& path) {
    PmsData data;
    docToPmsData(doc, data);
    std::string error;
    if (!savePmsFile(path, data, error)) {
        showMessage(MessageBox::Kind::Error, "Save failed", error);
        return false;
    }
    currentFilePath = path;
    addToRecentFiles(path);
    doc.clearModified();
    return true;
}

bool Editor::saveCurrent() {
    return currentFilePath.empty() ? false : saveMap(currentFilePath);
}

bool Editor::compileTo(const std::string& path) {
    PmsData data;
    docToPmsData(doc, data);
    std::string error;
    if (!compilePms(path, data, error)) {
        showMessage(MessageBox::Kind::Error, "Compile failed", error);
        return false;
    }
    return true;
}

bool Editor::exportPrefab(const std::string& path) {
    std::string error;
    if (!savePrefab(path, doc, error)) {
        showMessage(MessageBox::Kind::Error, "Export failed", error);
        return false;
    }
    return true;
}

bool Editor::importPrefab(const std::string& path) {
    undo.push(doc);
    std::string error;
    if (!loadPrefab(path, doc, error)) {
        undo.pop();
        showMessage(MessageBox::Kind::Error, "Import failed", error);
        return false;
    }
    doc.markModified();
    needsRedraw = true;
    return true;
}

/* The original wrote the clipboard prefab to <app>\Temp\copy.PFB (frm:11769 /
   frm:12062).  The install directory is read-only on modern platforms, so the
   per-user temp directory is used instead. */
std::string Editor::clipboardPrefabPath() const {
    std::error_code ec;
    const fs::path tmp = fs::temp_directory_path(ec);
    return ((ec ? fs::path(".") : tmp) / "polyworks-copy.PFB").string();
}

void Editor::copySelection() {
    if (!doc.anySelected()) {
        return;
    }
    std::string error;
    if (!savePrefab(clipboardPrefabPath(), doc, error)) {
        showMessage(MessageBox::Kind::Error, "Copy failed", error);
    }
}

void Editor::pasteSelection() {
    const std::string path = clipboardPrefabPath();
    if (!fileExists(path)) {
        return;
    }
    importPrefab(path);
}

/* VB6 Form_Load resolves a command-line map name against, in order: the path
   as given, <appPath>/Maps/, then <OpenSoldatDir>/Maps/ (frm:10657-10668). */
bool Editor::openCommandLineMap(const std::string& arg) {
    std::string name = arg;
    /* VB6 strips one pair of surrounding quotes from Command$. */
    if (!name.empty() && name.back() == '"') name.pop_back();
    if (!name.empty() && name.front() == '"') name.erase(name.begin());
    if (name.size() < 4) {
        return false;
    }
    std::string ext = name.substr(name.size() - 4);
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (ext != ".pms") {
        return false;
    }

    std::vector<std::string> candidates{name,
                                        joinPath(joinPath(appDir(), "Maps"), name)};
    if (!prefs.soldatDir.empty()) {
        candidates.push_back(joinPath(joinPath(prefs.soldatDir, "Maps"), name));
    }
    for (const std::string& c : candidates) {
        if (fileExists(c)) {
            return loadMap(c);
        }
    }
    std::fprintf(stderr, "PolyWorks: could not find map \"%s\"\n", arg.c_str());
    return false;
}

void Editor::runGame(bool openSoldat) {
    if (prefs.soldatDir.empty()) {
        showMessage(MessageBox::Kind::Info, "Run Soldat",
                    "No game directory is configured.\n"
                    "Set it in Map > Preferences.");
        return;
    }
    static const char* const kSoldat[] = {"soldat.exe", "Soldat.exe", "soldat"};
    static const char* const kOpen[] = {"opensoldat.exe", "OpenSoldat.exe",
                                        "opensoldat"};
    for (const char* n : (openSoldat ? kOpen : kSoldat)) {
        const std::string candidate = joinPath(prefs.soldatDir, n);
        if (fileExists(candidate) && launchProgram(candidate, {})) {
            return;
        }
    }
    showMessage(MessageBox::Kind::Error, "Run Soldat",
                std::string("No ") + (openSoldat ? "OpenSoldat" : "Soldat") +
                    " executable was found in\n" + prefs.soldatDir);
}

/* ---- edit --------------------------------------------------------------- */

void Editor::undoAction() {
    if (!undo.undo(doc)) {
        return;
    }
    doc.rebuildScreenCache();
    needsRedraw = true;
}

void Editor::redoAction() {
    if (!undo.redo(doc)) {
        return;
    }
    doc.rebuildScreenCache();
    needsRedraw = true;
}

void Editor::deleteSelected() {
    if (!doc.anySelected()) {
        return;
    }
    undo.push(doc);
    doc.deleteSelected();
    doc.markModified();
    needsRedraw = true;
}

void Editor::duplicateSelected() {
    if (!doc.anySelected()) {
        return;
    }
    undo.push(doc);
    doc.duplicateSelected(32.0f, 0.0f);  /* VB6 mnuDuplicate: +32 X only */
    doc.markModified();
    needsRedraw = true;
}

void Editor::selectAll() {
    doc.selectAll();
    needsRedraw = true;
}

void Editor::deselect() {
    doc.clearSelection();
    needsRedraw = true;
}

void Editor::invertSelection() {
    doc.invertSelection();
    needsRedraw = true;
}

void Editor::selectByColor() {
    doc.selectByColor(palette.r, palette.g, palette.b);
    needsRedraw = true;
}

void Editor::severConnections() {
    undo.push(doc);
    doc.severWaypointConnections();
    doc.markModified();
    needsRedraw = true;
}

void Editor::clearSketch() {
    undo.push(doc);
    doc.clearSketch();
    doc.markModified();
    needsRedraw = true;
}

void Editor::transformSelection(int which) {
    if (!doc.anySelected()) {
        return;
    }
    undo.push(doc);
    switch (which) {
    case 0: doc.flipSelected(true, false); break;
    case 1: doc.flipSelected(false, true); break;
    case 2: doc.rotateSelected(180.0f); break;
    case 3: doc.rotateSelected(90.0f); break;
    case 4: doc.rotateSelected(-90.0f); break;
    default: undo.pop(); return;
    }
    doc.markModified();
    needsRedraw = true;
}

void Editor::polyOperation(int which) {
    if (!doc.anySelected()) {
        return;
    }
    undo.push(doc);
    switch (which) {
    case 0: doc.splitAtVertex(); break;
    case 1: doc.joinSelectedVertices(); break;
    case 2: doc.createPolyFromSelected(); break;
    case 3: {
        int w = 0, h = 0;
        selectedTextureSize(w, h);
        doc.fixTextureOnSelected(w > 0 ? static_cast<float>(w) : 64.0f,
                                 h > 0 ? static_cast<float>(h) : 64.0f);
        break;
    }
    case 4: doc.untextureSelected(); break;
    case 5: doc.averageVertexColors(); break;
    default: undo.pop(); return;
    }
    doc.markModified();
    needsRedraw = true;
}

void Editor::texTransform(int which) {
    if (!doc.anySelected()) {
        return;
    }
    undo.push(doc);
    int texW = 0, texH = 0;
    selectedTextureSize(texW, texH);
    const float aspect = (texH > 0) ? static_cast<float>(texW) /
                                          static_cast<float>(texH)
                                    : 1.0f;
    switch (which) {
    case 0: doc.flipTextureOnSelected(true); break;
    case 1: doc.flipTextureOnSelected(false); break;
    case 2: doc.rotateTextureOnSelected(180.0f, aspect); break;
    case 3: doc.rotateTextureOnSelected(90.0f, aspect); break;
    case 4: doc.rotateTextureOnSelected(-90.0f, aspect); break;
    default: undo.pop(); return;
    }
    doc.markModified();
    needsRedraw = true;
}

void Editor::applyLightToVertices() {
    undo.push(doc);
    doc.applyLightsToBaseColors();
    doc.markModified();
    needsRedraw = true;
}

void Editor::snapSelectedVertices() {
    undo.push(doc);
    if (!doc.snapSelected(prefs.snapRadius)) {
        undo.pop();
    }
    needsRedraw = true;
}

void Editor::arrangeSelected(int which) {
    if (!doc.anySelected()) {
        return;
    }
    undo.push(doc);
    switch (which) {
    case 0: doc.bringSelectedToFront(); break;
    case 1: doc.sendSelectedToBack(); break;
    case 2: doc.bringSelectedForward(); break;
    case 3: doc.sendSelectedBackward(); break;
    default: undo.pop(); return;
    }
    doc.markModified();
    needsRedraw = true;
}

/* VB6 (frm:11007-11010, 11031-11040) moves by one world unit, or by one grid
   sub-division with Shift held. */
void Editor::nudgeSelection(float dirX, float dirY, bool fine) {
    if (!doc.anySelected()) {
        return;
    }
    float step = 1.0f;
    if (fine) {
        const int div = prefs.gridDivisions > 0 ? prefs.gridDivisions : 1;
        step = static_cast<float>(prefs.gridSpacing) / static_cast<float>(div);
        if (step <= 0.0f) {
            step = 1.0f;
        }
    }
    undo.push(doc);
    doc.moveSelected(dirX * step, dirY * step);
    doc.markModified();
    needsRedraw = true;
}

/* ---- view --------------------------------------------------------------- */

void Editor::zoomIn() {
    doc.setZoom(snapZoom(doc.zoom, 1), viewport.width * 0.5f,
                viewport.height * 0.5f);
}

void Editor::zoomOut() {
    doc.setZoom(snapZoom(doc.zoom, -1), viewport.width * 0.5f,
                viewport.height * 0.5f);
}

void Editor::zoomReset() {
    doc.setZoom(prefs.resetZoom, viewport.width * 0.5f, viewport.height * 0.5f);
}

void Editor::centerAndReset() {
    doc.zoom = prefs.resetZoom;
    doc.scrollX = -viewport.width * 0.5f / doc.zoom;
    doc.scrollY = -viewport.height * 0.5f / doc.zoom;
    doc.rebuildScreenCache();
}

void Editor::fitOnScreen() {
    doc.fitToViewport(viewport.width, viewport.height);
}

/* ---- tools -------------------------------------------------------------- */

void Editor::setActiveTool(int tool) {
    if (tool < 0 || tool >= kSelectableTools) {
        return;
    }
    activeTool = tool;
    interaction.setActiveTool(tool);
    needsRedraw = true;
}

void Editor::toggleWaypointType(int idx) {
    if (idx < 0 || idx >= 5) {
        return;
    }
    waypointState.type[idx] = !waypointState.type[idx];
    /* mnuWayType_Click (frm:12396) clears the opposing direction: a waypoint
       cannot tell the bot to go both left and right, or both up and down.
       Fly (index 4) is independent of the other four. */
    static const int kOpposite[5] = {1, 0, 3, 2, -1};
    const int other = kOpposite[idx];
    if (other >= 0 && waypointState.type[idx]) {
        waypointState.type[other] = false;
    }
}

/* ---- assets ------------------------------------------------------------- */

void Editor::selectedTextureSize(int& w, int& h) {
    w = 0;
    h = 0;
    if (doc.options.textureName.empty()) {
        return;
    }
    const unsigned int id = texMgr.loadTexture(doc.options.textureName, false);
    if (id != 0) {
        texMgr.getSize(id, w, h);
    }
    /* The core needs the pixel size for Fixed Texture moves but must not know
       how to load an image, so the UI hands it over whenever it is asked. */
    doc.textureW = w;
    doc.textureH = h;
}

int Editor::orAddSelectedSceneryIndex() {
    if (sceneryState.selected < 0 ||
        sceneryState.selected >= static_cast<int>(sceneryState.available.size())) {
        return 0;
    }
    const std::string& name =
        sceneryState.available[static_cast<size_t>(sceneryState.selected)];
    /* sceneryNames is 1-based with [0] reserved as a sentinel
       (map_document.cpp:15), so the index *is* the vector position. */
    for (int i = 1; i < static_cast<int>(doc.sceneryNames.size()); ++i) {
        if (doc.sceneryNames[static_cast<size_t>(i)] == name) {
            return i;
        }
    }
    doc.sceneryNames.push_back(name);
    return static_cast<int>(doc.sceneryNames.size()) - 1;
}

void Editor::setPaintColorFromPicker(uint8_t r, uint8_t g, uint8_t b) {
    /* VB6 pickers push the absorbed colour into frmPalette via SetValues and
       then reuse it as the active paint colour (frm:7745-7752). */
    palette.r = r;
    palette.g = g;
    palette.b = b;
    palette.checkPalette(r, g, b);
}

void Editor::refreshSceneryList() {
    const std::string dir = prefs.soldatDir.empty()
                                ? std::string()
                                : joinPath(prefs.soldatDir, "Scenery-gfx");
    sceneryState.available.clear();
    sceneryState.selected = -1;
    sceneryState.dir = dir;
    if (dir.empty() || !dirExists(dir)) {
        return;
    }
    std::error_code ec;
    for (const fs::directory_entry& e : fs::directory_iterator(dir, ec)) {
        if (!e.is_regular_file(ec)) {
            continue;
        }
        std::string ext = e.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (ext == ".bmp" || ext == ".png" || ext == ".jpg" || ext == ".gif") {
            sceneryState.available.push_back(e.path().filename().string());
        }
    }
    std::sort(sceneryState.available.begin(), sceneryState.available.end());
    if (!sceneryState.available.empty()) {
        sceneryState.selected = 0;
    }
}

void Editor::refreshSceneryInUse() {
    sceneryState.inUse.clear();
    for (const EditorScenery& s : doc.scenery) {
        if (s.style >= 1 && s.style < static_cast<int>(doc.sceneryNames.size())) {
            const std::string& n = doc.sceneryNames[static_cast<size_t>(s.style)];
            if (std::find(sceneryState.inUse.begin(), sceneryState.inUse.end(),
                          n) == sceneryState.inUse.end()) {
                sceneryState.inUse.push_back(n);
            }
        }
    }
}

void Editor::refreshTextureWindow() {
    textureWindow.texId = 0;
    textureWindow.path.clear();
    textureWindow.hasSelection = false;
    doc.textureW = 0;
    doc.textureH = 0;
    if (doc.options.textureName.empty()) {
        return;
    }
    textureWindow.path = texMgr.resolvePath(doc.options.textureName);
    int w = 0, h = 0;
    selectedTextureSize(w, h);
}

/* ---- preferences -------------------------------------------------------- */

void Editor::loadPrefs() {
    IniFile ini;
    ini.load(IniFile::preferredPath("polyworks.ini"));

    prefs.minZoom   = static_cast<float>(ini.readDouble("Zoom", "Min", prefs.minZoom));
    prefs.maxZoom   = static_cast<float>(ini.readDouble("Zoom", "Max", prefs.maxZoom));
    prefs.resetZoom = static_cast<float>(ini.readDouble("Zoom", "Reset", prefs.resetZoom));
    prefs.sanitiseZoom();
    prefs.gridSpacing   = static_cast<int>(ini.readInt("Grid", "Spacing", prefs.gridSpacing));
    prefs.gridDivisions = static_cast<int>(ini.readInt("Grid", "Divisions", prefs.gridDivisions));
    prefs.gridColor1 = static_cast<unsigned>(ini.readInt("Grid", "Color1", static_cast<long>(prefs.gridColor1)));
    prefs.gridColor2 = static_cast<unsigned>(ini.readInt("Grid", "Color2", static_cast<long>(prefs.gridColor2)));
    prefs.gridAlpha1 = static_cast<float>(ini.readDouble("Grid", "Alpha1", prefs.gridAlpha1));
    prefs.gridAlpha2 = static_cast<float>(ini.readDouble("Grid", "Alpha2", prefs.gridAlpha2));
    prefs.snapEnabled = ini.readBool("Snap", "Enabled", prefs.snapEnabled);
    prefs.snapRadius  = static_cast<float>(ini.readDouble("Snap", "Radius", prefs.snapRadius));
    prefs.undoDepth   = static_cast<int>(ini.readInt("Undo", "Depth", prefs.undoDepth));
    prefs.sceneryVerts = ini.readBool("Preferences", "SceneryVerts", prefs.sceneryVerts);
    prefs.polyBlendSrc  = static_cast<int>(ini.readInt("Blend", "PolySrc", prefs.polyBlendSrc));
    prefs.polyBlendDest = static_cast<int>(ini.readInt("Blend", "PolyDest", prefs.polyBlendDest));
    prefs.wireBlendSrc  = static_cast<int>(ini.readInt("Blend", "WireSrc", prefs.wireBlendSrc));
    prefs.wireBlendDest = static_cast<int>(ini.readInt("Blend", "WireDest", prefs.wireBlendDest));
    prefs.pointColor     = static_cast<unsigned>(ini.readInt("Colors", "Point", static_cast<long>(prefs.pointColor)));
    prefs.selectionColor = static_cast<unsigned>(ini.readInt("Colors", "Selection", static_cast<long>(prefs.selectionColor)));
    prefs.backgroundColor = static_cast<unsigned>(ini.readInt("Colors", "Background", static_cast<long>(prefs.backgroundColor)));
    prefs.paintColor = static_cast<unsigned>(ini.readInt("ToolSettings", "CurrentColor", static_cast<long>(prefs.paintColor))) & 0xFFFFFFu;
    prefs.colorRadius = static_cast<int>(ini.readInt("ToolSettings", "ColorRadius", prefs.colorRadius));
    prefs.colorOpacity = static_cast<float>(ini.readDouble("ToolSettings", "Opacity", prefs.colorOpacity * 100.0)) / 100.0f;
    prefs.colorBlendMode = static_cast<int>(ini.readInt("ToolSettings", "BlendMode", prefs.colorBlendMode));
    prefs.colorMode = static_cast<int>(ini.readInt("ToolSettings", "ColorMode", prefs.colorMode));
    prefs.soldatDir  = ini.readString("Paths", "SoldatDir", prefs.soldatDir);
    prefs.prefabsDir = ini.readString("Paths", "PrefabsDir", prefs.prefabsDir);
    prefs.uncompDir  = ini.readString("Paths", "UncompDir", prefs.uncompDir);

    recentFiles.clear();
    for (int i = 0; i < kMaxRecentFiles; ++i) {
        const std::string s =
            ini.readString("Recent", "File" + std::to_string(i));
        if (s.empty()) {
            break;
        }
        recentFiles.push_back(s);
    }

    /* VB6 modOSME.bas fell back to a well-known install location when the
       configured game directory was absent.  A portable PolyWorks directory
       that ships its own Textures/ and Scenery-gfx/ is checked first, so an
       extract-and-run distribution needs no configuration at all. */
    if (prefs.soldatDir.empty()) {
        const std::string dir = appDir();
        if (!dir.empty() && (dirExists(joinPath(dir, "Textures")) ||
                             dirExists(joinPath(dir, "Scenery-gfx")))) {
            prefs.soldatDir = dir;
        }
    }
    if (prefs.soldatDir.empty()) {
        static const char* const kCandidates[] = {
            "/usr/share/soldat", "/usr/local/share/soldat",
            "/usr/share/opensoldat", "C:\\OpenSoldat", "C:\\Soldat"};
        for (const char* c : kCandidates) {
            if (dirExists(c)) {
                prefs.soldatDir = c;
                break;
            }
        }
    }
}

void Editor::savePrefs() {
    const std::string path = IniFile::preferredPath("polyworks.ini");
    IniFile ini;
    ini.load(path);   /* keep entries this build does not know about */

    ini.write("Zoom", "Min",   static_cast<double>(prefs.minZoom));
    ini.write("Zoom", "Max",   static_cast<double>(prefs.maxZoom));
    ini.write("Zoom", "Reset", static_cast<double>(prefs.resetZoom));
    ini.write("Grid", "Spacing",   static_cast<long>(prefs.gridSpacing));
    ini.write("Grid", "Divisions", static_cast<long>(prefs.gridDivisions));
    ini.write("Grid", "Color1", static_cast<long>(prefs.gridColor1));
    ini.write("Grid", "Color2", static_cast<long>(prefs.gridColor2));
    ini.write("Grid", "Alpha1", static_cast<double>(prefs.gridAlpha1));
    ini.write("Grid", "Alpha2", static_cast<double>(prefs.gridAlpha2));
    ini.write("Snap", "Enabled", prefs.snapEnabled);
    ini.write("Snap", "Radius",  static_cast<double>(prefs.snapRadius));
    ini.write("Undo", "Depth",   static_cast<long>(prefs.undoDepth));
    ini.write("Preferences", "SceneryVerts", prefs.sceneryVerts);
    ini.write("Blend", "PolySrc",  static_cast<long>(prefs.polyBlendSrc));
    ini.write("Blend", "PolyDest", static_cast<long>(prefs.polyBlendDest));
    ini.write("Blend", "WireSrc",  static_cast<long>(prefs.wireBlendSrc));
    ini.write("Blend", "WireDest", static_cast<long>(prefs.wireBlendDest));
    ini.write("Colors", "Point",     static_cast<long>(prefs.pointColor));
    ini.write("Colors", "Selection", static_cast<long>(prefs.selectionColor));
    ini.write("Colors", "Background", static_cast<long>(prefs.backgroundColor));

    prefs.paintColor = (static_cast<unsigned>(palette.r) << 16) |
                       (static_cast<unsigned>(palette.g) << 8) | palette.b;
    prefs.colorRadius    = palette.radius;
    prefs.colorOpacity   = palette.opacity;
    prefs.colorBlendMode = palette.blendMode;
    prefs.colorMode      = palette.colorMode;
    ini.write("ToolSettings", "CurrentColor", static_cast<long>(prefs.paintColor));
    ini.write("ToolSettings", "ColorRadius",  static_cast<long>(prefs.colorRadius));
    ini.write("ToolSettings", "Opacity", static_cast<double>(prefs.colorOpacity * 100.0f));
    ini.write("ToolSettings", "BlendMode", static_cast<long>(prefs.colorBlendMode));
    ini.write("ToolSettings", "ColorMode", static_cast<long>(prefs.colorMode));
    ini.write("Paths", "SoldatDir",  prefs.soldatDir);
    ini.write("Paths", "PrefabsDir", prefs.prefabsDir);
    ini.write("Paths", "UncompDir",  prefs.uncompDir);

    for (int i = 0; i < kMaxRecentFiles; ++i) {
        const std::string key = "File" + std::to_string(i);
        if (i < static_cast<int>(recentFiles.size())) {
            ini.write("Recent", key, recentFiles[static_cast<size_t>(i)]);
        } else {
            ini.erase("Recent", key);
        }
    }
    ini.save();
}

void Editor::applyPrefs() {
    undo.setMaxDepth(prefs.undoDepth);
    ViewSettings& vs = doc.viewSettings;
    vs.gridSize      = static_cast<float>(prefs.gridSpacing);
    vs.gridDivisions = prefs.gridDivisions;
    vs.gridColor1    = prefs.gridColor1;
    vs.gridColor2    = prefs.gridColor2;
    vs.gridAlpha1    = prefs.gridAlpha1;
    vs.gridAlpha2    = prefs.gridAlpha2;
    vs.polyBlendSrc  = prefs.polyBlendSrc;
    vs.polyBlendDest = prefs.polyBlendDest;
    vs.wireBlendSrc  = prefs.wireBlendSrc;
    vs.wireBlendDest = prefs.wireBlendDest;
    vs.sceneryVerts  = prefs.sceneryVerts;
    doc.selectionColor = prefs.selectionColor & 0xFFFFFFu;

    /* Repointing the game directory has to retire the old one, or a stale path
       keeps being searched (and, worse, keeps resolving). */
    if (!m_appliedGameDir.empty() && m_appliedGameDir != prefs.soldatDir) {
        texMgr.removeSearchPath(joinPath(m_appliedGameDir, "Textures"));
        texMgr.removeSearchPath(joinPath(m_appliedGameDir, "Scenery-gfx"));
    }
    if (!prefs.soldatDir.empty()) {
        texMgr.addSearchPath(joinPath(prefs.soldatDir, "Textures"));
        texMgr.addSearchPath(joinPath(prefs.soldatDir, "Scenery-gfx"));
    }
    if (m_appliedGameDir != prefs.soldatDir) {
        m_appliedGameDir = prefs.soldatDir;
        refreshSceneryList();
        refreshTextureWindow();
    }
    needsRedraw = true;
}

void Editor::addToRecentFiles(const std::string& path) {
    if (path.empty()) {
        return;
    }
    std::error_code ec;
    const fs::path full = fs::weakly_canonical(fs::path(path), ec);
    const std::string key = ec ? path : full.string();
    recentFiles.erase(std::remove(recentFiles.begin(), recentFiles.end(), key),
                      recentFiles.end());
    recentFiles.push_front(key);
    while (static_cast<int>(recentFiles.size()) > kMaxRecentFiles) {
        recentFiles.pop_back();
    }
}

}  // namespace pw
