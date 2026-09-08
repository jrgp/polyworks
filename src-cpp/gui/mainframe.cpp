#include "mainframe.h"

#include "gl_viewport.h"
#include "panels/tools_panel.h"
#include "panels/display_panel.h"
#include "panels/info_panel.h"
#include "panels/texture_panel.h"
#include "panels/scenery_panel.h"
#include "panels/waypoint_panel.h"
#include "panels/palette_panel.h"
#include "dialogs/map_settings_dlg.h"
#include "dialogs/preferences_dlg.h"
#include "pms_io.h"
#include "geometry.h"

#include <wx/filedlg.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/filename.h>
#include <wx/sizer.h>
#include <wx/config.h>
#include <wx/stdpaths.h>
#include <wx/log.h>
#include <wx/utils.h>

#include <array>

namespace {

enum MenuId {
    ID_FILE_OPEN_COMPILED = wxID_HIGHEST + 1,
    ID_FILE_COMPILE,
    ID_FILE_COMPILE_AS,
    ID_FILE_EXPORT,
    ID_FILE_IMPORT,
    ID_FILE_RUN_OPENSOLDAT,
    ID_FILE_RUN_SOLDAT,
    ID_EDIT_DUPLICATE,
    ID_EDIT_DELETE_SELECTED,
    ID_EDIT_DESELECT,
    ID_EDIT_INVERT_SELECTION,
    ID_EDIT_SELECT_BY_COLOR,
    ID_POLY_SPLIT_AT_VERTEX,
    ID_POLY_JOIN_VERTICES,
    ID_POLY_CREATE_WITH_SELECTED,
    ID_POLY_FIX_TEXTURE,
    ID_POLY_UNTEXTURE,
    ID_POLY_AVERAGE_COLORS,
    ID_MAP_SETTINGS,
    ID_MAP_PREFERENCES,
    ID_ARRANGE_BRING_TO_FRONT,
    ID_ARRANGE_SEND_TO_BACK,
    ID_ARRANGE_BRING_FORWARD,
    ID_ARRANGE_SEND_BACKWARD,
    ID_VIEW_GRID,
    ID_VIEW_ZOOM_IN,
    ID_VIEW_ZOOM_OUT,
    ID_VIEW_ZOOM_RESET,
    ID_VIEW_CENTER_RESET,
    ID_VIEW_PALETTE,
    ID_POLY_TYPE_BASE,
    /* Window menu */
    ID_WINDOW_SHOW_ALL = ID_POLY_TYPE_BASE + 50,
    ID_WINDOW_HIDE_ALL,
    ID_WINDOW_LOAD_WORKSPACE,
    ID_WINDOW_SAVE_WORKSPACE,
    ID_WINDOW_RESET_LAYOUT,
    ID_WINDOW_TOOLS,
    ID_WINDOW_DISPLAY,
    ID_WINDOW_PALETTE,
    ID_WINDOW_WAYPOINTS,
    ID_WINDOW_SCENERY,
    ID_WINDOW_PROPERTIES,
    ID_WINDOW_TEXTURE,
    /* View layer toggles (named so Bind() can wire handlers) */
    ID_VIEW_POLYGONS,
    ID_VIEW_WIREFRAME,
    ID_VIEW_POINTS,
    ID_VIEW_OBJECTS,
    ID_VIEW_WAYPOINTS,
    ID_VIEW_LIGHTS,
    ID_VIEW_SKETCH,
    ID_VIEW_TEXTURES,
    ID_VIEW_BACKGROUND,
    ID_VIEW_SCENERY_BACK,
    ID_VIEW_SCENERY_MIDDLE,
    ID_VIEW_SCENERY_FRONT,
    /* Additional View items */
    ID_VIEW_FIT_ON_SCREEN,
    ID_VIEW_SNAP_TO_GRID,
    ID_VIEW_SNAP_TO_VERTS,
    ID_VIEW_BLEND_WIREFRAME,
    ID_VIEW_BLEND_POLYS,
    /* Edit menu additions */
    ID_EDIT_SEVER_CONNECTIONS,
    ID_EDIT_CLEAR_SKETCH,
    ID_EDIT_TRANSFORM_FLIP_H,
    ID_EDIT_TRANSFORM_FLIP_V,
    ID_EDIT_TRANSFORM_ROTATE_180,
    ID_EDIT_TRANSFORM_ROTATE_90CW,
    ID_EDIT_TRANSFORM_ROTATE_90CCW,
    /* Polygon menu additions */
    ID_POLY_FIXED_TEXTURE,
    ID_POLY_CUSTOM_TEX_X,
    ID_POLY_CUSTOM_TEX_Y,
    ID_EDIT_SNAP_SELECTED,
    ID_POLY_APPLY_LIGHT,
    ID_POLY_TEX_FLIP_H,
    ID_POLY_TEX_FLIP_V,
    ID_POLY_TEX_ROTATE_180,
    ID_POLY_TEX_ROTATE_90CW,
    ID_POLY_TEX_ROTATE_90CCW,
};

struct ToolInfo {
    const char* name;
    const char* hotkey;
};

constexpr std::array<ToolInfo, 14> kToolInfo{{
    {"Move", "A"},
    {"Create", "Q"},
    {"Vertex Select", "S"},
    {"Polygon Select", "W"},
    {"Vertex Color", "D"},
    {"Polygon Color", "E"},
    {"Texture", "F"},
    {"Scenery", "R"},
    {"Waypoint", "G"},
    {"Objects", "T"},
    {"Color Picker", "H"},
    {"Sketch", "Z"},
    {"Lights", "J"},
    {"Depth Map", "U"},
}};

/* VB6 SetTool assigns each ImageList entry a Tag which becomes the caption of
   lblCurrentTool (frm:1655-1683, frm:4256).  These are the exact strings. */
constexpr std::array<const char*, 27> kFunctionNames{{
    "Move Selection", "Create Polygons", "Select Vertices", "Select Polygons",
    "Color Vertices", "Color Polygons", "Transform Texture", "Create Scenery",
    "Create Waypoints", "Place Spawn Points or Colliders",
    "Pick a Vertex Color", "Sketch", "Create Lights", "Edit Depth Map",
    "Scroll Map", "Add to Selection", "Subtract from Selection",
    "Add to Selection", "Subtract from Selection", "Scale Selection",
    "Rotate Selection", "Connect Waypoints", "Create Quad",
    "Pick a pixel color", "Pick a Lit Vertex Color", "Erase Lines",
    "Move Lines",
}};

wxString BaseNameOrUntitled(const wxString& path) {
    if (path.empty()) {
        return "Untitled";
    }

    wxFileName fileName(path);
    return fileName.GetFullName();
}

}  // namespace

wxString GetToolName(int tool) {
    if (tool >= 0 && tool < static_cast<int>(kToolInfo.size())) {
        return kToolInfo[tool].name;
    }
    return "Unknown";
}

wxString GetToolHotkey(int tool) {
    if (tool >= 0 && tool < static_cast<int>(kToolInfo.size())) {
        return kToolInfo[tool].hotkey;
    }
    return {};
}

