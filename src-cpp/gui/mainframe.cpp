#include "mainframe.h"

#include "gl_viewport.h"
#include "panels/tools_panel.h"
#include "panels/display_panel.h"
#include "panels/info_panel.h"
#include "panels/scenery_panel.h"
#include "panels/waypoint_panel.h"
#include "dialogs/map_settings_dlg.h"
#include "dialogs/preferences_dlg.h"
#include "pms_io.h"

#include <wx/filedlg.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/filename.h>
#include <wx/sizer.h>

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
    ID_POLY_TYPE_BASE
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

constexpr std::array<const char*, 26> kPolyTypeNames{{
    "Normal",
    "Only Bullets",
    "Only Players",
    "No Collide",
    "Ice",
    "Deadly",
    "Bloody Deadly",
    "Hurts",
    "Regenerates",
    "Lava",
    "Alpha Bullets",
    "Alpha Players",
    "Bravo Bullets",
    "Bravo Players",
    "Charlie Bullets",
    "Charlie Players",
    "Delta Bullets",
    "Delta Players",
    "Bouncy",
    "Explosive",
    "Hit Multiply",
    "Collider",
    "No Pass",
    "Shift",
    "Weather",
    "No Footsteps",
}};

wxMenu* CreatePlaceholderMenu(const wxString& label) {
    auto* menu = new wxMenu();
    const int itemId = wxWindow::NewControlId();
    menu->Append(itemId, label);
    menu->Enable(itemId, false);
    return menu;
}

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
    Bind(wxEVT_MENU, &MainFrame::OnEditDeleteSelected, this, ID_EDIT_DELETE_SELECTED);
    Bind(wxEVT_MENU, &MainFrame::OnEditSelectAll, this, wxID_SELECTALL);
    Bind(wxEVT_MENU, &MainFrame::OnEditInvertSelection, this, ID_EDIT_INVERT_SELECTION);
    Bind(wxEVT_MENU, &MainFrame::OnMapSettings, this, ID_MAP_SETTINGS);
    Bind(wxEVT_MENU, &MainFrame::OnPreferences, this, ID_MAP_PREFERENCES);
    Bind(wxEVT_MENU, &MainFrame::OnExit, this, wxID_EXIT);
    Bind(wxEVT_CHAR_HOOK, &MainFrame::OnKeyDown, this);
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

int MainFrame::GetOrAddSelectedSceneryIndex() {
    if (m_sceneryPanel == nullptr) return 0;
    wxString selected = m_sceneryPanel->GetSelectedScenery();
    if (selected.IsEmpty()) return 0;

    std::string name = selected.ToStdString();
    /* Search existing names (1-based) */
    for (int i = 0; i < static_cast<int>(m_doc.sceneryNames.size()); ++i) {
        if (m_doc.sceneryNames[i] == name)
            return i + 1;
    }
    /* Add new name */
    m_doc.sceneryNames.push_back(name);
    return static_cast<int>(m_doc.sceneryNames.size());
}

