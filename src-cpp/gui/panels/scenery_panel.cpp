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
#include <wx/menu.h>
#include <wx/msgdlg.h>

static const wxColour BG_COLOUR(0x31, 0x3C, 0x4A);   /* 0x4A3C31 → RGB(0x31,0x3C,0x4A)? */
/* VB6 BackColor &H004A3C31& = RGB(0x31,0x3C,0x4A) — stored as BGR in VB6 */
static const wxColour BG_COL(0x31, 0x3C, 0x4A);
static const wxColour FG_COL(*wxWHITE);

/* Prefix marking a scenery file that the current map actually references. */
static const char* const kInUseMark = "\u2022 ";

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

    m_soldatPath = soldatPath;
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
    m_lstScenery->Bind(wxEVT_RIGHT_DOWN, &SceneryPanel::OnListRightDown, this);
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

    m_soldatPath = soldatPath;
    for (const auto& path : files) {
        m_lstScenery->Append(wxFileName(path).GetFullName());
    }
    UpdateInUse(m_inUse);

    if (m_lstScenery->GetCount() > 0) {
        m_lstScenery->SetSelection(0);
    }
}

void SceneryPanel::UpdateInUse(const std::vector<std::string>& names) {
    /* VB6 keeps an explicit "In Use" branch in the scenery tree
       (frmScenery.frm:397).  The port uses a single list, so in-use entries
       are flagged with a bullet instead of being duplicated into a branch. */
    m_inUse = names;
    if (!m_lstScenery) return;
    const int sel = m_lstScenery->GetSelection();
    for (unsigned i = 0; i < m_lstScenery->GetCount(); ++i) {
        wxString label = m_lstScenery->GetString(i);
        const bool marked = label.StartsWith(kInUseMark);
        wxString bare = marked ? label.Mid(wxStrlen(kInUseMark)) : label;
        bool used = false;
        for (const auto& n : names) {
            if (bare.IsSameAs(wxString::FromUTF8(n.c_str()), false)) {
                used = true;
                break;
            }
        }
        const wxString want = used ? (wxString(kInUseMark) + bare) : bare;
        if (want != label) m_lstScenery->SetString(i, want);
    }
    if (sel != wxNOT_FOUND) m_lstScenery->SetSelection(sel);
}

void SceneryPanel::OnListRightDown(wxMouseEvent& event) {
    /* mnuScenery (frmScenery.frm:601-628): Reload / Refresh / Clear Unused. */
    enum { kReload = 1, kRefresh, kClearUnused };
    wxMenu menu;
    menu.Append(kReload, "Reload List");
    menu.Append(kRefresh, "Refresh Textures");
    menu.AppendSeparator();
    menu.Append(kClearUnused, "Clear Unused");

    const int cmd = GetPopupMenuSelectionFromUser(menu, event.GetPosition());
    if (cmd == kReload) {
        ListScenery(m_soldatPath);
    } else if (cmd == kRefresh) {
        if (m_mainFrame != nullptr) m_mainFrame->ReloadSceneryTextures();
    } else if (cmd == kClearUnused) {
        if (m_mainFrame != nullptr) m_mainFrame->ClearUnusedScenery();
    }
}

wxString SceneryPanel::GetSelectedScenery() const {
    int sel = m_lstScenery ? m_lstScenery->GetSelection() : wxNOT_FOUND;
    if (sel == wxNOT_FOUND) return {};
    wxString name = m_lstScenery->GetString(sel);
    if (name.StartsWith(kInUseMark)) name = name.Mid(wxStrlen(kInUseMark));
    return name;
}

void SceneryPanel::OnScenerySelect(wxCommandEvent& /*event*/) {}

void SceneryPanel::OnLevelBack(wxCommandEvent& /*event*/)   { m_level = SCENERY_BACK;   }
void SceneryPanel::OnLevelMiddle(wxCommandEvent& /*event*/) { m_level = SCENERY_MIDDLE; }
void SceneryPanel::OnLevelFront(wxCommandEvent& /*event*/)  { m_level = SCENERY_FRONT;  }
void SceneryPanel::OnRotate(wxCommandEvent& /*event*/)      { m_rotate = m_chkRotate->GetValue(); }
void SceneryPanel::OnScale(wxCommandEvent& /*event*/)       { m_scale  = m_chkScale->GetValue();  }