MainFrame::MainFrame(const wxString& skinsPath)
    : wxFrame(nullptr, wxID_ANY, "PolyWorks", wxDefaultPosition, wxSize(800, 600)),
      m_skinsPath(skinsPath) {
    SetMinClientSize(wxSize(640, 480));
    DragAcceptFiles(true);

    buildMenuBar();
    buildStatusBar();

    m_viewport = new GlViewport(this, m_doc, m_undoStack);
    if (m_viewport != nullptr) {
        m_viewport->setSkinsPath(skinsPath.ToStdString());
    }

    /* VB6 read polyworks.ini at startup (modConfig.bas LoadConfig). */
    LoadPrefs();
    ApplyPrefs();

    auto* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(m_viewport, 1, wxEXPAND);
    SetSizer(sizer);

    Bind(wxEVT_MENU, &MainFrame::OnFileNew, this, wxID_NEW);
    Bind(wxEVT_MENU, &MainFrame::OnFileOpen, this, wxID_OPEN);
    Bind(wxEVT_MENU, &MainFrame::OnFileSave, this, wxID_SAVE);
    Bind(wxEVT_MENU, &MainFrame::OnFileSaveAs, this, wxID_SAVEAS);
    Bind(wxEVT_MENU, &MainFrame::OnFileCompile, this, ID_FILE_COMPILE);
    Bind(wxEVT_MENU, &MainFrame::OnEditUndo, this, wxID_UNDO);
    Bind(wxEVT_MENU, &MainFrame::OnEditRedo, this, wxID_REDO);
    Bind(wxEVT_MENU, &MainFrame::OnEditDuplicateSelected, this, ID_EDIT_DUPLICATE);
    Bind(wxEVT_MENU, &MainFrame::OnEditCopy,  this, wxID_COPY);
    Bind(wxEVT_MENU, &MainFrame::OnEditPaste, this, wxID_PASTE);
    Bind(wxEVT_MENU, &MainFrame::OnEditDeleteSelected, this, ID_EDIT_DELETE_SELECTED);
    Bind(wxEVT_MENU, &MainFrame::OnEditSelectAll, this, wxID_SELECTALL);
    Bind(wxEVT_MENU, &MainFrame::OnEditInvertSelection, this, ID_EDIT_INVERT_SELECTION);
    Bind(wxEVT_MENU, &MainFrame::OnEditSelectByColor, this, ID_EDIT_SELECT_BY_COLOR);
    Bind(wxEVT_MENU, &MainFrame::OnPolyOperation, this, ID_POLY_SPLIT_AT_VERTEX);
    Bind(wxEVT_MENU, &MainFrame::OnPolyOperation, this, ID_POLY_JOIN_VERTICES);
    Bind(wxEVT_MENU, &MainFrame::OnPolyOperation, this, ID_POLY_CREATE_WITH_SELECTED);
    Bind(wxEVT_MENU, &MainFrame::OnPolyOperation, this, ID_POLY_FIX_TEXTURE);
    Bind(wxEVT_MENU, &MainFrame::OnPolyOperation, this, ID_POLY_UNTEXTURE);
    Bind(wxEVT_MENU, &MainFrame::OnPolyOperation, this, ID_POLY_AVERAGE_COLORS);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { RefreshViewport(); }, wxID_REFRESH);
    Bind(wxEVT_MENU, &MainFrame::OnMapSettings, this, ID_MAP_SETTINGS);
    Bind(wxEVT_MENU, &MainFrame::OnPreferences, this, ID_MAP_PREFERENCES);
    Bind(wxEVT_MENU, &MainFrame::OnExit, this, wxID_EXIT);
    Bind(wxEVT_MENU, [this](wxCommandEvent& ev) {
        if (m_palettePanel != nullptr)
            m_palettePanel->Show(ev.IsChecked());
    }, ID_VIEW_PALETTE);
    /* Window menu */
    Bind(wxEVT_MENU, &MainFrame::OnWindowShowAll,       this, ID_WINDOW_SHOW_ALL);
    Bind(wxEVT_MENU, &MainFrame::OnWindowHideAll,       this, ID_WINDOW_HIDE_ALL);
    Bind(wxEVT_MENU, &MainFrame::OnWindowLoadWorkspace, this, ID_WINDOW_LOAD_WORKSPACE);
    Bind(wxEVT_MENU, &MainFrame::OnWindowSaveWorkspace, this, ID_WINDOW_SAVE_WORKSPACE);
    Bind(wxEVT_MENU, &MainFrame::OnWindowResetLayout,   this, ID_WINDOW_RESET_LAYOUT);
    Bind(wxEVT_MENU, &MainFrame::OnWindowTogglePanel,   this, ID_WINDOW_TOOLS);
    Bind(wxEVT_MENU, &MainFrame::OnWindowTogglePanel,   this, ID_WINDOW_DISPLAY);
    Bind(wxEVT_MENU, &MainFrame::OnWindowTogglePanel,   this, ID_WINDOW_PALETTE);
    Bind(wxEVT_MENU, &MainFrame::OnWindowTogglePanel,   this, ID_WINDOW_WAYPOINTS);
    Bind(wxEVT_MENU, &MainFrame::OnWindowTogglePanel,   this, ID_WINDOW_SCENERY);
    Bind(wxEVT_MENU, &MainFrame::OnWindowTogglePanel,   this, ID_WINDOW_PROPERTIES);
    Bind(wxEVT_MENU, &MainFrame::OnWindowTogglePanel,   this, ID_WINDOW_TEXTURE);
    Bind(wxEVT_CHAR_HOOK, &MainFrame::OnKeyDown, this);

    /* View layer toggles */
    auto bindLayerToggle = [this](int id) {
        Bind(wxEVT_MENU, &MainFrame::OnViewLayerToggle, this, id);
    };
    bindLayerToggle(ID_VIEW_POLYGONS);
    bindLayerToggle(ID_VIEW_WIREFRAME);
    bindLayerToggle(ID_VIEW_POINTS);
    bindLayerToggle(ID_VIEW_GRID);
    bindLayerToggle(ID_VIEW_OBJECTS);
    bindLayerToggle(ID_VIEW_WAYPOINTS);
    bindLayerToggle(ID_VIEW_LIGHTS);
    bindLayerToggle(ID_VIEW_SKETCH);
    bindLayerToggle(ID_VIEW_TEXTURES);
    bindLayerToggle(ID_VIEW_BACKGROUND);
    bindLayerToggle(ID_VIEW_SCENERY_BACK);
    bindLayerToggle(ID_VIEW_SCENERY_MIDDLE);
    bindLayerToggle(ID_VIEW_SCENERY_FRONT);
    bindLayerToggle(ID_VIEW_BLEND_WIREFRAME);
    bindLayerToggle(ID_VIEW_BLEND_POLYS);
    bindLayerToggle(ID_VIEW_SNAP_TO_GRID);
    bindLayerToggle(ID_VIEW_SNAP_TO_VERTS);

    /* Zoom / fit */
    Bind(wxEVT_MENU, &MainFrame::OnViewZoom, this, ID_VIEW_ZOOM_IN);
    Bind(wxEVT_MENU, &MainFrame::OnViewZoom, this, ID_VIEW_ZOOM_OUT);
    Bind(wxEVT_MENU, &MainFrame::OnViewZoom, this, ID_VIEW_ZOOM_RESET);
    Bind(wxEVT_MENU, &MainFrame::OnViewZoom, this, ID_VIEW_CENTER_RESET);
    Bind(wxEVT_MENU, &MainFrame::OnViewFitOnScreen, this, ID_VIEW_FIT_ON_SCREEN);

    /* Arrange */
    Bind(wxEVT_MENU, &MainFrame::OnArrangeSelected, this, ID_ARRANGE_BRING_TO_FRONT);
    Bind(wxEVT_MENU, &MainFrame::OnArrangeSelected, this, ID_ARRANGE_SEND_TO_BACK);
    Bind(wxEVT_MENU, &MainFrame::OnArrangeSelected, this, ID_ARRANGE_BRING_FORWARD);
    Bind(wxEVT_MENU, &MainFrame::OnArrangeSelected, this, ID_ARRANGE_SEND_BACKWARD);

    /* Deselect */
    Bind(wxEVT_MENU, [this](wxCommandEvent&) {
        m_doc.clearSelection(); RefreshViewport();
    }, ID_EDIT_DESELECT);

    /* Sever / Clear Sketch */
    Bind(wxEVT_MENU, &MainFrame::OnEditSeverConnections, this, ID_EDIT_SEVER_CONNECTIONS);
    Bind(wxEVT_MENU, &MainFrame::OnEditClearSketch,      this, ID_EDIT_CLEAR_SKETCH);

    /* Transform */
    auto bindTransform = [this](int id) {
        Bind(wxEVT_MENU, &MainFrame::OnEditTransform, this, id);
    };
    bindTransform(ID_EDIT_TRANSFORM_FLIP_H);
    bindTransform(ID_EDIT_TRANSFORM_FLIP_V);
    bindTransform(ID_EDIT_TRANSFORM_ROTATE_180);
    bindTransform(ID_EDIT_TRANSFORM_ROTATE_90CW);
    bindTransform(ID_EDIT_TRANSFORM_ROTATE_90CCW);

    /* Polygon menu: Fixed Texture toggle, Apply Light, Texture Transform */
    Bind(wxEVT_MENU, [this](wxCommandEvent& e) {
        m_doc.viewSettings.fixedTexture = e.IsChecked();
    }, ID_POLY_FIXED_TEXTURE);
    Bind(wxEVT_MENU, [this](wxCommandEvent& e) {
        m_customTexX = e.IsChecked();
    }, ID_POLY_CUSTOM_TEX_X);
    Bind(wxEVT_MENU, [this](wxCommandEvent& e) {
        m_customTexY = e.IsChecked();
    }, ID_POLY_CUSTOM_TEX_Y);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) {
        m_undoStack.push(m_doc);
        if (!m_doc.snapSelected(m_prefs.snapRadius)) m_undoStack.pop();
        RefreshViewport();
        UpdateTitle();
    }, ID_EDIT_SNAP_SELECTED);
    Bind(wxEVT_MENU, &MainFrame::OnPolyApplyLight, this, ID_POLY_APPLY_LIGHT);
    auto bindTexTransform = [this](int id) {
        Bind(wxEVT_MENU, &MainFrame::OnPolyTexTransform, this, id);
    };
    bindTexTransform(ID_POLY_TEX_FLIP_H);
    bindTexTransform(ID_POLY_TEX_FLIP_V);
    bindTexTransform(ID_POLY_TEX_ROTATE_180);
    bindTexTransform(ID_POLY_TEX_ROTATE_90CW);
    bindTexTransform(ID_POLY_TEX_ROTATE_90CCW);

    /* File: Open Compiled, Compile As, Export, Import, Run */
    Bind(wxEVT_MENU, &MainFrame::OnFileOpenCompiled, this, ID_FILE_OPEN_COMPILED);
    Bind(wxEVT_MENU, &MainFrame::OnFileCompileAs,    this, ID_FILE_COMPILE_AS);
    Bind(wxEVT_MENU, &MainFrame::OnFileExport,       this, ID_FILE_EXPORT);
    Bind(wxEVT_MENU, &MainFrame::OnFileImport,       this, ID_FILE_IMPORT);
    Bind(wxEVT_MENU, &MainFrame::OnFileRunSoldat,    this, ID_FILE_RUN_OPENSOLDAT);
    Bind(wxEVT_MENU, &MainFrame::OnFileRunSoldat,    this, ID_FILE_RUN_SOLDAT);
    Bind(wxEVT_SIZE, &MainFrame::OnSize, this);

    UpdateStatusBar();
    UpdateTitle();
    Centre();
}

void MainFrame::AttachToolsPanel(ToolsPanel* toolsPanel) {
    m_toolsPanel = toolsPanel;
    if (m_toolsPanel != nullptr) {
        m_toolsPanel->SetActiveTool(m_activeTool);
    }
}

void MainFrame::AttachDisplayPanel(DisplayPanel* displayPanel) {
    m_displayPanel = displayPanel;
}

void MainFrame::AttachInfoPanel(InfoPanel* infoPanel) {
    m_infoPanel = infoPanel;
}

void MainFrame::AttachSceneryPanel(SceneryPanel* sceneryPanel) {
    m_sceneryPanel = sceneryPanel;
}

void MainFrame::AttachWaypointPanel(WaypointPanel* waypointPanel) {
    m_waypointPanel = waypointPanel;
}

void MainFrame::AttachTexturePanel(TexturePanel* texturePanel) {
    m_texturePanel = texturePanel;
    RefreshTexturePanel();
}

void MainFrame::ReloadSceneryTextures() {
    /* mnuRefresh (frmScenery.frm:620) re-reads every scenery bitmap from disk
       so external edits show up without restarting the editor. */
    if (m_viewport == nullptr) return;
    m_viewport->GetTextureManager().clear();
    RefreshViewport();
}

void MainFrame::ClearUnusedScenery() {
    m_undoStack.push(m_doc);
    const int removed = m_doc.clearUnusedScenery();
    if (removed == 0) {
        m_undoStack.pop();
        wxMessageBox("No unused scenery entries were found.", "Clear Unused",
                     wxOK | wxICON_INFORMATION, this);
        return;
    }
    RefreshSceneryInUse();
    UpdateStatusBar();
    UpdateTitle();
    RefreshViewport();
}

void MainFrame::RefreshSceneryInUse() {
    if (m_sceneryPanel == nullptr) return;
    std::vector<std::string> used;
    for (const auto& s : m_doc.scenery) {
        if (s.style >= 1 && s.style < static_cast<int>(m_doc.sceneryNames.size())) {
            const std::string& n = m_doc.sceneryNames[static_cast<size_t>(s.style)];
            if (std::find(used.begin(), used.end(), n) == used.end())
                used.push_back(n);
        }
    }
    m_sceneryPanel->UpdateInUse(used);
}

void MainFrame::RefreshTexturePanel() {
    if (m_texturePanel == nullptr || m_viewport == nullptr) return;
    if (m_doc.options.textureName.empty()) return;
    const std::string path =
        m_viewport->GetTextureManager().resolvePath(m_doc.options.textureName);
    if (!path.empty())
        m_texturePanel->SetTexture(wxString::FromUTF8(path.c_str()));
}

bool MainFrame::GetTextureSelection(float& u1, float& v1,
                                    float& u2, float& v2) const {
    if (m_texturePanel == nullptr) return false;
    return m_texturePanel->GetSelection(u1, v1, u2, v2);
}

