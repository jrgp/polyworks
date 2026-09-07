#pragma once

#include "map_document.h"
#include "undo_stack.h"

#include <wx/frame.h>
#include <wx/statusbr.h>
#include <wx/stattext.h>

class GlViewport;
class ToolsPanel;
class wxCommandEvent;
class wxKeyEvent;
class wxSizeEvent;

wxString GetToolName(int tool);
wxString GetToolHotkey(int tool);

class MainFrame final : public wxFrame {
public:
    explicit MainFrame(const wxString& skinsPath);

    void AttachToolsPanel(ToolsPanel* toolsPanel);
    void SetActiveTool(int tool);
    int GetActiveTool() const { return m_activeTool; }
    void UpdateStatusBar();
    void UpdateTitle();
    void UpdateMouseWorldPosition(const Vec2& world);

    MapDocument m_doc;
    UndoStack m_undoStack;
    int m_activeTool = 0;
    GlViewport* m_viewport = nullptr;

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

    void OnExit(wxCommandEvent& event);
    void OnKeyDown(wxKeyEvent& event);
    void OnSize(wxSizeEvent& event);

    bool SaveDocumentToPath(const wxString& path);
    void RefreshViewport();

    wxString m_skinsPath;
    wxString m_currentFilePath;
    ToolsPanel* m_toolsPanel = nullptr;
    Vec2 m_lastMouseWorld{};

    wxStatusBar* m_statusBar = nullptr;
    wxStaticText* m_positionText = nullptr;
    wxStaticText* m_filenameText = nullptr;
    wxStaticText* m_zoomText = nullptr;
    wxStaticText* m_toolText = nullptr;
};
