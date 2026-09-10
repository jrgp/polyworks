#include "display_panel.h"
#include "mainframe.h"

#include <wx/sizer.h>
#include <wx/statbox.h>

#include <algorithm>

/* VB6 twips-to-pixels: 1 twip = 1/15 px @96dpi.
   frmDisplay ClientWidth=3120, ClientHeight=2400 → 208×160 px.
   We use a slightly wider panel for comfortable checkbox labels. */

DisplayPanel::DisplayPanel(MainFrame* parent, ViewSettings& viewSettings)
    : wxFrame(parent, wxID_ANY, "Display",
              wxDefaultPosition, wxSize(210, 270),
              wxFRAME_FLOAT_ON_PARENT | wxCAPTION | wxCLOSE_BOX |
              wxFRAME_NO_TASKBAR | wxRESIZE_BORDER),
      m_mainFrame(parent),
      m_viewSettings(viewSettings) {
    SetBackgroundColour(wxColour(0x4A, 0x3C, 0x31));
    SetForegroundColour(*wxWHITE);
    buildUI();
}

void DisplayPanel::buildUI() {
    auto* outer = new wxBoxSizer(wxHORIZONTAL);
    auto* left  = new wxBoxSizer(wxVERTICAL);
    auto* right = new wxBoxSizer(wxVERTICAL);

    auto addCheck = [&](wxSizer* col, wxCheckBox*& field, const char* label, bool checked) {
        field = new wxCheckBox(this, wxID_ANY, label);
        field->SetValue(checked);
        field->SetForegroundColour(*wxWHITE);
        field->SetBackgroundColour(wxColour(0x4A, 0x3C, 0x31));
        field->Bind(wxEVT_CHECKBOX, &DisplayPanel::OnCheckbox, this);
        col->Add(field, 0, wxLEFT | wxTOP, 6);
    };

    /* frmDisplay.frm lays the eleven toggles out in two columns (left=360,
       right=2040 twips).  The three scenery layers are not here: the original
       puts them in View > Scenery Layers (mnuShowSceneryLayers) and frmDisplay
       carries only the master "Scenery" switch. */
    addCheck(left,  m_chkWaypoints,     " Waypoints",  m_viewSettings.showWaypoints);
    addCheck(left,  m_chkObjects,       " Objects",    m_viewSettings.showObjects);
    addCheck(left,  m_chkScenery,       " Scenery",    m_viewSettings.showScenery);
    addCheck(left,  m_chkPoints,        " Points",     m_viewSettings.showPoints);
    addCheck(left,  m_chkWireframe,     " Wireframe",  m_viewSettings.showWireframe);
    addCheck(left,  m_chkTexture,       " Texture",    m_viewSettings.showTexture);
    addCheck(left,  m_chkPolys,         " Polygons",   m_viewSettings.showPolys);
    addCheck(left,  m_chkBackground,    " Background", m_viewSettings.showBackground);

    addCheck(right, m_chkGrid,          " Grid",       m_viewSettings.showGrid);
    addCheck(right, m_chkLights,        " Lights",     m_viewSettings.showLights);
    addCheck(right, m_chkSketch,        " Sketch",     m_viewSettings.showSketch);

    outer->Add(left, 0, wxRIGHT, 8);
    outer->Add(right, 0, wxRIGHT, 6);
    SetSizer(outer);
    Fit();

    /* GTK only measures a check button's label once the widget is realised, so
       the constructor's Fit() can leave the right-hand column clipped.  After
       the first show, widen the frame to whatever the laid-out controls plus
       their measured labels actually need. */
    Bind(wxEVT_SHOW, [this](wxShowEvent& event) {
        event.Skip();
        if (m_fitted || !event.IsShown()) return;
        m_fitted = true;
        CallAfter([this] {
            int needed = 0;
            for (wxWindow* child : GetChildren()) {
                auto* box = dynamic_cast<wxCheckBox*>(child);
                if (box == nullptr) continue;
                box->InvalidateBestSize();
                const int width = std::max(box->GetBestSize().x,
                                           box->GetTextExtent(box->GetLabel()).x + 40);
                needed = std::max(needed, box->GetPosition().x + width);
            }
            if (needed > GetClientSize().x)
                SetClientSize(needed + 6, GetClientSize().y);
        });
    });
}

void DisplayPanel::Sync() {
    if (m_chkPolys)        m_chkPolys->SetValue(m_viewSettings.showPolys);
    if (m_chkWireframe)    m_chkWireframe->SetValue(m_viewSettings.showWireframe);
    if (m_chkPoints)       m_chkPoints->SetValue(m_viewSettings.showPoints);
    if (m_chkTexture)      m_chkTexture->SetValue(m_viewSettings.showTexture);
    if (m_chkBackground)   m_chkBackground->SetValue(m_viewSettings.showBackground);
    if (m_chkGrid)         m_chkGrid->SetValue(m_viewSettings.showGrid);
    if (m_chkObjects)      m_chkObjects->SetValue(m_viewSettings.showObjects);
    if (m_chkWaypoints)    m_chkWaypoints->SetValue(m_viewSettings.showWaypoints);
    if (m_chkLights)       m_chkLights->SetValue(m_viewSettings.showLights);
    if (m_chkSketch)       m_chkSketch->SetValue(m_viewSettings.showSketch);
    if (m_chkScenery)      m_chkScenery->SetValue(m_viewSettings.showScenery);
}

void DisplayPanel::OnCheckbox(wxCommandEvent& /*event*/) {
    m_viewSettings.showPolys          = m_chkPolys->GetValue();
    m_viewSettings.showWireframe      = m_chkWireframe->GetValue();
    m_viewSettings.showPoints         = m_chkPoints->GetValue();
    m_viewSettings.showTexture        = m_chkTexture->GetValue();
    m_viewSettings.showBackground     = m_chkBackground->GetValue();
    m_viewSettings.showGrid           = m_chkGrid->GetValue();
    m_viewSettings.showObjects        = m_chkObjects->GetValue();
    m_viewSettings.showWaypoints      = m_chkWaypoints->GetValue();
    m_viewSettings.showLights         = m_chkLights->GetValue();
    m_viewSettings.showSketch         = m_chkSketch->GetValue();
    m_viewSettings.showScenery        = m_chkScenery->GetValue();

    if (m_mainFrame != nullptr) {
        m_mainFrame->SyncViewMenu();
        m_mainFrame->RefreshViewport();
    }
}
