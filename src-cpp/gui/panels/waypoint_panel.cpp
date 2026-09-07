/*
 * waypoint_panel.cpp — Port of frmWaypoints.frm
 */
#include "waypoint_panel.h"
#include "gui/mainframe.h"

#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/panel.h>

static const wxColour BG(0x31, 0x3C, 0x4A);   /* VB6 &H004A3C31 = BGR → RGB(0x31,0x3C,0x4A) */

WaypointPanel::WaypointPanel(MainFrame* parent)
    : wxFrame(parent, wxID_ANY, "Waypoints",
              wxDefaultPosition, wxDefaultSize,
              wxFRAME_FLOAT_ON_PARENT | wxFRAME_NO_TASKBAR |
              wxSYSTEM_MENU | wxCAPTION | wxCLOSE_BOX | wxCLIP_CHILDREN)
    , m_mainFrame(parent)
{
    SetBackgroundColour(BG);
    buildUI();
    SetClientSize(208, 160);
}

void WaypointPanel::buildUI() {
    auto* panel = new wxPanel(this, wxID_ANY);
    panel->SetBackgroundColour(BG);

    auto* root = new wxBoxSizer(wxVERTICAL);

    /* Direction flags */
    auto* dirBox = new wxStaticBoxSizer(wxVERTICAL, panel, "Direction flags");
    dirBox->GetStaticBox()->SetForegroundColour(*wxWHITE);
    dirBox->GetStaticBox()->SetBackgroundColour(BG);

    auto* row1 = new wxBoxSizer(wxHORIZONTAL);
    auto* row2 = new wxBoxSizer(wxHORIZONTAL);

    auto makeChk = [&](wxPanel* p, const wxString& label) {
        auto* c = new wxCheckBox(p, wxID_ANY, label);
        c->SetForegroundColour(*wxWHITE);
        c->SetBackgroundColour(BG);
        c->SetFont(wxFont(8, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL, false, "Arial"));
        return c;
    };

    m_chkLeft  = makeChk(panel, "Left");
    m_chkRight = makeChk(panel, "Right");
    m_chkUp    = makeChk(panel, "Up");
    m_chkDown  = makeChk(panel, "Down");
    m_chkM2    = makeChk(panel, "M2");

    row1->Add(m_chkLeft,  1, wxALL, 2);
    row1->Add(m_chkRight, 1, wxALL, 2);
    row1->Add(m_chkUp,    1, wxALL, 2);
    row2->Add(m_chkDown,  1, wxALL, 2);
    row2->Add(m_chkM2,    1, wxALL, 2);

    dirBox->Add(row1, 0, wxEXPAND);
    dirBox->Add(row2, 0, wxEXPAND);
    root->Add(dirBox, 0, wxEXPAND | wxALL, 4);

    /* Path number */
    auto* pathRow = new wxBoxSizer(wxHORIZONTAL);
    auto* pathLbl = new wxStaticText(panel, wxID_ANY, "Path:");
    pathLbl->SetForegroundColour(*wxWHITE);
    pathLbl->SetFont(wxFont(8, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL, false, "Arial"));
    m_spinPath = new wxSpinCtrl(panel, wxID_ANY, "0", wxDefaultPosition, wxSize(60, -1),
                                 wxSP_ARROW_KEYS, 0, 255, 0);
    m_spinPath->SetFont(wxFont(8, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL, false, "Arial"));
    pathRow->Add(pathLbl,    0, wxALIGN_CENTER_VERTICAL | wxALL, 2);
    pathRow->Add(m_spinPath, 0, wxALL, 2);
    root->Add(pathRow, 0, wxEXPAND | wxLEFT | wxRIGHT, 4);

    /* Special action */
    auto* spRow = new wxBoxSizer(wxHORIZONTAL);
    auto* spLbl = new wxStaticText(panel, wxID_ANY, "Special:");
    spLbl->SetForegroundColour(*wxWHITE);
    spLbl->SetFont(wxFont(8, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL, false, "Arial"));

    const wxString specialChoices[] = {
        "None", "Forward throw", "Backward throw", "Jet pack"
    };
    m_cboSpecial = new wxChoice(panel, wxID_ANY,
                                 wxDefaultPosition, wxSize(110, -1),
                                 4, specialChoices);
    m_cboSpecial->SetFont(wxFont(8, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL, false, "Arial"));
    m_cboSpecial->SetSelection(0);
    spRow->Add(spLbl,       0, wxALIGN_CENTER_VERTICAL | wxALL, 2);
    spRow->Add(m_cboSpecial, 0, wxALL, 2);
    root->Add(spRow, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 4);

    panel->SetSizer(root);
    auto* fs = new wxBoxSizer(wxVERTICAL);
    fs->Add(panel, 1, wxEXPAND);
    SetSizer(fs);

    /* Events */
    m_chkLeft ->Bind(wxEVT_CHECKBOX, &WaypointPanel::OnLeft,  this);
    m_chkRight->Bind(wxEVT_CHECKBOX, &WaypointPanel::OnRight, this);
    m_chkUp   ->Bind(wxEVT_CHECKBOX, &WaypointPanel::OnUp,    this);
    m_chkDown ->Bind(wxEVT_CHECKBOX, &WaypointPanel::OnDown,  this);
    m_chkM2   ->Bind(wxEVT_CHECKBOX, &WaypointPanel::OnM2,    this);
}

int WaypointPanel::GetPathNum() const {
    return m_spinPath ? m_spinPath->GetValue() : 0;
}

int WaypointPanel::GetSpecial() const {
    return m_cboSpecial ? m_cboSpecial->GetSelection() : 0;
}

void WaypointPanel::ShowWaypoint(bool left, bool right, bool up, bool down, bool m2,
                                   int pathNum, int special) {
    m_left = left; m_right = right; m_up = up; m_down = down; m_m2 = m2;
    if (m_chkLeft)  m_chkLeft ->SetValue(left);
    if (m_chkRight) m_chkRight->SetValue(right);
    if (m_chkUp)    m_chkUp   ->SetValue(up);
    if (m_chkDown)  m_chkDown ->SetValue(down);
    if (m_chkM2)    m_chkM2   ->SetValue(m2);
    if (m_spinPath) m_spinPath->SetValue(pathNum);
    if (m_cboSpecial) m_cboSpecial->SetSelection(special);
}

void WaypointPanel::Clear() {
    ShowWaypoint(false, false, false, false, false, 0, 0);
}

void WaypointPanel::OnLeft(wxCommandEvent& e)  { m_left  = e.IsChecked(); }
void WaypointPanel::OnRight(wxCommandEvent& e) { m_right = e.IsChecked(); }
void WaypointPanel::OnUp(wxCommandEvent& e)    { m_up    = e.IsChecked(); }
void WaypointPanel::OnDown(wxCommandEvent& e)  { m_down  = e.IsChecked(); }
void WaypointPanel::OnM2(wxCommandEvent& e)    { m_m2    = e.IsChecked(); }
