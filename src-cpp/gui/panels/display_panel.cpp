#include "display_panel.h"
#include "mainframe.h"

#include <wx/sizer.h>
#include <wx/statbox.h>

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
    auto* outer = new wxBoxSizer(wxVERTICAL);

    auto addCheck = [&](wxCheckBox*& field, const char* label, bool checked) {
        field = new wxCheckBox(this, wxID_ANY, label);
        field->SetValue(checked);
        field->SetForegroundColour(*wxWHITE);
        field->SetBackgroundColour(wxColour(0x4A, 0x3C, 0x31));
        field->Bind(wxEVT_CHECKBOX, &DisplayPanel::OnCheckbox, this);
        outer->Add(field, 0, wxLEFT | wxTOP, 6);
    };

    addCheck(m_chkPolys,         " Polygons",       m_viewSettings.showPolys);
    addCheck(m_chkWireframe,     " Wireframe",      m_viewSettings.showWireframe);
    addCheck(m_chkPoints,        " Points",         m_viewSettings.showPoints);
    addCheck(m_chkTexture,       " Texture",        m_viewSettings.showTexture);
    addCheck(m_chkBackground,    " Background",     m_viewSettings.showBackground);
    addCheck(m_chkGrid,          " Grid",           m_viewSettings.showGrid);
    addCheck(m_chkObjects,       " Objects",        m_viewSettings.showObjects);
    addCheck(m_chkWaypoints,     " Waypoints",      m_viewSettings.showWaypoints);
    addCheck(m_chkLights,        " Lights",         m_viewSettings.showLights);
    addCheck(m_chkSketch,        " Sketch",         m_viewSettings.showSketch);
    addCheck(m_chkSceneryBack,   " Scenery Back",   m_viewSettings.showSceneryBack);
    addCheck(m_chkSceneryMiddle, " Scenery Middle", m_viewSettings.showSceneryMiddle);
    addCheck(m_chkSceneryFront,  " Scenery Front",  m_viewSettings.showSceneryFront);

    outer->AddSpacer(8);
    SetSizer(outer);
    Fit();
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
    if (m_chkSceneryBack)  m_chkSceneryBack->SetValue(m_viewSettings.showSceneryBack);
    if (m_chkSceneryMiddle)m_chkSceneryMiddle->SetValue(m_viewSettings.showSceneryMiddle);
    if (m_chkSceneryFront) m_chkSceneryFront->SetValue(m_viewSettings.showSceneryFront);
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
    m_viewSettings.showSceneryBack    = m_chkSceneryBack->GetValue();
    m_viewSettings.showSceneryMiddle  = m_chkSceneryMiddle->GetValue();
    m_viewSettings.showSceneryFront   = m_chkSceneryFront->GetValue();

    if (m_mainFrame != nullptr)
        m_mainFrame->RefreshViewport();
}
