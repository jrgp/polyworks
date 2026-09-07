/*
 * scenery_panel.cpp — Port of frmScenery.frm
 */
#include "scenery_panel.h"
#include "gui/mainframe.h"
#include "pms_types.h"

#include <wx/sizer.h>
#include <wx/dir.h>
#include <wx/filename.h>
#include <wx/statbox.h>
#include <wx/panel.h>

static const wxColour BG_COLOUR(0x31, 0x3C, 0x4A);   /* 0x4A3C31 → RGB(0x31,0x3C,0x4A)? */
/* VB6 BackColor &H004A3C31& = RGB(0x31,0x3C,0x4A) — stored as BGR in VB6 */
static const wxColour BG_COL(0x31, 0x3C, 0x4A);
static const wxColour FG_COL(*wxWHITE);

SceneryPanel::SceneryPanel(MainFrame* parent, const wxString& soldatPath)
    : wxFrame(parent, wxID_ANY, "Scenery",
              wxDefaultPosition, wxDefaultSize,
              wxFRAME_FLOAT_ON_PARENT | wxFRAME_NO_TASKBAR |
              wxSYSTEM_MENU | wxCAPTION | wxCLOSE_BOX | wxCLIP_CHILDREN)
    , m_mainFrame(parent)
{
    SetBackgroundColour(wxColour(0x31, 0x3C, 0x4A));
    buildUI();
    SetClientSize(208, 220);

    if (!soldatPath.empty()) {
        ListScenery(soldatPath);
    }
}

void SceneryPanel::buildUI() {
    auto* panel = new wxPanel(this, wxID_ANY);
    panel->SetBackgroundColour(wxColour(0x31, 0x3C, 0x4A));

    auto* mainSizer = new wxBoxSizer(wxVERTICAL);

    /* Scenery list */
    m_lstScenery = new wxListBox(panel, wxID_ANY,
                                  wxDefaultPosition, wxSize(190, 100),
                                  0, nullptr, wxLB_SINGLE);
    m_lstScenery->SetBackgroundColour(wxColour(0x20, 0x20, 0x20));
    m_lstScenery->SetForegroundColour(*wxWHITE);
    m_lstScenery->SetFont(wxFont(8, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL, false, "Arial"));
    mainSizer->Add(m_lstScenery, 1, wxEXPAND | wxALL, 4);

    /* Level selector */
    auto* lvlBox = new wxStaticBoxSizer(wxHORIZONTAL, panel, "Level");
    lvlBox->GetStaticBox()->SetForegroundColour(*wxWHITE);
    lvlBox->GetStaticBox()->SetBackgroundColour(wxColour(0x31, 0x3C, 0x4A));

    m_rbBack   = new wxRadioButton(panel, wxID_ANY, "Back",   wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
    m_rbMiddle = new wxRadioButton(panel, wxID_ANY, "Middle");
    m_rbFront  = new wxRadioButton(panel, wxID_ANY, "Front");

    for (auto* rb : {m_rbBack, m_rbMiddle, m_rbFront}) {
        rb->SetForegroundColour(*wxWHITE);
        rb->SetBackgroundColour(wxColour(0x31, 0x3C, 0x4A));
        rb->SetFont(wxFont(8, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL, false, "Arial"));
        lvlBox->Add(rb, 0, wxALL, 2);
    }
    m_rbMiddle->SetValue(true);  /* default: middle */
    mainSizer->Add(lvlBox, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 4);

    /* Rotate / Scale toggles */
    auto* optSizer = new wxBoxSizer(wxHORIZONTAL);
    m_chkRotate = new wxCheckBox(panel, wxID_ANY, "Rotate");
    m_chkScale  = new wxCheckBox(panel, wxID_ANY, "Scale");
    for (auto* c : {m_chkRotate, m_chkScale}) {
        c->SetForegroundColour(*wxWHITE);
        c->SetBackgroundColour(wxColour(0x31, 0x3C, 0x4A));
        c->SetFont(wxFont(8, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL, false, "Arial"));
    }
    optSizer->Add(m_chkRotate, 1, wxALL, 4);
    optSizer->Add(m_chkScale,  1, wxALL, 4);
    mainSizer->Add(optSizer, 0, wxEXPAND);

    panel->SetSizer(mainSizer);
    auto* frameSizer = new wxBoxSizer(wxVERTICAL);
    frameSizer->Add(panel, 1, wxEXPAND);
    SetSizer(frameSizer);

    /* Events */
    m_lstScenery->Bind(wxEVT_LISTBOX, &SceneryPanel::OnScenerySelect, this);
    m_rbBack->Bind(wxEVT_RADIOBUTTON,   &SceneryPanel::OnLevelBack,   this);
    m_rbMiddle->Bind(wxEVT_RADIOBUTTON, &SceneryPanel::OnLevelMiddle, this);
    m_rbFront->Bind(wxEVT_RADIOBUTTON,  &SceneryPanel::OnLevelFront,  this);
    m_chkRotate->Bind(wxEVT_CHECKBOX,   &SceneryPanel::OnRotate,      this);
    m_chkScale->Bind(wxEVT_CHECKBOX,    &SceneryPanel::OnScale,       this);
}

void SceneryPanel::ListScenery(const wxString& soldatPath) {
    if (!m_lstScenery) return;
    m_lstScenery->Clear();

    wxString sceneryDir = wxFileName(soldatPath, "Scenery-gfx").GetFullPath();
    if (!wxFileName::DirExists(sceneryDir)) return;

    static const char* kExtArr[] = {"*.bmp", "*.png", "*.tga", "*.gif"};
    wxArrayString files;
    for (const auto* ext : kExtArr) {
        wxDir::GetAllFiles(sceneryDir, &files, ext, wxDIR_FILES);
    }
    files.Sort();

    for (const auto& path : files) {
        m_lstScenery->Append(wxFileName(path).GetFullName());
    }

    if (m_lstScenery->GetCount() > 0) {
        m_lstScenery->SetSelection(0);
    }
}

void SceneryPanel::UpdateInUse(const std::vector<std::string>& names) {
    /* Could show in-use items distinctly; for now just keep the list as-is */
    (void)names;
}

wxString SceneryPanel::GetSelectedScenery() const {
    int sel = m_lstScenery ? m_lstScenery->GetSelection() : wxNOT_FOUND;
    if (sel == wxNOT_FOUND) return {};
    return m_lstScenery->GetString(sel);
}

void SceneryPanel::OnScenerySelect(wxCommandEvent& /*event*/) {}

void SceneryPanel::OnLevelBack(wxCommandEvent& /*event*/)   { m_level = SCENERY_BACK;   }
void SceneryPanel::OnLevelMiddle(wxCommandEvent& /*event*/) { m_level = SCENERY_MIDDLE; }
void SceneryPanel::OnLevelFront(wxCommandEvent& /*event*/)  { m_level = SCENERY_FRONT;  }
void SceneryPanel::OnRotate(wxCommandEvent& /*event*/)      { m_rotate = m_chkRotate->GetValue(); }
void SceneryPanel::OnScale(wxCommandEvent& /*event*/)       { m_scale  = m_chkScale->GetValue();  }
