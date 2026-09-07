#pragma once
/*
 * info_panel.h — Port of frmInfo.frm
 *
 * Floating modeless panel showing live entity counts and selection properties.
 * Original VB6: ClientWidth=3120, ClientHeight=3120 → ~208×208 px @96dpi.
 */

#include "map_document.h"

#include <wx/frame.h>
#include <wx/stattext.h>

class MainFrame;

class InfoPanel final : public wxFrame {
public:
    InfoPanel(MainFrame* parent, const MapDocument& doc);

    /* Call after any edit to update displayed values. */
    void Refresh();

private:
    void buildUI();

    MainFrame*          m_mainFrame;
    const MapDocument&  m_doc;

    wxStaticText* m_lblPolys     = nullptr;
    wxStaticText* m_lblScenery   = nullptr;
    wxStaticText* m_lblSpawns    = nullptr;
    wxStaticText* m_lblColliders = nullptr;
    wxStaticText* m_lblWaypoints = nullptr;
    wxStaticText* m_lblLights    = nullptr;
    wxStaticText* m_lblSelected  = nullptr;
    wxStaticText* m_lblZoom      = nullptr;
    wxStaticText* m_lblScroll    = nullptr;
};