void MainFrame::AttachPalettePanel(PalettePanel* palettePanel) {
    m_palettePanel = palettePanel;

    /* Wire palette callbacks → viewport paint color sync */
    auto syncPaintColor = [this]() {
        if (!m_viewport || !m_palettePanel) return;
        uint8_t r, g, b;
        m_palettePanel->GetCurrentColor(r, g, b);
        m_viewport->setPaintColor(r, g, b,
            m_palettePanel->GetOpacity(),
            m_palettePanel->GetBlendMode(),
            static_cast<float>(m_palettePanel->GetRadius()));
    };

    palettePanel->onColorSelected = [syncPaintColor](uint8_t, uint8_t, uint8_t) {
        syncPaintColor();
    };
    palettePanel->onColorChanged = [syncPaintColor](uint8_t, uint8_t, uint8_t) {
        syncPaintColor();
    };
    palettePanel->onChannelChanged = [syncPaintColor](int, int) {
        syncPaintColor();
    };
    palettePanel->onBlendModeChanged = [syncPaintColor](int) {
        syncPaintColor();
    };
    palettePanel->onRadiusChanged = [syncPaintColor](int) {
        syncPaintColor();
    };
}

void MainFrame::GetPaintColor(uint8_t& r, uint8_t& g, uint8_t& b) const {
    if (m_palettePanel) m_palettePanel->GetCurrentColor(r, g, b);
    else { r = g = b = 255; }
}
float MainFrame::GetPaintOpacity() const {
    return m_palettePanel ? m_palettePanel->GetOpacity() : 1.0f;
}
int MainFrame::GetPaintBlendMode() const {
    return m_palettePanel ? m_palettePanel->GetBlendMode() : 0;
}
float MainFrame::GetPaintRadius() const {
    return m_palettePanel ? static_cast<float>(m_palettePanel->GetRadius()) : 8.0f;
}

int MainFrame::GetOrAddSelectedSceneryIndex() {
    if (m_sceneryPanel == nullptr) return 0;
    wxString selected = m_sceneryPanel->GetSelectedScenery();
    if (selected.IsEmpty()) return 0;

    std::string name = selected.ToStdString();
    /* sceneryNames is 1-based with [0] reserved as a sentinel
       (map_document.cpp:15), so the index *is* the vector position. */
    for (int i = 1; i < static_cast<int>(m_doc.sceneryNames.size()); ++i) {
        if (m_doc.sceneryNames[static_cast<size_t>(i)] == name)
            return i;
    }
    /* Add new name */
    m_doc.sceneryNames.push_back(name);
    return static_cast<int>(m_doc.sceneryNames.size()) - 1;
}

void MainFrame::buildMenuBar() {
    auto* menuBar = new wxMenuBar();

    auto* fileMenu = new wxMenu();
    fileMenu->Append(wxID_NEW, "&New\tCtrl+N");
    fileMenu->Append(wxID_OPEN, "&Open...\tCtrl+O");
    fileMenu->Append(ID_FILE_OPEN_COMPILED, "Open &Compiled...\tCtrl+Shift+O");
    fileMenu->AppendSeparator();

    m_recentMenu = new wxMenu();
    fileMenu->AppendSubMenu(m_recentMenu, "Open &Recent");
    LoadRecentFiles();
    RebuildRecentMenu();
    fileMenu->AppendSeparator();
    fileMenu->Append(wxID_SAVE, "&Save\tCtrl+S");
    fileMenu->Append(wxID_SAVEAS, "Save &As...\tCtrl+Shift+S");
    fileMenu->AppendSeparator();
    fileMenu->Append(ID_FILE_COMPILE, "&Compile to PMS\tF9");
    fileMenu->Append(ID_FILE_COMPILE_AS, "Compile to PMS &As...");
    fileMenu->AppendSeparator();
    fileMenu->Append(ID_FILE_EXPORT, "E&xport...");
    fileMenu->Append(ID_FILE_IMPORT, "&Import...");
    fileMenu->AppendSeparator();
    fileMenu->Append(ID_FILE_RUN_OPENSOLDAT, "Run &OpenSoldat");
    fileMenu->Append(ID_FILE_RUN_SOLDAT, "Run &Soldat");
    fileMenu->AppendSeparator();
    fileMenu->Append(wxID_EXIT, "E&xit");

    auto* editMenu = new wxMenu();
    editMenu->Append(wxID_UNDO, "&Undo\tCtrl+Z");
    editMenu->Append(wxID_REDO, "&Redo\tCtrl+Y");
    editMenu->AppendSeparator();
    editMenu->Append(ID_EDIT_DUPLICATE, "&Duplicate\tCtrl+D");
    editMenu->Append(wxID_COPY, "&Copy\tCtrl+C");
    editMenu->Append(wxID_PASTE, "&Paste\tCtrl+V");
    editMenu->Append(ID_EDIT_DELETE_SELECTED, "C&lear\tDel");
    editMenu->AppendSeparator();
    editMenu->Append(wxID_SELECTALL, "Select &All\tCtrl+A");
    editMenu->Append(ID_EDIT_DESELECT, "&Deselect\tEsc");
    editMenu->Append(ID_EDIT_INVERT_SELECTION, "&Invert Selection\tCtrl+I");
    editMenu->Append(ID_EDIT_SELECT_BY_COLOR, "Select by &Color\tCtrl+B");
    editMenu->AppendSeparator();
    auto* transformMenu = new wxMenu();
    transformMenu->Append(ID_EDIT_TRANSFORM_ROTATE_180,   "Rotate &180°");
    transformMenu->Append(ID_EDIT_TRANSFORM_ROTATE_90CW,  "Rotate 90° C&W");
    transformMenu->Append(ID_EDIT_TRANSFORM_ROTATE_90CCW, "Rotate 90° CC&W");
    transformMenu->AppendSeparator();
    transformMenu->Append(ID_EDIT_TRANSFORM_FLIP_H, "Flip &Horizontal");
    transformMenu->Append(ID_EDIT_TRANSFORM_FLIP_V, "Flip &Vertical");
    editMenu->AppendSubMenu(transformMenu, "&Transform");
    editMenu->AppendSeparator();
    editMenu->Append(ID_EDIT_SEVER_CONNECTIONS, "&Sever Connections\tBack");
    editMenu->Append(ID_EDIT_CLEAR_SKETCH, "Clear S&ketch");

    auto* viewMenu = new wxMenu();
    viewMenu->AppendCheckItem(ID_VIEW_POLYGONS,       "&Polygons")->Check(true);
    viewMenu->AppendCheckItem(ID_VIEW_WIREFRAME,      "&Wireframe");
    viewMenu->AppendCheckItem(ID_VIEW_POINTS,         "Poi&nts")->Check(true);
    viewMenu->AppendCheckItem(ID_VIEW_GRID,           "&Grid\tCtrl+'");
    viewMenu->AppendCheckItem(ID_VIEW_OBJECTS,        "&Objects")->Check(true);
    viewMenu->AppendCheckItem(ID_VIEW_WAYPOINTS,      "&Waypoints")->Check(true);
    viewMenu->AppendCheckItem(ID_VIEW_LIGHTS,         "&Lights")->Check(true);
    viewMenu->AppendCheckItem(ID_VIEW_SKETCH,         "S&ketch")->Check(true);
    viewMenu->AppendCheckItem(ID_VIEW_TEXTURES,       "&Textures")->Check(true);
    viewMenu->AppendCheckItem(ID_VIEW_BACKGROUND,     "&Background")->Check(true);
    viewMenu->AppendCheckItem(ID_VIEW_SCENERY_BACK,   "Scenery &Back")->Check(true);
    viewMenu->AppendCheckItem(ID_VIEW_SCENERY_MIDDLE, "Scenery &Middle")->Check(true);
    viewMenu->AppendCheckItem(ID_VIEW_SCENERY_FRONT,  "Scenery &Front")->Check(true);
    viewMenu->AppendSeparator();
    viewMenu->AppendCheckItem(ID_VIEW_BLEND_WIREFRAME, "Blend &Wireframe");
    viewMenu->AppendCheckItem(ID_VIEW_BLEND_POLYS,     "B&lend Polygons");
    viewMenu->AppendSeparator();
    viewMenu->AppendCheckItem(ID_VIEW_SNAP_TO_GRID,   "Snap to &Grid\tCtrl+Shift+G");
    viewMenu->AppendCheckItem(ID_VIEW_SNAP_TO_VERTS,  "Snap to &Vertices\tCtrl+Shift+V");
    viewMenu->AppendSeparator();
    viewMenu->AppendCheckItem(ID_VIEW_PALETTE, "Color &Palette");
    viewMenu->AppendSeparator();
    viewMenu->Append(ID_VIEW_ZOOM_IN,     "Zoom &In\tCtrl++");
    viewMenu->Append(ID_VIEW_ZOOM_OUT,    "Zoom &Out\tCtrl+-");
    viewMenu->Append(ID_VIEW_ZOOM_RESET,  "Zoom &100%\t*");
    viewMenu->Append(ID_VIEW_CENTER_RESET,"Center and &Reset\tCtrl+0");
    viewMenu->Append(ID_VIEW_FIT_ON_SCREEN, "&Fit on Screen\tCtrl+Shift+F");
    viewMenu->AppendSeparator();
    viewMenu->Append(wxID_REFRESH, "&Refresh\tF5");

    auto* mapMenu = new wxMenu();
    mapMenu->Append(ID_MAP_SETTINGS, "Map &Settings\tCtrl+M");
    mapMenu->Append(ID_MAP_PREFERENCES, "&Preferences\tCtrl+P");

    auto* polygonMenu = new wxMenu();
    auto* typeMenu = new wxMenu();
    for (int i = 0; i < POLY_TYPE_COUNT; ++i) {
        typeMenu->AppendRadioItem(ID_POLY_TYPE_BASE + i, polyTypeName(i));
    }
    typeMenu->Check(ID_POLY_TYPE_BASE + POLY_NORMAL, true);
    polygonMenu->AppendSubMenu(typeMenu, "&Type");
    polygonMenu->AppendSeparator();
    polygonMenu->Append(ID_POLY_SPLIT_AT_VERTEX,      "&Split at Vertex\tCtrl+L");
    polygonMenu->Append(ID_POLY_JOIN_VERTICES,         "&Join Vertices\tCtrl+J");
    polygonMenu->Append(ID_POLY_CREATE_WITH_SELECTED,  "&Create with Selected\tCtrl+E");
    /* mnuSnapSelected (frm:12354) runs the same snap that a drag-release does,
       without requiring the user to nudge the selection first. */
    polygonMenu->Append(ID_EDIT_SNAP_SELECTED, "Snap Selected &Vertices");
    polygonMenu->AppendSeparator();
    polygonMenu->Append(ID_POLY_FIX_TEXTURE,           "&Fix Texture\tCtrl+F");
    polygonMenu->Append(ID_POLY_UNTEXTURE,             "&Untexture\tCtrl+U");
    polygonMenu->AppendSeparator();
    polygonMenu->Append(ID_POLY_AVERAGE_COLORS,        "&Average Vertex Colors\tCtrl+G");
    polygonMenu->AppendSeparator();
    polygonMenu->AppendCheckItem(ID_POLY_FIXED_TEXTURE, "F&ixed Texture");
    /* mnuCustomX / mnuCustomY (frm:14519): when a Textured Quad is created,
       take its U (resp. V) range from the rectangle selected in the Texture
       window instead of from world coordinates. */
    polygonMenu->AppendCheckItem(ID_POLY_CUSTOM_TEX_X, "User Defined &X");
    polygonMenu->AppendCheckItem(ID_POLY_CUSTOM_TEX_Y, "User Defined &Y");
    polygonMenu->Append(ID_POLY_APPLY_LIGHT, "&Apply Light to Vertices");
    polygonMenu->AppendSeparator();
    auto* texTransMenu = new wxMenu();
    texTransMenu->Append(ID_POLY_TEX_ROTATE_180,   "Rotate &180°");
    texTransMenu->Append(ID_POLY_TEX_ROTATE_90CW,  "Rotate 90° C&W");
    texTransMenu->Append(ID_POLY_TEX_ROTATE_90CCW, "Rotate 90° CC&W");
    texTransMenu->AppendSeparator();
    texTransMenu->Append(ID_POLY_TEX_FLIP_H, "Flip &Horizontal");
    texTransMenu->Append(ID_POLY_TEX_FLIP_V, "Flip &Vertical");
    polygonMenu->AppendSubMenu(texTransMenu, "Texture &Transform");

    auto* arrangeMenu = new wxMenu();
    arrangeMenu->Append(ID_ARRANGE_BRING_TO_FRONT, "Bring to &Front\tHome");
    arrangeMenu->Append(ID_ARRANGE_SEND_TO_BACK, "Send to &Back\tEnd");
    arrangeMenu->Append(ID_ARRANGE_BRING_FORWARD, "Bring &Forward\tPgUp");
    arrangeMenu->Append(ID_ARRANGE_SEND_BACKWARD, "Send Back&ward\tPgDn");

    menuBar->Append(fileMenu, "&File");
    menuBar->Append(editMenu, "&Edit");
    menuBar->Append(viewMenu, "&View");
    menuBar->Append(mapMenu, "&Map");
    menuBar->Append(polygonMenu, "P&olygon");
    menuBar->Append(arrangeMenu, "&Arrange");

    /* Window menu — mirrors original VB6 "Window" menu (Index=4) */
    auto* windowMenu = new wxMenu();
    auto* workspaceMenu = new wxMenu();
    workspaceMenu->Append(ID_WINDOW_LOAD_WORKSPACE, "Load Workspace...");
    workspaceMenu->Append(ID_WINDOW_SAVE_WORKSPACE, "Save Workspace...");
    workspaceMenu->Append(ID_WINDOW_RESET_LAYOUT,   "Reset Window Locations");
    windowMenu->AppendSubMenu(workspaceMenu, "&Workspace");
    windowMenu->Append(ID_WINDOW_SHOW_ALL, "Show &All");
    windowMenu->Append(ID_WINDOW_HIDE_ALL, "Hide A&ll");
    windowMenu->AppendSeparator();
    m_winItemTools      = windowMenu->AppendCheckItem(ID_WINDOW_TOOLS,      "&Tools");
    m_winItemDisplay    = windowMenu->AppendCheckItem(ID_WINDOW_DISPLAY,    "&Display");
    m_winItemPalette    = windowMenu->AppendCheckItem(ID_WINDOW_PALETTE,    "P&alette");
    m_winItemWaypoints  = windowMenu->AppendCheckItem(ID_WINDOW_WAYPOINTS,  "&Waypoints");
    m_winItemScenery    = windowMenu->AppendCheckItem(ID_WINDOW_SCENERY,    "&Scenery");
    m_winItemProperties = windowMenu->AppendCheckItem(ID_WINDOW_PROPERTIES, "P&roperties");
    m_winItemTexture    = windowMenu->AppendCheckItem(ID_WINDOW_TEXTURE,    "&Texture");
    menuBar->Append(windowMenu, "&Window");

    SetMenuBar(menuBar);
}

