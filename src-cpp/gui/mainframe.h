#pragma once

#include "map_document.h"
#include "undo_stack.h"
#include "dialogs/preferences_dlg.h"

#include <wx/frame.h>
#include <wx/confbase.h>
#include <wx/statusbr.h>
#include <wx/stattext.h>

#include <memory>

class GlViewport;
class ToolsPanel;
class DisplayPanel;
class InfoPanel;
class TexturePanel;
class SceneryPanel;
class WaypointPanel;
class PalettePanel;
class wxCommandEvent;
class wxKeyEvent;
class wxSizeEvent;

wxString GetToolName(int tool);
wxString GetToolHotkey(int tool);

/* Put a floating tool window at `pos`, nudged so it stays on the display the
   main window is on.  The VB6 layout offsets assume a large desktop; without
   this a panel can land completely off a smaller screen and look missing. */
void PlacePanelOnScreen(wxWindow* panel, const wxPoint& pos);

class MainFrame final : public wxFrame {
public:
    explicit MainFrame(const wxString& skinsPath);

    void AttachToolsPanel(ToolsPanel* toolsPanel);
    void AttachDisplayPanel(DisplayPanel* displayPanel);
    void AttachInfoPanel(InfoPanel* infoPanel);
    void AttachTexturePanel(TexturePanel* texturePanel);
    void RefreshTexturePanel();
    /* frmScenery context menu actions (frmScenery.frm:601-628). */
    void ReloadSceneryTextures();
    void ClearUnusedScenery();
    /* Push the map's referenced scenery names into the Scenery panel. */
    void RefreshSceneryInUse();
    void AttachSceneryPanel(SceneryPanel* sceneryPanel);
    void AttachWaypointPanel(WaypointPanel* waypointPanel);
    /* Make the Window menu ticks agree with the panels that are on screen. */
    void SyncWindowMenu();
    void AttachPalettePanel(PalettePanel* palettePanel);
    void SetActiveTool(int tool);
    int GetActiveTool() const { return m_activeTool; }
    void SetCurrentSpawnTeam(int team) { m_currentSpawnTeam = team; }
    int GetCurrentSpawnTeam() const { return m_currentSpawnTeam; }
    void SetSceneryRotate(bool v) { m_sceneryRotate = v; }
    bool GetSceneryRotate() const { return m_sceneryRotate; }
    void SetSceneryScale(bool v) { m_sceneryScale = v; }
    bool GetSceneryScale() const { return m_sceneryScale; }
    void SetSceneryLevel(int v) { m_sceneryLevel = v; }
    int GetSceneryLevel() const { return m_sceneryLevel; }

    /* Polygon type applied to newly created polygons (VB6 `polyType`,
       set from the mnuPolyTypes context menu). */
    void SetCreationPolyType(int t) { m_creationPolyType = t; }
    int  GetCreationPolyType() const { return m_creationPolyType; }

    /* Movement flags stamped onto newly created waypoints
       (VB6 mnuWayType, frm:636-655).  Index 0..4 = Left/Right/Up/Down/Fly. */
    wxString ClipboardPrefabPath() const;
    void OnEditCopy(wxCommandEvent&);
    void OnEditPaste(wxCommandEvent&);
    void ToggleWaypointTypeKey(int idx);
    void ToggleLayerById(int id);
    void SetWaypointType(int idx, bool on) {
        if (idx >= 0 && idx < 5) m_waypointType[idx] = on;
    }
    bool GetWaypointType(int idx) const {
        return (idx >= 0 && idx < 5) ? m_waypointType[idx] : false;
    }

    /* Applies a picked colour to the palette, exactly as the VB6 pickers do
       via frmPalette.SetValues. */
    void SetPaintColorFromPicker(uint8_t r, uint8_t g, uint8_t b);

    /* Adds `path` to the most-recently-used list and rebuilds the menu. */
    void AddToRecentFiles(const wxString& path);
    void UpdateStatusBar();
    void UpdateTitle();
    void UpdateMouseWorldPosition(const Vec2& world);
    void RefreshViewport();

    /* Palette state getters — used by the viewport for color painting tools */
    void GetPaintColor(uint8_t& r, uint8_t& g, uint8_t& b) const;
    float GetPaintOpacity() const;
    int   GetPaintBlendMode() const;
    float GetPaintRadius() const;
    int   GetOrAddSelectedSceneryIndex();

    MapDocument m_doc;
    UndoStack m_undoStack;
    int m_activeTool = 0;
    GlViewport*    m_viewport      = nullptr;
    DisplayPanel*  m_displayPanel  = nullptr;
    InfoPanel*     m_infoPanel     = nullptr;
    TexturePanel*  m_texturePanel  = nullptr;
    SceneryPanel*  m_sceneryPanel  = nullptr;
    WaypointPanel* m_waypointPanel = nullptr;
    PalettePanel*  m_palettePanel  = nullptr;

private:
    void buildMenuBar();
    void buildStatusBar();
    void layoutStatusBarFields();