void MainFrame::buildMenuBar() {
    auto* menuBar = new wxMenuBar();

    auto* fileMenu = new wxMenu();
    fileMenu->Append(wxID_NEW, "&New\tCtrl+N");
    fileMenu->Append(wxID_OPEN, "&Open...\tCtrl+O");
    fileMenu->Append(ID_FILE_OPEN_COMPILED, "Open &Compiled...\tCtrl+Shift+O");
    fileMenu->AppendSeparator();

    auto* recentMenu = new wxMenu();
    const int recentPlaceholderId = wxWindow::NewControlId();
    recentMenu->Append(recentPlaceholderId, "(empty)");
    recentMenu->Enable(recentPlaceholderId, false);
    fileMenu->AppendSubMenu(recentMenu, "Open &Recent");
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
    editMenu->Append(wxID_COPY, "&Copy");
    editMenu->Append(wxID_PASTE, "&Paste");
    editMenu->Append(ID_EDIT_DELETE_SELECTED, "C&lear\tDel");
    editMenu->AppendSeparator();
    editMenu->Append(wxID_SELECTALL, "Select &All\tCtrl+A");
    editMenu->Append(ID_EDIT_DESELECT, "&Deselect\tEsc");
    editMenu->Append(ID_EDIT_INVERT_SELECTION, "&Invert Selection\tCtrl+I");

    auto* viewMenu = new wxMenu();
    viewMenu->AppendCheckItem(wxWindow::NewControlId(), "&Polygons")->Check(true);
    viewMenu->AppendCheckItem(wxWindow::NewControlId(), "&Wireframe");
    viewMenu->AppendCheckItem(wxWindow::NewControlId(), "Poi&nts")->Check(true);
    viewMenu->AppendCheckItem(ID_VIEW_GRID, "&Grid\tCtrl+'");
    viewMenu->AppendCheckItem(wxWindow::NewControlId(), "&Objects")->Check(true);
    viewMenu->AppendCheckItem(wxWindow::NewControlId(), "&Waypoints")->Check(true);
    viewMenu->AppendCheckItem(wxWindow::NewControlId(), "&Lights")->Check(true);
    viewMenu->AppendCheckItem(wxWindow::NewControlId(), "S&ketch")->Check(true);
    viewMenu->AppendCheckItem(wxWindow::NewControlId(), "&Textures")->Check(true);
    viewMenu->AppendCheckItem(wxWindow::NewControlId(), "&Background")->Check(true);
    viewMenu->AppendCheckItem(wxWindow::NewControlId(), "Scenery &Back")->Check(true);
    viewMenu->AppendCheckItem(wxWindow::NewControlId(), "Scenery &Middle")->Check(true);
    viewMenu->AppendCheckItem(wxWindow::NewControlId(), "Scenery &Front")->Check(true);
    viewMenu->AppendSeparator();
    viewMenu->Append(ID_VIEW_ZOOM_IN, "Zoom &In\tCtrl++");
    viewMenu->Append(ID_VIEW_ZOOM_OUT, "Zoom &Out\tCtrl+-");
    viewMenu->Append(ID_VIEW_ZOOM_RESET, "Zoom &100%\t*");
    viewMenu->Append(ID_VIEW_CENTER_RESET, "Center and &Reset\tCtrl+0");

    auto* mapMenu = new wxMenu();
    mapMenu->Append(ID_MAP_SETTINGS, "Map &Settings\tCtrl+M");
    mapMenu->Append(ID_MAP_PREFERENCES, "&Preferences\tCtrl+P");

    auto* polygonMenu = new wxMenu();
    auto* typeMenu = new wxMenu();
    for (size_t i = 0; i < kPolyTypeNames.size(); ++i) {
        typeMenu->AppendRadioItem(ID_POLY_TYPE_BASE + static_cast<int>(i), kPolyTypeNames[i]);
    }
    typeMenu->Check(ID_POLY_TYPE_BASE + POLY_NORMAL, true);
    polygonMenu->AppendSubMenu(typeMenu, "&Type");

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
    menuBar->Append(CreatePlaceholderMenu("(coming soon)"), "&Objects");
    menuBar->Append(CreatePlaceholderMenu("(coming soon)"), "&Waypoints");
    menuBar->Append(CreatePlaceholderMenu("(coming soon)"), "&Scenery");
    menuBar->Append(CreatePlaceholderMenu("(coming soon)"), "Scenery&Tree");
    menuBar->Append(CreatePlaceholderMenu("(coming soon)"), "&VertexSelect");

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

    PmsData data;
    std::string error;
    if (loadPmsFile(dialog.GetPath().ToStdString(), data, error) != PmsLoadResult::OK) {
        wxMessageBox(wxString::FromUTF8(error.c_str()), "Open failed", wxOK | wxICON_ERROR, this);
        return;
    }

    pmsDataToDoc(data, m_doc);
    m_doc.clearModified();
    m_doc.rebuildScreenCache();
    m_currentFilePath = dialog.GetPath();
    if (m_viewport != nullptr) {
        m_viewport->addTexturePath(wxFileName(dialog.GetPath()).GetPath().ToStdString());
    }
    m_undoStack.clear();
    UpdateStatusBar();
    UpdateTitle();
    RefreshViewport();
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

void MainFrame::OnEditDuplicateSelected(wxCommandEvent& event) {
    if (!m_doc.anySelected()) {
        return;
    }

    m_undoStack.push(m_doc);
    m_doc.duplicateSelected(10.0f, 10.0f);
    UpdateStatusBar();
    UpdateTitle();
    RefreshViewport();
}

void MainFrame::OnExit(wxCommandEvent& event) {
    Close(true);
}

void MainFrame::OnMapSettings(wxCommandEvent&) {
    MapSettingsDlg dlg(this, m_doc.options, m_skinsPath.ToStdString());
    if (dlg.ShowModal() == wxID_OK) {
        m_doc.markModified();
        UpdateTitle();
        RefreshViewport();
    }
}

void MainFrame::OnPreferences(wxCommandEvent&) {
    /* Preferences dialog — uses a local AppPrefs struct for now; later persisted to ini */
    AppPrefs prefs;
    PreferencesDlg dlg(this, prefs);
    dlg.ShowModal();
    /* TODO: apply prefs.undoDepth to m_undoStack, propagate grid settings to renderer */
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
        case 'Z': SetActiveTool(11); return;
        case 'J': SetActiveTool(12); return;
        case 'U': SetActiveTool(13); return;

        /* Delete selected */
        case WXK_DELETE:
        case WXK_BACK:
            m_undoStack.push(m_doc);
            m_doc.deleteSelected();
            m_doc.markModified();
            UpdateStatusBar();
            UpdateTitle();
            RefreshViewport();
            return;

        /* Nudge selected 1 world-unit in arrow direction */
        case WXK_LEFT:  m_undoStack.push(m_doc); m_doc.moveSelected(-1.0f,  0.0f); RefreshViewport(); return;
        case WXK_RIGHT: m_undoStack.push(m_doc); m_doc.moveSelected( 1.0f,  0.0f); RefreshViewport(); return;
        case WXK_UP:    m_undoStack.push(m_doc); m_doc.moveSelected( 0.0f, -1.0f); RefreshViewport(); return;
        case WXK_DOWN:  m_undoStack.push(m_doc); m_doc.moveSelected( 0.0f,  1.0f); RefreshViewport(); return;

        default: break;
        }
    }

    /* Shift+arrow: nudge 10 world units */
    if (mods == wxMOD_SHIFT) {
        switch (key) {
        case WXK_LEFT:  m_undoStack.push(m_doc); m_doc.moveSelected(-10.0f,   0.0f); RefreshViewport(); return;
        case WXK_RIGHT: m_undoStack.push(m_doc); m_doc.moveSelected( 10.0f,   0.0f); RefreshViewport(); return;
        case WXK_UP:    m_undoStack.push(m_doc); m_doc.moveSelected(  0.0f, -10.0f); RefreshViewport(); return;
        case WXK_DOWN:  m_undoStack.push(m_doc); m_doc.moveSelected(  0.0f,  10.0f); RefreshViewport(); return;
        default: break;
        }
    }

    event.Skip();
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
    m_toolText->SetLabel("Tool: " + GetToolName(m_activeTool));
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