void MainFrame::buildStatusBar() {
    m_statusBar = CreateStatusBar(4);
    const int widths[] = {170, -1, 100, 170};
    m_statusBar->SetStatusWidths(4, widths);

    m_positionText = new wxStaticText(m_statusBar, wxID_ANY, {});
    m_filenameText = new wxStaticText(m_statusBar, wxID_ANY, {});
    m_zoomText = new wxStaticText(m_statusBar, wxID_ANY, {});
    m_toolText = new wxStaticText(m_statusBar, wxID_ANY, {});

    layoutStatusBarFields();
}

void MainFrame::layoutStatusBarFields() {
    if (m_statusBar == nullptr) {
        return;
    }

    const std::array<wxStaticText*, 4> fields{{
        m_positionText,
        m_filenameText,
        m_zoomText,
        m_toolText,
    }};

    for (int i = 0; i < static_cast<int>(fields.size()); ++i) {
        wxRect rect;
        m_statusBar->GetFieldRect(i, rect);
        fields[i]->SetPosition(wxPoint(rect.x + 4, rect.y + 2));
        fields[i]->SetSize(wxSize(rect.width - 8, rect.height - 4));
    }
}

void MainFrame::OnFileNew(wxCommandEvent& event) {
    m_doc.clear();
    m_undoStack.clear();
    m_currentFilePath.clear();
    UpdateStatusBar();
    UpdateTitle();
    RefreshViewport();
}

void MainFrame::OnFileOpen(wxCommandEvent& event) {
    wxFileDialog dialog(this,
                        "Open PolyWorks Map",
                        wxEmptyString,
                        wxEmptyString,
                        "PolyWorks Map (*.pms)|*.pms|All files (*)|*",
                        wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dialog.ShowModal() != wxID_OK) {
        return;
    }

    LoadDocumentFromPath(dialog.GetPath());
}

/* Shared by File>Open, the recent-file list and command-line/file-association
   startup (VB6 Form_Load command-line branch, frm:10648-10670). */
/* Register the asset search paths for a freshly opened map.
 *
 * VB6 resolves textures from `OpenSoldatDir & "textures\"` and scenery from
 * `OpenSoldatDir & "Scenery-gfx\"` (frm:4284, frm:2092).  Maps normally live
 * in <soldat>/Maps/, so the parent of the .pms directory is the natural
 * equivalent when no Soldat directory has been configured.  The configured
 * directory from Preferences is added separately by ApplyPrefs().
 */
void MainFrame::RegisterAssetPathsForMap(const wxString& mapPath) {
    if (m_viewport == nullptr) {
        return;
    }

    wxFileName fn(mapPath);
    fn.Normalize();
    const wxString pmsDir = fn.GetPath();

    /* NB: wxFileName(pmsDir, wxEmptyString).GetPath() returns pmsDir itself,
       not its parent.  That bug silently disabled the <soldat>/Textures and
       <soldat>/Scenery-gfx fallbacks for maps in the usual <soldat>/Maps/
       location, so no real Soldat map ever found its textures. */
    wxFileName parent = wxFileName::DirName(pmsDir);
    if (parent.GetDirCount() > 0) {
        parent.RemoveLastDir();
    }
    const wxString parentDir = parent.GetPath();

    m_viewport->addTexturePath(pmsDir.ToStdString());
    m_viewport->addTexturePath((pmsDir + wxFILE_SEP_PATH + "Textures").ToStdString());
    m_viewport->addTexturePath((pmsDir + wxFILE_SEP_PATH + "Scenery-gfx").ToStdString());
    if (!parentDir.empty() && parentDir != pmsDir) {
        m_viewport->addTexturePath((parentDir + wxFILE_SEP_PATH + "Textures").ToStdString());
        m_viewport->addTexturePath((parentDir + wxFILE_SEP_PATH + "Scenery-gfx").ToStdString());
        m_viewport->addTexturePath(parentDir.ToStdString());
    }
}

bool MainFrame::LoadDocumentFromPath(const wxString& path) {
    PmsData data;
    std::string error;
    if (loadPmsFile(path.ToStdString(), data, error) != PmsLoadResult::OK) {
        wxMessageBox(wxString::FromUTF8(error.c_str()), "Open failed", wxOK | wxICON_ERROR, this);
        return false;
    }

    pmsDataToDoc(data, m_doc);

    /* VB6 LoadFile resets the view (frm:1935-1939): zoomFactor = 1 and
       scrollCoords(2) = -ScaleWidth/2, -ScaleHeight/2, which puts world origin
       at the centre of the viewport.  Soldat maps are built around the origin,
       so this frames the map; without it the map lands off the bottom-right.
       Must run *after* pmsDataToDoc, which calls MapDocument::clear() and
       would otherwise zero the scroll again. */
    m_doc.zoom = 1.0f;
    if (m_viewport != nullptr) {
        const wxSize vs = m_viewport->GetClientSize();
        m_doc.scrollX = -vs.GetWidth() / 2.0f;
        m_doc.scrollY = -vs.GetHeight() / 2.0f;
    } else {
        m_doc.scrollX = 0.0f;
        m_doc.scrollY = 0.0f;
    }
    m_doc.clearModified();
    m_doc.rebuildScreenCache();
    m_currentFilePath = path;
    RegisterAssetPathsForMap(path);
    AddToRecentFiles(path);
    m_undoStack.clear();
    UpdateStatusBar();
    UpdateTitle();
    RefreshTexturePanel();
    RefreshSceneryInUse();
    RefreshViewport();
    return true;
}

/* VB6 Form_Load resolves a command-line map name against, in order: the path
   as given, <appPath>/Maps/, then <OpenSoldatDir>/Maps/ (frm:10657-10668). */
bool MainFrame::OpenCommandLineMap(const wxString& arg) {
    wxString name = arg;
    /* VB6 strips one pair of surrounding quotes from Command$. */
    if (name.EndsWith("\"")) name = name.Mid(0, name.length() - 1);
    if (name.StartsWith("\"")) name = name.Mid(1);

    if (name.Lower().Right(4) != ".pms") return false;

    wxArrayString candidates;
    candidates.Add(name);
    const wxString appDir = wxFileName(wxStandardPaths::Get().GetExecutablePath()).GetPath();
    candidates.Add(appDir + wxFILE_SEP_PATH + "Maps" + wxFILE_SEP_PATH + name);
    if (!m_prefs.soldatDir.empty()) {
        candidates.Add(wxString::FromUTF8(m_prefs.soldatDir.c_str()) +
                       wxFILE_SEP_PATH + "Maps" + wxFILE_SEP_PATH + name);
    }

    for (const wxString& c : candidates) {
        if (!wxFileExists(c)) continue;
        wxBusyCursor busy;   /* VB6 sets vbHourglass around the load. */
        return LoadDocumentFromPath(c);
    }

    wxLogWarning("Could not find map \"%s\".", arg);
    return false;
}

bool MainFrame::SaveDocumentToPath(const wxString& path) {
    PmsData data;
    docToPmsData(m_doc, data);

    std::string error;
    if (!savePmsFile(path.ToStdString(), data, error)) {
        wxMessageBox(wxString::FromUTF8(error.c_str()), "Save failed", wxOK | wxICON_ERROR, this);
        return false;
    }

    m_currentFilePath = path;
    AddToRecentFiles(path);
    m_doc.clearModified();
    UpdateStatusBar();
    UpdateTitle();
    return true;
}

