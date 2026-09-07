#pragma once
/*
 * display_panel.h — Port of frmDisplay.frm
 *
 * Floating modeless panel with checkboxes controlling viewport visibility.
 * Original VB6: ClientWidth=3120, ClientHeight=2400 (twips → ~208×160 px @96dpi)
 */

#include "map_document.h"

#include <wx/frame.h>
#include <wx/checkbox.h>

class MainFrame;

class DisplayPanel final : public wxFrame {
public:
    DisplayPanel(MainFrame* parent, ViewSettings& viewSettings);

    /* Sync checkboxes from viewSettings (call after external changes). */
    void Sync();

private:
    void buildUI();
    void OnCheckbox(wxCommandEvent& event);

    MainFrame*    m_mainFrame;
    ViewSettings& m_viewSettings;

    wxCheckBox* m_chkPolys          = nullptr;
    wxCheckBox* m_chkWireframe      = nullptr;
    wxCheckBox* m_chkPoints         = nullptr;
    wxCheckBox* m_chkTexture        = nullptr;
    wxCheckBox* m_chkBackground     = nullptr;
    wxCheckBox* m_chkGrid           = nullptr;
    wxCheckBox* m_chkObjects        = nullptr;
    wxCheckBox* m_chkWaypoints      = nullptr;
    wxCheckBox* m_chkLights         = nullptr;
    wxCheckBox* m_chkSketch         = nullptr;
    wxCheckBox* m_chkSceneryBack    = nullptr;
    wxCheckBox* m_chkSceneryMiddle  = nullptr;
    wxCheckBox* m_chkSceneryFront   = nullptr;
};
