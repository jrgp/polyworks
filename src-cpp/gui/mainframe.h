#pragma once

#include "map_document.h"
#include "undo_stack.h"

#include <wx/frame.h>
#include <wx/statusbr.h>
#include <wx/stattext.h>

class GlViewport;
class ToolsPanel;
class DisplayPanel;
class InfoPanel;
class SceneryPanel;
class WaypointPanel;
class PalettePanel;
class wxCommandEvent;
class wxKeyEvent;
class wxSizeEvent;

wxString GetToolName(int tool);
wxString GetToolHotkey(int tool);

class MainFrame final : public wxFrame {
public:
    explicit MainFrame(const wxString& skinsPath);

    void AttachToolsPanel(ToolsPanel* toolsPanel);
    void AttachDisplayPanel(DisplayPanel* displayPanel);
    void AttachInfoPanel(InfoPanel* infoPanel);
    void AttachSceneryPanel(SceneryPanel* sceneryPanel);
    void AttachWaypointPanel(WaypointPanel* waypointPanel);
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
    SceneryPanel*  m_sceneryPanel  = nullptr;
    WaypointPanel* m_waypointPanel = nullptr;
    PalettePanel*  m_palettePanel  = nullptr;

private:
    void buildMenuBar();
    void buildStatusBar();
    void layoutStatusBarFields();

    void OnFileNew(wxCommandEvent& event);
    void OnFileOpen(wxCommandEvent& event);
    void OnFileSave(wxCommandEvent& event);
    void OnFileSaveAs(wxCommandEvent& event);
    void OnFileCompile(wxCommandEvent& event);

    void OnEditUndo(wxCommandEvent& event);
    void OnEditRedo(wxCommandEvent& event);
    void OnEditDeleteSelected(wxCommandEvent& event);
    void OnEditSelectAll(wxCommandEvent& event);
    void OnEditInvertSelection(wxCommandEvent& event);
    void OnEditDuplicateSelected(wxCommandEvent& event);

    void OnMapSettings(wxCommandEvent& event);
    void OnPreferences(wxCommandEvent& event);

    void OnWindowShowAll(wxCommandEvent& event);
    void OnWindowHideAll(wxCommandEvent& event);
    void OnWindowTogglePanel(wxCommandEvent& event);
    void OnWindowLoadWorkspace(wxCommandEvent& event);
    void OnWindowSaveWorkspace(wxCommandEvent& event);
    void OnWindowResetLayout(wxCommandEvent& event);

    void OnExit(wxCommandEvent& event);
    void OnKeyDown(wxKeyEvent& event);
    void OnSize(wxSizeEvent& event);

    bool SaveDocumentToPath(const wxString& path);

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