void MainFrame::OnFileSave(wxCommandEvent& event) {
    if (m_currentFilePath.empty()) {
        wxCommandEvent saveAsEvent;
        OnFileSaveAs(saveAsEvent);
        return;
    }

    SaveDocumentToPath(m_currentFilePath);
}

void MainFrame::OnFileSaveAs(wxCommandEvent& event) {
    wxFileDialog dialog(this,
                        "Save PolyWorks Map",
                        wxEmptyString,
                        BaseNameOrUntitled(m_currentFilePath),
                        "PolyWorks Map (*.pms)|*.pms|All files (*)|*",
                        wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dialog.ShowModal() != wxID_OK) {
        return;
    }

    SaveDocumentToPath(dialog.GetPath());
}

void MainFrame::OnFileCompile(wxCommandEvent& event) {
    wxFileDialog dialog(this,
                        "Compile PolyWorks Map",
                        wxEmptyString,
                        BaseNameOrUntitled(m_currentFilePath),
                        "PolyWorks Map (*.pms)|*.pms|All files (*)|*",
                        wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dialog.ShowModal() != wxID_OK) {
        return;
    }

    PmsData data;
    docToPmsData(m_doc, data);

    std::string error;
    if (!compilePms(dialog.GetPath().ToStdString(), data, error)) {
        wxMessageBox(wxString::FromUTF8(error.c_str()), "Compile failed", wxOK | wxICON_ERROR, this);
        return;
    }
}

void MainFrame::OnEditUndo(wxCommandEvent& event) {
    if (!m_undoStack.undo(m_doc)) {
        return;
    }

    m_doc.rebuildScreenCache();
    UpdateStatusBar();
    UpdateTitle();
    RefreshViewport();
}

void MainFrame::OnEditRedo(wxCommandEvent& event) {
    if (!m_undoStack.redo(m_doc)) {
        return;
    }

    m_doc.rebuildScreenCache();
    UpdateStatusBar();
    UpdateTitle();
    RefreshViewport();
}

void MainFrame::OnEditDeleteSelected(wxCommandEvent& event) {
    if (!m_doc.anySelected()) {
        return;
    }

    m_undoStack.push(m_doc);
    m_doc.deleteSelected();
    UpdateStatusBar();
    UpdateTitle();
    RefreshViewport();
}

void MainFrame::OnEditSelectAll(wxCommandEvent& event) {
    m_doc.selectAll();
    RefreshViewport();
}

void MainFrame::OnEditInvertSelection(wxCommandEvent& event) {
    m_doc.invertSelection();
    RefreshViewport();
}

void MainFrame::OnEditSelectByColor(wxCommandEvent& /*event*/) {
    /* Use the current palette color for selection.
       Matches VB6 Ctrl+B: select all vertices matching the active palette color. */
    uint8_t r = 255, g = 255, b = 255;
    GetPaintColor(r, g, b);
    m_doc.selectByColor(r, g, b);
    RefreshViewport();
}

void MainFrame::OnPolyOperation(wxCommandEvent& event) {
    if (!m_doc.anySelected()) return;

    m_undoStack.push(m_doc);
    const int id = event.GetId();

    if (id == ID_POLY_SPLIT_AT_VERTEX) {
        m_doc.splitAtVertex();
    } else if (id == ID_POLY_JOIN_VERTICES) {
        m_doc.joinSelectedVertices();
    } else if (id == ID_POLY_CREATE_WITH_SELECTED) {
        m_doc.createPolyFromSelected();
    } else if (id == ID_POLY_FIX_TEXTURE) {
        /* Use texture dimensions from the first selected polygon.
           If the texture is loaded in the viewport, query it; otherwise use a
           canonical Soldat default of 64×64. */
        float texW = 64.0f, texH = 64.0f;
        if (m_viewport) {
            auto dims = m_viewport->getTextureSize(m_doc);
            if (dims.first > 0) { texW = static_cast<float>(dims.first); texH = static_cast<float>(dims.second); }
        }
        m_doc.fixTextureOnSelected(texW, texH);
    } else if (id == ID_POLY_UNTEXTURE) {
        m_doc.untextureSelected();
    } else if (id == ID_POLY_AVERAGE_COLORS) {
        m_doc.averageVertexColors();
    } else {
        m_undoStack.pop();  /* nothing done */
        return;
    }

    m_doc.markModified();
    UpdateStatusBar();
    UpdateTitle();
    RefreshViewport();
}

void MainFrame::OnEditDuplicateSelected(wxCommandEvent& event) {
    if (!m_doc.anySelected()) {
        return;
    }

    m_undoStack.push(m_doc);
    m_doc.duplicateSelected(32.0f, 0.0f);  /* VB6 mnuDuplicate: +32 X only */
    UpdateStatusBar();
    UpdateTitle();
    RefreshViewport();
}

void MainFrame::OnEditSeverConnections(wxCommandEvent&) {
    m_undoStack.push(m_doc);
    m_doc.severWaypointConnections();
    m_doc.markModified();
    RefreshViewport();
}

void MainFrame::OnEditClearSketch(wxCommandEvent&) {
    m_undoStack.push(m_doc);
    m_doc.clearSketch();
    m_doc.markModified();
    UpdateStatusBar();
    RefreshViewport();
}

void MainFrame::OnEditTransform(wxCommandEvent& event) {
    if (!m_doc.anySelected()) return;
    m_undoStack.push(m_doc);
    const int id = event.GetId();
    if (id == ID_EDIT_TRANSFORM_FLIP_H) {
        m_doc.flipSelected(true, false);
    } else if (id == ID_EDIT_TRANSFORM_FLIP_V) {
        m_doc.flipSelected(false, true);
    } else if (id == ID_EDIT_TRANSFORM_ROTATE_180) {
        m_doc.rotateSelected(180.0f);
    } else if (id == ID_EDIT_TRANSFORM_ROTATE_90CW) {
        m_doc.rotateSelected(90.0f);
    } else if (id == ID_EDIT_TRANSFORM_ROTATE_90CCW) {
        m_doc.rotateSelected(-90.0f);
    } else {
        m_undoStack.pop(); return;
    }
    m_doc.markModified();
    RefreshViewport();
}

void MainFrame::OnPolyApplyLight(wxCommandEvent&) {
    m_undoStack.push(m_doc);
    m_doc.applyLightsToBaseColors();
    m_doc.markModified();
    RefreshViewport();
}

void MainFrame::OnPolyTexTransform(wxCommandEvent& event) {
    if (!m_doc.anySelected()) return;
    m_undoStack.push(m_doc);
    const int id = event.GetId();
    float texW = 64.0f, texH = 64.0f;
    if (m_viewport) {
        auto dims = m_viewport->getTextureSize(m_doc);
        if (dims.first > 0) { texW = static_cast<float>(dims.first); texH = static_cast<float>(dims.second); }
    }
    const float aspect = (texH > 0) ? texW / texH : 1.0f;
    if (id == ID_POLY_TEX_FLIP_H) {
        m_doc.flipTextureOnSelected(true);
    } else if (id == ID_POLY_TEX_FLIP_V) {
        m_doc.flipTextureOnSelected(false);
    } else if (id == ID_POLY_TEX_ROTATE_180) {
        m_doc.rotateTextureOnSelected(180.0f, aspect);
    } else if (id == ID_POLY_TEX_ROTATE_90CW) {
        m_doc.rotateTextureOnSelected(90.0f, aspect);
    } else if (id == ID_POLY_TEX_ROTATE_90CCW) {
        m_doc.rotateTextureOnSelected(-90.0f, aspect);
    } else {
        m_undoStack.pop(); return;
    }
    m_doc.markModified();
    RefreshViewport();
}

void MainFrame::OnViewLayerToggle(wxCommandEvent& event) {
    const int id = event.GetId();
    const bool v = event.IsChecked();
    ViewSettings& vs = m_doc.viewSettings;
    if      (id == ID_VIEW_POLYGONS)       vs.showPolys = v;
    else if (id == ID_VIEW_WIREFRAME)      vs.showWireframe = v;
    else if (id == ID_VIEW_POINTS)         vs.showPoints = v;
    else if (id == ID_VIEW_GRID)           vs.showGrid = v;
    else if (id == ID_VIEW_OBJECTS)        vs.showObjects = v;
    else if (id == ID_VIEW_WAYPOINTS)      vs.showWaypoints = v;
    else if (id == ID_VIEW_LIGHTS)         vs.showLights = v;
    else if (id == ID_VIEW_SKETCH)         vs.showSketch = v;
    else if (id == ID_VIEW_TEXTURES)       vs.showTexture = v;
    else if (id == ID_VIEW_BACKGROUND)     vs.showBackground = v;
    else if (id == ID_VIEW_SCENERY_BACK)   vs.showSceneryBack = v;
    else if (id == ID_VIEW_SCENERY_MIDDLE) vs.showSceneryMiddle = v;
    else if (id == ID_VIEW_SCENERY_FRONT)  vs.showSceneryFront = v;
    else if (id == ID_VIEW_BLEND_WIREFRAME) vs.blendWireframe = v;
    else if (id == ID_VIEW_BLEND_POLYS)    vs.blendPolys = v;
    else if (id == ID_VIEW_SNAP_TO_GRID)   vs.snapToGrid = v;
    else if (id == ID_VIEW_SNAP_TO_VERTS)  vs.snapToVertices = v;
    RefreshViewport();
}

void MainFrame::OnViewZoom(wxCommandEvent& event) {
    if (!m_viewport) return;
    wxSize sz = m_viewport->GetClientSize();
    const float cx = sz.GetWidth() * 0.5f;
    const float cy = sz.GetHeight() * 0.5f;
    const int id = event.GetId();
    if (id == ID_VIEW_ZOOM_IN) {
        m_doc.setZoom(snapZoom(m_doc.zoom, 1), cx, cy);
    } else if (id == ID_VIEW_ZOOM_OUT) {
        m_doc.setZoom(snapZoom(m_doc.zoom, -1), cx, cy);
    } else if (id == ID_VIEW_ZOOM_RESET) {
        m_doc.setZoom(1.0f, cx, cy);
    } else if (id == ID_VIEW_CENTER_RESET) {
        m_doc.zoom = 1.0f;
        m_doc.scrollX = 0;
        m_doc.scrollY = 0;
        m_doc.rebuildScreenCache();
    }
    UpdateStatusBar();
    RefreshViewport();
}

void MainFrame::OnViewFitOnScreen(wxCommandEvent&) {
    if (!m_viewport) return;
    wxSize sz = m_viewport->GetClientSize();
    m_doc.fitToViewport(static_cast<float>(sz.GetWidth()),
                        static_cast<float>(sz.GetHeight()));
    UpdateStatusBar();
    RefreshViewport();
}

