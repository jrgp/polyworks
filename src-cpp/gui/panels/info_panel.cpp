#include "info_panel.h"
#include "mainframe.h"

#include <wx/sizer.h>

InfoPanel::InfoPanel(MainFrame* parent, const MapDocument& doc)
    : wxFrame(parent, wxID_ANY, "Map Info",
              wxDefaultPosition, wxSize(210, 230),
              wxFRAME_FLOAT_ON_PARENT | wxCAPTION | wxCLOSE_BOX |
              wxFRAME_NO_TASKBAR),
      m_mainFrame(parent),
      m_doc(doc) {
    SetBackgroundColour(wxColour(0x4A, 0x3C, 0x31));
    SetForegroundColour(*wxWHITE);
    buildUI();
}

void InfoPanel::buildUI() {
    auto* outer = new wxBoxSizer(wxVERTICAL);

    auto addRow = [&](wxStaticText*& field, const char* label) {
        auto* row = new wxBoxSizer(wxHORIZONTAL);
        auto* lbl = new wxStaticText(this, wxID_ANY, label);
        lbl->SetForegroundColour(*wxWHITE);
        field = new wxStaticText(this, wxID_ANY, "0",
                                 wxDefaultPosition, wxSize(60, -1),
                                 wxALIGN_RIGHT | wxST_NO_AUTORESIZE);
        field->SetForegroundColour(*wxWHITE);
        row->Add(lbl,   1, wxALIGN_CENTER_VERTICAL);
        row->Add(field, 0, wxALIGN_CENTER_VERTICAL);
        outer->Add(row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 6);
    };

    addRow(m_lblPolys,     "Polygons:");
    addRow(m_lblScenery,   "Scenery:");
    addRow(m_lblSpawns,    "Spawns:");
    addRow(m_lblColliders, "Colliders:");
    addRow(m_lblWaypoints, "Waypoints:");
    addRow(m_lblLights,    "Lights:");
    addRow(m_lblSelected,  "Selected:");
    addRow(m_lblZoom,      "Zoom:");
    addRow(m_lblScroll,    "Scroll:");

    outer->AddSpacer(8);
    SetSizer(outer);
    Fit();

    Refresh();
}

void InfoPanel::Refresh() {
    if (m_lblPolys == nullptr) return;

    /* Count selected entities */
    int selCount = 0;
    for (const auto& p : m_doc.polys)
        if (p.anySelected()) ++selCount;
    for (const auto& s : m_doc.scenery)    if (s.selected)  ++selCount;
    for (const auto& sp : m_doc.spawns)    if (sp.selected) ++selCount;
    for (const auto& c : m_doc.colliders)  if (c.selected)  ++selCount;
    for (const auto& wp : m_doc.waypoints) if (wp.selected) ++selCount;
    for (const auto& l : m_doc.lights)     if (l.selected)  ++selCount;

    m_lblPolys->SetLabel(wxString::Format("%d", (int)m_doc.polys.size()));
    m_lblScenery->SetLabel(wxString::Format("%d", (int)m_doc.scenery.size()));
    m_lblSpawns->SetLabel(wxString::Format("%d", (int)m_doc.spawns.size()));
    m_lblColliders->SetLabel(wxString::Format("%d", (int)m_doc.colliders.size()));
    m_lblWaypoints->SetLabel(wxString::Format("%d", (int)m_doc.waypoints.size()));
    m_lblLights->SetLabel(wxString::Format("%d", (int)m_doc.lights.size()));
    m_lblSelected->SetLabel(wxString::Format("%d", selCount));
    m_lblZoom->SetLabel(wxString::Format("%.0f%%", m_doc.zoom * 100.0f));
    m_lblScroll->SetLabel(wxString::Format("%.0f,%.0f", m_doc.scrollX, m_doc.scrollY));
}