    void OnFileNew(wxCommandEvent& event);
    void OnFileOpen(wxCommandEvent& event);
public:
    /* Load a .pms into the document, replacing its contents. */
    bool LoadDocumentFromPath(const wxString& path);
    /* Register texture/scenery search paths derived from a map's location. */
    void RegisterAssetPathsForMap(const wxString& mapPath);
    /* Register the search paths that are relative to the executable, so a
       portable (extract-and-run) installation resolves its own assets. */
    void RegisterAppAssetPaths();
    /* The directory containing the running executable (VB6 `appPath`). */
    static wxString AppDir();
    /* Settings store: <appPath>/polyworks.ini when writable (portable), else
       the platform per-user store. */
    static std::unique_ptr<wxConfigBase> OpenConfig();
    /* Resolve and open a map named on the command line (file association). */
    bool OpenCommandLineMap(const wxString& arg);
    /* VB6 `prompt` guard: offer to save before discarding a modified map.
       Returns false when the user cancelled the pending action. */
    bool ConfirmDiscardChanges();
private:
    void OnFileOpenCompiled(wxCommandEvent& event);
    void OnFileSave(wxCommandEvent& event);
    void OnFileSaveAs(wxCommandEvent& event);
    void OnFileCompile(wxCommandEvent& event);
    void OnFileCompileAs(wxCommandEvent& event);
    void OnFileExport(wxCommandEvent& event);
    void OnFileImport(wxCommandEvent& event);
    void OnFileRunSoldat(wxCommandEvent& event);

    void OnEditUndo(wxCommandEvent& event);
    void OnEditRedo(wxCommandEvent& event);
    void OnEditDeleteSelected(wxCommandEvent& event);
    void OnEditSelectAll(wxCommandEvent& event);
    void OnEditInvertSelection(wxCommandEvent& event);
    void OnEditDuplicateSelected(wxCommandEvent& event);
    void OnEditSelectByColor(wxCommandEvent& event);
    void OnEditSeverConnections(wxCommandEvent& event);
    void OnEditClearSketch(wxCommandEvent& event);
    void OnEditTransform(wxCommandEvent& event);
    void OnPolyOperation(wxCommandEvent& event);
    void OnPolyApplyLight(wxCommandEvent& event);
    void OnPolyTexTransform(wxCommandEvent& event);

    void OnViewLayerToggle(wxCommandEvent& event);
    void OnViewZoom(wxCommandEvent& event);
    void OnViewFitOnScreen(wxCommandEvent& event);
    void OnArrangeSelected(wxCommandEvent& event);

    void OnMapSettings(wxCommandEvent& event);
    void OnPreferences(wxCommandEvent& event);

    void OnWindowShowAll(wxCommandEvent& event);
    void OnWindowHideAll(wxCommandEvent& event);
    void OnWindowTogglePanel(wxCommandEvent& event);
    void OnCloseWindow(wxCloseEvent& event);
    void OnWindowLoadWorkspace(wxCommandEvent& event);
    void OnWindowSaveWorkspace(wxCommandEvent& event);
    void OnWindowResetLayout(wxCommandEvent& event);

    void OnExit(wxCommandEvent& event);
    void OnKeyDown(wxKeyEvent& event);
    void NudgeSelection(float dirX, float dirY, bool shift);
    void OnSize(wxSizeEvent& event);

    bool SaveDocumentToPath(const wxString& path);

    /* Preferences persistence (VB6 modConfig.bas wrote polyworks.ini). */
    void LoadPrefs();
    void SavePrefs();
    void ApplyPrefs();

    AppPrefs m_prefs;
    /* Game directory the texture search paths and scenery list were last
       built from, so ApplyPrefs() can tell when it has actually changed. */
    std::string m_appliedSoldatDir;

    wxString m_skinsPath;
    wxString m_currentFilePath;
    ToolsPanel* m_toolsPanel = nullptr;
    Vec2 m_lastMouseWorld{};

    /* Objects tool state */
    int m_currentSpawnTeam = 0;   /* currently selected spawn type (0 = general) */

    /* Scenery tool state (persisted across placements) */
    bool m_sceneryRotate = false;
    bool m_sceneryScale  = false;
    int  m_sceneryLevel  = 0;     /* 0=Back, 1=Middle, 2=Front */

    /* Polygon-creation state (VB6 `polyType`) */
    int  m_creationPolyType = 0;
    /* mnuCustomX / mnuCustomY: source quad UVs from the Texture window. */
    bool m_customTexX = false;
    bool m_customTexY = false;

public:
    bool GetCustomTexX() const { return m_customTexX; }
    bool GetCustomTexY() const { return m_customTexY; }
    /* Normalised rectangle currently selected in the Texture window. */
    bool GetTextureSelection(float& u1, float& v1, float& u2, float& v2) const;

private:

    /* Waypoint-creation state (VB6 mnuWayType checked flags) */
    bool m_waypointType[5] = {false, false, false, false, false};

    /* Open Recent (VB6 mnuRecent 0..9, frm:546-557) */
    static constexpr int kMaxRecentFiles = 10;
    wxMenu*  m_recentMenu = nullptr;
    wxArrayString m_recentFiles;
    void RebuildRecentMenu();
    void LoadRecentFiles();
    void SaveRecentFiles();

    /* Window menu check items (kept to query/update state) */
    wxMenuItem* m_winItemTools     = nullptr;
    wxMenuItem* m_winItemDisplay   = nullptr;
    wxMenuItem* m_winItemPalette   = nullptr;
    wxMenuItem* m_winItemWaypoints = nullptr;
    wxMenuItem* m_winItemScenery   = nullptr;
    wxMenuItem* m_winItemProperties = nullptr;
    wxMenuItem* m_winItemTexture   = nullptr;

    wxStatusBar* m_statusBar = nullptr;
    wxStaticText* m_positionText = nullptr;
    wxStaticText* m_filenameText = nullptr;
    wxStaticText* m_zoomText = nullptr;
    wxStaticText* m_toolText = nullptr;
};