void MainFrame::OnArrangeSelected(wxCommandEvent& event) {
    if (!m_doc.anySelected()) return;
    m_undoStack.push(m_doc);
    const int id = event.GetId();
    if (id == ID_ARRANGE_BRING_TO_FRONT) {
        m_doc.bringSelectedToFront();
    } else if (id == ID_ARRANGE_SEND_TO_BACK) {
        m_doc.sendSelectedToBack();
    } else if (id == ID_ARRANGE_BRING_FORWARD) {
        m_doc.bringSelectedForward();
    } else if (id == ID_ARRANGE_SEND_BACKWARD) {
        m_doc.sendSelectedBackward();
    } else {
        m_undoStack.pop(); return;
    }
    m_doc.markModified();
    RefreshViewport();
}

void MainFrame::OnFileOpenCompiled(wxCommandEvent&) {
    /* Compiled PMS uses the same binary format as native PMS; open identically. */
    wxFileDialog dialog(this,
                        "Open Compiled PMS",
                        wxEmptyString,
                        wxEmptyString,
                        "PolyWorks Map (*.pms)|*.pms|All files (*)|*",
                        wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dialog.ShowModal() != wxID_OK) return;

    PmsData data;
    std::string error;
    if (loadPmsFile(dialog.GetPath().ToStdString(), data, error) != PmsLoadResult::OK) {
        wxMessageBox(wxString::FromUTF8(error.c_str()), "Open failed", wxOK | wxICON_ERROR, this);
        return;
    }
    pmsDataToDoc(data, m_doc);

    /* VB6 LoadFile resets the view (frm:1935-1939): zoomFactor = 1 and
       scrollCoords(2) = -ScaleWidth/2, -ScaleHeight/2, which puts world origin
       at the centre of the viewport.  Soldat maps are built around the origin,
       so this frames the map; without it the map lands off the bottom-right.
       Must run *after* pmsDataToDoc, which calls MapDocument::clear() and
       would otherwise zero the scroll again. */
    m_doc.zoom = 1.0f;
    if (m_viewport != nullptr) {
        const wxSize vs = m_viewport->GetClientSize();
        m_doc.scrollX = -vs.GetWidth() / 2.0f;
        m_doc.scrollY = -vs.GetHeight() / 2.0f;
    } else {
        m_doc.scrollX = 0.0f;
        m_doc.scrollY = 0.0f;
    }
    m_doc.clearModified();
    m_doc.rebuildScreenCache();
    m_currentFilePath = dialog.GetPath();
    RegisterAssetPathsForMap(dialog.GetPath());
    m_undoStack.clear();
    UpdateStatusBar();
    UpdateTitle();
    RefreshViewport();
}

void MainFrame::OnFileCompileAs(wxCommandEvent&) {
    wxFileDialog dialog(this,
                        "Compile PMS As",
                        wxEmptyString,
                        BaseNameOrUntitled(m_currentFilePath),
                        "PolyWorks Map (*.pms)|*.pms|All files (*)|*",
                        wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dialog.ShowModal() != wxID_OK) return;

    PmsData data;
    docToPmsData(m_doc, data);
    std::string error;
    if (!compilePms(dialog.GetPath().ToStdString(), data, error))
        wxMessageBox(wxString::FromUTF8(error.c_str()), "Compile failed", wxOK | wxICON_ERROR, this);
}

wxString MainFrame::ClipboardPrefabPath() const {
    /* The original wrote the clipboard prefab to <app>\Temp\copy.PFB
       (frm:11769 / frm:12062).  The install directory is read-only on modern
       platforms, so use the per-user temp directory instead. */
    return wxFileName::GetTempDir() + wxFileName::GetPathSeparator() +
           "polyworks-copy.PFB";
}

void MainFrame::OnEditCopy(wxCommandEvent&) {
    if (!m_doc.anySelected()) return;
    std::string error;
    if (!savePrefab(ClipboardPrefabPath().ToStdString(), m_doc, error))
        wxMessageBox(wxString::FromUTF8(error.c_str()), "Copy failed",
                     wxOK | wxICON_ERROR, this);
}

void MainFrame::OnEditPaste(wxCommandEvent&) {
    const wxString path = ClipboardPrefabPath();
    if (!wxFileExists(path)) return;
    m_undoStack.push(m_doc);
    std::string error;
    if (!loadPrefab(path.ToStdString(), m_doc, error)) {
        m_undoStack.pop();
        wxMessageBox(wxString::FromUTF8(error.c_str()), "Paste failed",
                     wxOK | wxICON_ERROR, this);
        return;
    }
    m_doc.markModified();
    UpdateStatusBar();
    UpdateTitle();
    RefreshViewport();
}

void MainFrame::OnFileExport(wxCommandEvent&) {
    if (!m_doc.anySelected()) {
        wxMessageBox("Select objects to export as a prefab.", "Export Prefab",
                     wxOK | wxICON_INFORMATION, this);
        return;
    }
    wxFileDialog dialog(this,
                        "Export Prefab",
                        wxString(m_prefs.prefabsDir),
                        wxEmptyString,
                        "PolyWorks Prefab (*.pwf)|*.pwf|All files (*)|*",
                        wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dialog.ShowModal() != wxID_OK) return;

    std::string error;
    if (!savePrefab(dialog.GetPath().ToStdString(), m_doc, error))
        wxMessageBox(wxString::FromUTF8(error.c_str()), "Export failed", wxOK | wxICON_ERROR, this);
}

void MainFrame::OnFileImport(wxCommandEvent&) {
    wxFileDialog dialog(this,
                        "Import Prefab",
                        wxString(m_prefs.prefabsDir),
                        wxEmptyString,
                        "PolyWorks Prefab (*.pwf)|*.pwf|All files (*)|*",
                        wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dialog.ShowModal() != wxID_OK) return;

    m_undoStack.push(m_doc);
    std::string error;
    if (!loadPrefab(dialog.GetPath().ToStdString(), m_doc, error)) {
        m_undoStack.pop();
        wxMessageBox(wxString::FromUTF8(error.c_str()), "Import failed", wxOK | wxICON_ERROR, this);
        return;
    }
    UpdateStatusBar();
    UpdateTitle();
    RefreshViewport();
}

void MainFrame::OnFileRunSoldat(wxCommandEvent& event) {
    /* Launch Soldat or OpenSoldat if a path has been configured via Preferences. */
    /* For now, show a message pointing to Preferences if nothing is configured. */
    const wxString key = (event.GetId() == ID_FILE_RUN_OPENSOLDAT)
        ? "/Soldat/OpenSoldatExe" : "/Soldat/SoldatExe";
    const bool wantOpen = (event.GetId() == ID_FILE_RUN_OPENSOLDAT);
    wxConfig cfg("PolyWorks");
    wxString exe;
    cfg.Read(key, &exe);
    if (exe.IsEmpty() && !m_prefs.soldatDir.empty()) {
        /* The original launched <game dir>\soldat.exe directly; fall back to
           the configured game directory rather than requiring a second
           setting. */
        const char* names[] = {"soldat.exe", "Soldat.exe", "soldat"};
        const char* openNames[] = {"opensoldat.exe", "OpenSoldat.exe", "opensoldat"};
        for (const char* n : (wantOpen ? openNames : names)) {
            const wxString candidate = wxString(m_prefs.soldatDir) +
                                       wxFileName::GetPathSeparator() + n;
            if (wxFileExists(candidate)) { exe = candidate; break; }
        }
    }
    if (exe.IsEmpty()) {
        wxMessageBox(
            "No Soldat executable found.\n"
            "Set the game directory in Edit > Preferences.",
            "Run Soldat", wxOK | wxICON_INFORMATION, this);
        return;
    }
    if (!wxFileExists(exe)) {
        wxMessageBox(
            wxString::Format("Executable not found:\n%s\n\nCheck the path in Edit > Preferences.", exe),
            "Run Soldat", wxOK | wxICON_ERROR, this);
        return;
    }
    wxExecute(wxString::Format("\"%s\"", exe));
}

void MainFrame::OnExit(wxCommandEvent& event) {
    Close(true);
}

/* ---- Window menu helpers ----------------------------------------------- */

namespace {
void SetPanelVisible(wxWindow* panel, wxMenuItem* menuItem, bool visible) {
    if (panel != nullptr) panel->Show(visible);
    if (menuItem != nullptr) menuItem->Check(visible);
}
bool PanelVisible(wxWindow* panel) {
    return panel != nullptr && panel->IsShown();
}
}  // namespace

void MainFrame::OnWindowShowAll(wxCommandEvent&) {
    SetPanelVisible(m_toolsPanel,     m_winItemTools,      true);
    SetPanelVisible(m_displayPanel,   m_winItemDisplay,    true);
    SetPanelVisible(m_palettePanel,   m_winItemPalette,    true);
    SetPanelVisible(m_waypointPanel,  m_winItemWaypoints,  true);
    SetPanelVisible(m_sceneryPanel,   m_winItemScenery,    true);
    SetPanelVisible(m_infoPanel,      m_winItemProperties, true);
    SetPanelVisible(m_texturePanel,   m_winItemTexture,    true);
}

void MainFrame::OnWindowHideAll(wxCommandEvent&) {
    SetPanelVisible(m_toolsPanel,     m_winItemTools,      false);
    SetPanelVisible(m_displayPanel,   m_winItemDisplay,    false);
    SetPanelVisible(m_palettePanel,   m_winItemPalette,    false);
    SetPanelVisible(m_waypointPanel,  m_winItemWaypoints,  false);
    SetPanelVisible(m_sceneryPanel,   m_winItemScenery,    false);
    SetPanelVisible(m_infoPanel,      m_winItemProperties, false);
    SetPanelVisible(m_texturePanel,   m_winItemTexture,    false);
}

void MainFrame::OnWindowTogglePanel(wxCommandEvent& event) {
    const int id = event.GetId();
    if (id == ID_WINDOW_TOOLS) {
        bool vis = !PanelVisible(m_toolsPanel);
        SetPanelVisible(m_toolsPanel, m_winItemTools, vis);
    } else if (id == ID_WINDOW_DISPLAY) {
        bool vis = !PanelVisible(m_displayPanel);
        SetPanelVisible(m_displayPanel, m_winItemDisplay, vis);
    } else if (id == ID_WINDOW_PALETTE) {
        bool vis = !PanelVisible(m_palettePanel);
        SetPanelVisible(m_palettePanel, m_winItemPalette, vis);
    } else if (id == ID_WINDOW_WAYPOINTS) {
        bool vis = !PanelVisible(m_waypointPanel);
        SetPanelVisible(m_waypointPanel, m_winItemWaypoints, vis);
    } else if (id == ID_WINDOW_SCENERY) {
        bool vis = !PanelVisible(m_sceneryPanel);
        SetPanelVisible(m_sceneryPanel, m_winItemScenery, vis);
    } else if (id == ID_WINDOW_PROPERTIES) {
        bool vis = !PanelVisible(m_infoPanel);
        SetPanelVisible(m_infoPanel, m_winItemProperties, vis);
    } else if (id == ID_WINDOW_TEXTURE) {
        bool vis = !PanelVisible(m_texturePanel);
        SetPanelVisible(m_texturePanel, m_winItemTexture, vis);
    }
}

void MainFrame::OnWindowLoadWorkspace(wxCommandEvent&) {
    wxFileDialog dlg(this, "Load Workspace", wxEmptyString, wxEmptyString,
                     "Workspace (*.ini)|*.ini|All files (*)|*", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() != wxID_OK) return;
    wxConfig cfg("PolyWorks", wxEmptyString, dlg.GetPath());
    if (m_toolsPanel    && cfg.HasEntry("/Tools/x"))
        m_toolsPanel->Move(cfg.ReadLong("/Tools/x", 0), cfg.ReadLong("/Tools/y", 0));
    if (m_displayPanel  && cfg.HasEntry("/Display/x"))
        m_displayPanel->Move(cfg.ReadLong("/Display/x", 0), cfg.ReadLong("/Display/y", 0));
    if (m_sceneryPanel  && cfg.HasEntry("/Scenery/x"))
        m_sceneryPanel->Move(cfg.ReadLong("/Scenery/x", 0), cfg.ReadLong("/Scenery/y", 0));
    if (m_waypointPanel && cfg.HasEntry("/Waypoints/x"))
        m_waypointPanel->Move(cfg.ReadLong("/Waypoints/x", 0), cfg.ReadLong("/Waypoints/y", 0));
    if (m_infoPanel     && cfg.HasEntry("/Info/x"))
        m_infoPanel->Move(cfg.ReadLong("/Info/x", 0), cfg.ReadLong("/Info/y", 0));
    if (m_palettePanel  && cfg.HasEntry("/Palette/x"))
        m_palettePanel->Move(cfg.ReadLong("/Palette/x", 0), cfg.ReadLong("/Palette/y", 0));
}

void MainFrame::OnWindowSaveWorkspace(wxCommandEvent&) {
    wxFileDialog dlg(this, "Save Workspace", wxEmptyString, "workspace.ini",
                     "Workspace (*.ini)|*.ini|All files (*)|*",
                     wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dlg.ShowModal() != wxID_OK) return;
    wxConfig cfg("PolyWorks", wxEmptyString, dlg.GetPath());
    auto savePos = [&](const char* group, wxWindow* w) {
        if (w == nullptr) return;
        wxPoint p = w->GetPosition();
        cfg.Write(wxString::Format("/%s/x", group), (long)p.x);
        cfg.Write(wxString::Format("/%s/y", group), (long)p.y);
    };
    savePos("Tools",     m_toolsPanel);
    savePos("Display",   m_displayPanel);
    savePos("Scenery",   m_sceneryPanel);
    savePos("Waypoints", m_waypointPanel);
    savePos("Info",      m_infoPanel);
    savePos("Palette",   m_palettePanel);
    cfg.Flush();
}

void MainFrame::OnWindowResetLayout(wxCommandEvent&) {
    /* Reset all panels to their default positions relative to main window */
    const wxPoint base = GetScreenPosition();
    const wxSize  size = GetSize();
    if (m_toolsPanel)    m_toolsPanel->Move(base.x - 75,              base.y + 40);
    if (m_displayPanel)  m_displayPanel->Move(base.x + size.x + 5,    base.y + 40);
    if (m_sceneryPanel)  m_sceneryPanel->Move(base.x + size.x + 5,    base.y + 200);
    if (m_waypointPanel) m_waypointPanel->Move(base.x + size.x + 5,   base.y + 380);
    if (m_infoPanel)     m_infoPanel->Move(base.x - 75,               base.y + 300);
    if (m_palettePanel)  m_palettePanel->Move(base.x - 75,            base.y + 200);
}

void MainFrame::OnMapSettings(wxCommandEvent&) {
    MapSettingsDlg dlg(this, m_doc.options, m_skinsPath.ToStdString());
    if (dlg.ShowModal() == wxID_OK) {
        m_doc.markModified();
        UpdateTitle();
        RefreshTexturePanel();
        RefreshViewport();
    }
}

void MainFrame::LoadRecentFiles() {
    wxConfig cfg("PolyWorks", "PolyWorks");
    m_recentFiles.Clear();
    for (int i = 0; i < kMaxRecentFiles; ++i) {
        wxString s;
        if (!cfg.Read(wxString::Format("Recent/File%d", i), &s) || s.empty())
            break;
        m_recentFiles.Add(s);
    }
}

void MainFrame::SaveRecentFiles() {
    wxConfig cfg("PolyWorks", "PolyWorks");
    for (int i = 0; i < kMaxRecentFiles; ++i) {
        const wxString key = wxString::Format("Recent/File%d", i);
        if (i < static_cast<int>(m_recentFiles.GetCount()))
            cfg.Write(key, m_recentFiles[static_cast<size_t>(i)]);
        else
            cfg.DeleteEntry(key);
    }
    cfg.Flush();
}

void MainFrame::RebuildRecentMenu() {
    if (m_recentMenu == nullptr) return;
    while (m_recentMenu->GetMenuItemCount() > 0)
        m_recentMenu->Delete(m_recentMenu->FindItemByPosition(0));

    if (m_recentFiles.IsEmpty()) {
        wxMenuItem* empty = m_recentMenu->Append(wxID_ANY, "(empty)");
        empty->Enable(false);
        return;
    }

    for (size_t i = 0; i < m_recentFiles.GetCount(); ++i) {
        const wxString path = m_recentFiles[i];
        /* VB6 showed the bare file name; keep the full path as the tooltip so
           two maps with the same name stay distinguishable. */
        wxMenuItem* item = m_recentMenu->Append(
            wxID_ANY,
            wxString::Format("&%d %s", static_cast<int>(i) + 1,
                             wxFileName(path).GetFullName()),
            path);
        m_recentMenu->Bind(wxEVT_MENU, [this, path](wxCommandEvent&) {
            if (!wxFileExists(path)) {
                wxMessageBox(wxString::Format(
                                 "The map no longer exists:\n%s", path),
                             "PolyWorks", wxOK | wxICON_WARNING, this);
                m_recentFiles.Remove(path);
                RebuildRecentMenu();
                SaveRecentFiles();
                return;
            }
            LoadDocumentFromPath(path);
        }, item->GetId());
    }
}

void MainFrame::AddToRecentFiles(const wxString& path) {
    if (path.empty()) return;
    const wxString full = wxFileName(path).GetFullPath();
    m_recentFiles.Remove(full);
    m_recentFiles.Insert(full, 0);
    while (static_cast<int>(m_recentFiles.GetCount()) > kMaxRecentFiles)
        m_recentFiles.RemoveAt(m_recentFiles.GetCount() - 1);
    RebuildRecentMenu();
    SaveRecentFiles();
}

void MainFrame::SetPaintColorFromPicker(uint8_t r, uint8_t g, uint8_t b) {
    /* VB6 pickers push the absorbed colour into frmPalette via SetValues and
       then reuse it as the active paint colour (frm:7745-7752). */
    if (m_palettePanel != nullptr)
        m_palettePanel->SetValues(r, g, b);
    if (m_viewport != nullptr) {
        m_viewport->setPaintColor(r, g, b, GetPaintOpacity(),
                                  GetPaintBlendMode(), GetPaintRadius());
    }
}

void MainFrame::LoadPrefs() {
    wxConfig cfg("PolyWorks", "PolyWorks");
    double d = 0;
    long   l = 0;
    if (cfg.Read("Zoom/Min",   &d)) m_prefs.minZoom   = static_cast<float>(d);
    if (cfg.Read("Zoom/Max",   &d)) m_prefs.maxZoom   = static_cast<float>(d);
    if (cfg.Read("Zoom/Reset", &d)) m_prefs.resetZoom = static_cast<float>(d);
    if (cfg.Read("Grid/Spacing",   &l)) m_prefs.gridSpacing   = static_cast<int>(l);
    if (cfg.Read("Grid/Divisions", &l)) m_prefs.gridDivisions = static_cast<int>(l);
    if (cfg.Read("Grid/Color1", &l)) m_prefs.gridColor1 = static_cast<unsigned>(l);
    if (cfg.Read("Grid/Color2", &l)) m_prefs.gridColor2 = static_cast<unsigned>(l);
    if (cfg.Read("Snap/Enabled", &l)) m_prefs.snapEnabled = (l != 0);
    if (cfg.Read("Snap/Radius",  &d)) m_prefs.snapRadius  = static_cast<float>(d);
    if (cfg.Read("Undo/Depth",   &l)) m_prefs.undoDepth   = static_cast<int>(l);
    if (cfg.Read("Grid/Alpha1", &d)) m_prefs.gridAlpha1 = static_cast<float>(d);
    if (cfg.Read("Grid/Alpha2", &d)) m_prefs.gridAlpha2 = static_cast<float>(d);
    if (cfg.Read("Blend/PolySrc",  &l)) m_prefs.polyBlendSrc  = static_cast<int>(l);
    if (cfg.Read("Blend/PolyDest", &l)) m_prefs.polyBlendDest = static_cast<int>(l);
    if (cfg.Read("Blend/WireSrc",  &l)) m_prefs.wireBlendSrc  = static_cast<int>(l);
    if (cfg.Read("Blend/WireDest", &l)) m_prefs.wireBlendDest = static_cast<int>(l);
    if (cfg.Read("Colors/Point",     &l)) m_prefs.pointColor     = static_cast<unsigned>(l);
    if (cfg.Read("Colors/Selection", &l)) m_prefs.selectionColor = static_cast<unsigned>(l);

    wxString s;
    if (cfg.Read("Paths/SoldatDir",  &s)) m_prefs.soldatDir  = s.ToStdString();
    if (cfg.Read("Paths/PrefabsDir", &s)) m_prefs.prefabsDir = s.ToStdString();
    if (cfg.Read("Paths/UncompDir",  &s)) m_prefs.uncompDir  = s.ToStdString();

    /* VB6 modOSME.bas fell back to a well-known install location when the
       configured game directory was absent. */
    if (m_prefs.soldatDir.empty()) {
        const char* candidates[] = {
            "/usr/share/soldat", "/usr/local/share/soldat",
            "/usr/share/opensoldat", "C:\\OpenSoldat", "C:\\Soldat"
        };
        for (const char* c : candidates)
            if (wxDirExists(c)) { m_prefs.soldatDir = c; break; }
    }
}

void MainFrame::SavePrefs() {
    wxConfig cfg("PolyWorks", "PolyWorks");
    cfg.Write("Zoom/Min",   static_cast<double>(m_prefs.minZoom));
    cfg.Write("Zoom/Max",   static_cast<double>(m_prefs.maxZoom));
    cfg.Write("Zoom/Reset", static_cast<double>(m_prefs.resetZoom));
    cfg.Write("Grid/Spacing",   static_cast<long>(m_prefs.gridSpacing));
    cfg.Write("Grid/Divisions", static_cast<long>(m_prefs.gridDivisions));
    cfg.Write("Grid/Color1", static_cast<long>(m_prefs.gridColor1));
    cfg.Write("Grid/Color2", static_cast<long>(m_prefs.gridColor2));
    cfg.Write("Snap/Enabled", static_cast<long>(m_prefs.snapEnabled ? 1 : 0));
    cfg.Write("Snap/Radius",  static_cast<double>(m_prefs.snapRadius));
    cfg.Write("Undo/Depth",   static_cast<long>(m_prefs.undoDepth));
    cfg.Write("Grid/Alpha1", static_cast<double>(m_prefs.gridAlpha1));
    cfg.Write("Grid/Alpha2", static_cast<double>(m_prefs.gridAlpha2));
    cfg.Write("Blend/PolySrc",  static_cast<long>(m_prefs.polyBlendSrc));
    cfg.Write("Blend/PolyDest", static_cast<long>(m_prefs.polyBlendDest));
    cfg.Write("Blend/WireSrc",  static_cast<long>(m_prefs.wireBlendSrc));
    cfg.Write("Blend/WireDest", static_cast<long>(m_prefs.wireBlendDest));
    cfg.Write("Colors/Point",     static_cast<long>(m_prefs.pointColor));
    cfg.Write("Colors/Selection", static_cast<long>(m_prefs.selectionColor));
    cfg.Write("Paths/SoldatDir",  wxString(m_prefs.soldatDir));
    cfg.Write("Paths/PrefabsDir", wxString(m_prefs.prefabsDir));
    cfg.Write("Paths/UncompDir",  wxString(m_prefs.uncompDir));
    cfg.Flush();
}

void MainFrame::ApplyPrefs() {
    m_undoStack.setMaxDepth(m_prefs.undoDepth);
    m_doc.viewSettings.gridSize      = static_cast<float>(m_prefs.gridSpacing);
    m_doc.viewSettings.gridDivisions = m_prefs.gridDivisions;
    m_doc.viewSettings.gridColor1    = m_prefs.gridColor1;
    m_doc.viewSettings.gridColor2    = m_prefs.gridColor2;
    m_doc.viewSettings.gridAlpha1    = m_prefs.gridAlpha1;
    m_doc.viewSettings.gridAlpha2    = m_prefs.gridAlpha2;
    m_doc.viewSettings.polyBlendSrc  = m_prefs.polyBlendSrc;
    m_doc.viewSettings.polyBlendDest = m_prefs.polyBlendDest;
    m_doc.viewSettings.wireBlendSrc  = m_prefs.wireBlendSrc;
    m_doc.viewSettings.wireBlendDest = m_prefs.wireBlendDest;
    if (m_viewport != nullptr) {
        m_viewport->setSnapRadius(m_prefs.snapRadius);
        if (!m_prefs.soldatDir.empty()) {
            auto& tm = m_viewport->GetTextureManager();
            tm.addSearchPath(m_prefs.soldatDir + "/Textures");
            tm.addSearchPath(m_prefs.soldatDir + "/Scenery-gfx");
        }
    }
}

void MainFrame::OnPreferences(wxCommandEvent&) {
    PreferencesDlg dlg(this, m_prefs);
    if (dlg.ShowModal() != wxID_OK) return;
    ApplyPrefs();
    SavePrefs();
    RefreshViewport();
}

void MainFrame::ToggleWaypointTypeKey(int idx) {
    SetWaypointType(idx, !GetWaypointType(idx));
    UpdateStatusBar();
}

void MainFrame::ToggleLayerById(int id) {
    wxMenuBar* mb = GetMenuBar();
    if (mb == nullptr) return;
    wxMenuItem* item = mb->FindItem(id);
    if (item == nullptr || !item->IsCheckable()) return;
    item->Check(!item->IsChecked());
    wxCommandEvent evt(wxEVT_MENU, id);
    evt.SetInt(item->IsChecked() ? 1 : 0);
    OnViewLayerToggle(evt);
}

void MainFrame::OnKeyDown(wxKeyEvent& event) {
    const int mods = event.GetModifiers();
    const int key  = event.GetKeyCode();

    if (mods == wxMOD_NONE) {
        switch (key) {
        /* Tool shortcuts */
        case 'A': SetActiveTool(0); return;
        case 'Q': SetActiveTool(1); return;
        case 'S': SetActiveTool(2); return;
        case 'W': SetActiveTool(3); return;
        case 'D': SetActiveTool(4); return;
        case 'E': SetActiveTool(5); return;
        case 'F': SetActiveTool(6); return;
        case 'R': SetActiveTool(7); return;
        case 'G': SetActiveTool(8); return;
        case 'T': SetActiveTool(9); return;
        case 'H': SetActiveTool(10); return;
        case 'Y': SetActiveTool(11); return;  /* Sketch (DIK_Y = 21) */
        case 'J': SetActiveTool(12); return;
        case 'U': SetActiveTool(13); return;

        /* Waypoint direction keys (modConfig.bas:179-183 -- DIK scancodes
           J/K/I/M/N).  The original checked tool hotkeys first, so 'J' is
           consumed by the Lights tool and never reaches the waypoint
           handler; that ordering is preserved above. */
        case 'K': ToggleWaypointTypeKey(1); return;
        case 'I': ToggleWaypointTypeKey(2); return;
        case 'M': ToggleWaypointTypeKey(3); return;
        case 'N': ToggleWaypointTypeKey(4); return;

        /* Display-layer keys (modConfig.bas:186-193 -- numpad 1..8). */
        case WXK_NUMPAD1: ToggleLayerById(ID_VIEW_BACKGROUND);     return;
        case WXK_NUMPAD2: ToggleLayerById(ID_VIEW_POLYGONS);       return;
        case WXK_NUMPAD3: ToggleLayerById(ID_VIEW_TEXTURES);       return;
        case WXK_NUMPAD4: ToggleLayerById(ID_VIEW_WIREFRAME);      return;
        case WXK_NUMPAD5: ToggleLayerById(ID_VIEW_POINTS);         return;
        case WXK_NUMPAD6: ToggleLayerById(ID_VIEW_SCENERY_MIDDLE); return;
        case WXK_NUMPAD7: ToggleLayerById(ID_VIEW_OBJECTS);        return;
        case WXK_NUMPAD8: ToggleLayerById(ID_VIEW_WAYPOINTS);      return;

        /* Delete selected */
        case WXK_DELETE:
            m_undoStack.push(m_doc);
            m_doc.deleteSelected();
            m_doc.markModified();
            UpdateStatusBar();
            UpdateTitle();
            RefreshViewport();
            return;

        /* Backspace = Sever waypoint connections (matches original PolyWorks) */
        case WXK_BACK:
            m_undoStack.push(m_doc);
            m_doc.severWaypointConnections();
            m_doc.markModified();
            RefreshViewport();
            return;

        /* Nudge the selection.  VB6 (frm:11007-11010, 11031-11040) moves by
           one world unit, or by one grid sub-division with Shift held. */
        case WXK_LEFT:  NudgeSelection(-1.0f,  0.0f, false); return;
        case WXK_RIGHT: NudgeSelection( 1.0f,  0.0f, false); return;
        case WXK_UP:    NudgeSelection( 0.0f, -1.0f, false); return;
        case WXK_DOWN:  NudgeSelection( 0.0f,  1.0f, false); return;

        default: break;
        }
    }

    if (mods == wxMOD_SHIFT) {
        switch (key) {
        case WXK_LEFT:  NudgeSelection(-1.0f,  0.0f, true); return;
        case WXK_RIGHT: NudgeSelection( 1.0f,  0.0f, true); return;
        case WXK_UP:    NudgeSelection( 0.0f, -1.0f, true); return;
        case WXK_DOWN:  NudgeSelection( 0.0f,  1.0f, true); return;
        default: break;
        }
    }

    event.Skip();
}

void MainFrame::NudgeSelection(float dirX, float dirY, bool shift) {
    float step = 1.0f;
    if (shift) {
        const int div = m_prefs.gridDivisions > 0 ? m_prefs.gridDivisions : 1;
        step = static_cast<float>(m_prefs.gridSpacing) / static_cast<float>(div);
        if (step <= 0.0f) step = 1.0f;
    }
    m_undoStack.push(m_doc);
    m_doc.moveSelected(dirX * step, dirY * step);
    m_doc.markModified();
    UpdateStatusBar();
    UpdateTitle();
    RefreshViewport();
}

void MainFrame::OnSize(wxSizeEvent& event) {
    layoutStatusBarFields();
    event.Skip();
}

void MainFrame::SetActiveTool(int tool) {
    if (tool < 0 || tool >= static_cast<int>(kToolInfo.size())) {
        return;
    }

    m_activeTool = tool;
    if (m_toolsPanel != nullptr) {
        m_toolsPanel->SetActiveTool(tool);
    }
    if (m_viewport != nullptr) {
        m_viewport->setActiveTool(tool);
    }
    UpdateStatusBar();
    RefreshViewport();
}

void MainFrame::UpdateStatusBar() {
    if (m_positionText == nullptr) {
        return;
    }

    m_positionText->SetLabel(wxString::Format("Pos: %.1f, %.1f", m_lastMouseWorld.x, m_lastMouseWorld.y));
    m_filenameText->SetLabel("File: " + BaseNameOrUntitled(m_currentFilePath));
    m_zoomText->SetLabel(wxString::Format("Zoom: %.0f%%", m_doc.zoom * 100.0f));
    /* VB6 labels the *effective* function, then appends the polygon type when
       creating and the waypoint direction when placing waypoints
       (frm:4256-4266). */
    int fn = (m_viewport != nullptr) ? m_viewport->GetCurrentFunction()
                                     : m_activeTool;
    wxString toolLabel = (fn >= 0 && fn < static_cast<int>(kFunctionNames.size()))
                             ? wxString(kFunctionNames[static_cast<size_t>(fn)])
                             : GetToolName(m_activeTool);
    if (m_activeTool == 1) {
        toolLabel += wxString::Format(" (%s)", polyTypeName(m_creationPolyType));
    } else if (m_activeTool == 8) {
        static const char* const kWayNames[5] = {"Left", "Right", "Up",
                                                 "Down", "Fly"};
        for (int i = 0; i < 5; ++i)
            if (m_waypointType[i]) toolLabel += wxString::Format(" (%s)", kWayNames[i]);
    }
    m_toolText->SetLabel("Tool: " + toolLabel);
    layoutStatusBarFields();

    if (m_infoPanel != nullptr)
        m_infoPanel->Refresh();
}

void MainFrame::UpdateTitle() {
    const wxString modifiedMarker = m_doc.modified ? "*" : "";
    SetTitle(BaseNameOrUntitled(m_currentFilePath) + modifiedMarker + " - PolyWorks");
}

void MainFrame::UpdateMouseWorldPosition(const Vec2& world) {
    m_lastMouseWorld = world;
    UpdateStatusBar();
}

void MainFrame::RefreshViewport() {
    if (m_viewport != nullptr) {
        m_viewport->Refresh(false);
    }
}
