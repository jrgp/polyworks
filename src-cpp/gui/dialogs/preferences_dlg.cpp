/*
 * preferences_dlg.cpp — Port of frmPreferences.frm
 */
#include "preferences_dlg.h"

#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/panel.h>
#include <wx/notebook.h>
#include <wx/dir.h>
#include <wx/dirdlg.h>
#include <wx/filename.h>
#include <wx/msgdlg.h>
#include <wx/button.h>

#include <string>

static const wxColour BG(0x31, 0x3C, 0x4A);   /* VB6 BackColor &H004A3C31 */

static void styleLabel(wxStaticText* lbl) {
    lbl->SetForegroundColour(*wxWHITE);
    lbl->SetFont(wxFont(9, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL,
                        wxFONTWEIGHT_NORMAL, false, "Arial"));
}

static wxTextCtrl* makeFloat(wxWindow* parent, const wxString& val) {
    auto* t = new wxTextCtrl(parent, wxID_ANY, val,
                              wxDefaultPosition, wxSize(70, -1));
    t->SetFont(wxFont(9, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL,
                      wxFONTWEIGHT_NORMAL, false, "Arial"));
    return t;
}

static void addRow(wxSizer* sizer, wxWindow* parent,
                   const wxString& label, wxWindow* ctrl) {
    auto* lbl = new wxStaticText(parent, wxID_ANY, label);
    styleLabel(lbl);
    auto* row = new wxBoxSizer(wxHORIZONTAL);
    row->Add(lbl, 1, wxALIGN_CENTER_VERTICAL | wxALL, 3);
    row->Add(ctrl, 0, wxALIGN_CENTER_VERTICAL | wxALL, 3);
    sizer->Add(row, 0, wxEXPAND);
}

/* A path box with the folder button frmPreferences puts beside each one. */
static void addBrowseRow(wxSizer* sizer, wxWindow* parent,
                         const wxString& label, wxTextCtrl* ctrl,
                         const wxString& prompt) {
    auto* lbl = new wxStaticText(parent, wxID_ANY, label);
    styleLabel(lbl);
    auto* browse = new wxButton(parent, wxID_ANY, "...",
                                wxDefaultPosition, wxDefaultSize,
                                wxBU_EXACTFIT);
    browse->SetToolTip(prompt);
    browse->Bind(wxEVT_BUTTON, [ctrl, prompt](wxCommandEvent&) {
        wxDirDialog dlg(ctrl->GetParent(), prompt, ctrl->GetValue(),
                        wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);
        if (dlg.ShowModal() == wxID_OK) ctrl->SetValue(dlg.GetPath());
    });

    auto* row = new wxBoxSizer(wxHORIZONTAL);
    row->Add(lbl, 1, wxALIGN_CENTER_VERTICAL | wxALL, 3);
    row->Add(ctrl, 0, wxALIGN_CENTER_VERTICAL | wxALL, 3);
    row->Add(browse, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT | wxTOP | wxBOTTOM, 3);
    sizer->Add(row, 0, wxEXPAND);
}

PreferencesDlg::PreferencesDlg(wxWindow* parent, AppPrefs& prefs)
    : wxDialog(parent, wxID_ANY, "Preferences",
               wxDefaultPosition, wxDefaultSize,
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
    , m_prefs(prefs)
{
    SetBackgroundColour(BG);
    buildUI();
    populateFromPrefs();
    Fit();
}

void PreferencesDlg::buildUI() {
    auto* outer = new wxBoxSizer(wxVERTICAL);

    /* Notebook with tabs */
    auto* nb = new wxNotebook(this, wxID_ANY);
    nb->SetBackgroundColour(BG);

    /* ---- Zoom tab ---- */
    auto* zoomPage = new wxPanel(nb);
    zoomPage->SetBackgroundColour(BG);
    auto* zs = new wxBoxSizer(wxVERTICAL);
    m_txtMinZoom   = makeFloat(zoomPage, "0.0625");
    m_txtMaxZoom   = makeFloat(zoomPage, "16");
    m_txtResetZoom = makeFloat(zoomPage, "1");
    addRow(zs, zoomPage, "Min zoom:", m_txtMinZoom);
    addRow(zs, zoomPage, "Max zoom:", m_txtMaxZoom);
    addRow(zs, zoomPage, "Reset zoom:", m_txtResetZoom);
    zoomPage->SetSizer(zs);
    nb->AddPage(zoomPage, "Zoom");

    /* ---- Grid tab ---- */
    auto* gridPage = new wxPanel(nb);
    gridPage->SetBackgroundColour(BG);
    auto* gs = new wxBoxSizer(wxVERTICAL);
    m_spinSpacing   = new wxSpinCtrl(gridPage, wxID_ANY, "10",  wxDefaultPosition, wxSize(70,-1), wxSP_ARROW_KEYS, 1, 500, 10);
    m_spinDivisions = new wxSpinCtrl(gridPage, wxID_ANY, "4",   wxDefaultPosition, wxSize(70,-1), wxSP_ARROW_KEYS, 1, 64,  4);
    m_cpkGrid1      = new wxColourPickerCtrl(gridPage, wxID_ANY, wxColour(0x60, 0x60, 0x60));
    m_cpkGrid2      = new wxColourPickerCtrl(gridPage, wxID_ANY, wxColour(0x40, 0x40, 0x40));
    addRow(gs, gridPage, "Spacing (px):",   m_spinSpacing);
    addRow(gs, gridPage, "Divisions:",      m_spinDivisions);
    addRow(gs, gridPage, "Grid color 1:",   m_cpkGrid1);
    addRow(gs, gridPage, "Grid color 2:",   m_cpkGrid2);
    /* frmPreferences txtOpacity1 / txtOpacity2 (percent). */
    m_txtGridAlpha1 = makeFloat(gridPage, "50");
    m_txtGridAlpha2 = makeFloat(gridPage, "30");
    addRow(gs, gridPage, "Grid opacity 1 (%):", m_txtGridAlpha1);
    addRow(gs, gridPage, "Grid opacity 2 (%):", m_txtGridAlpha2);
    gridPage->SetSizer(gs);
    nb->AddPage(gridPage, "Grid");

    /* ---- Snap tab ---- */
    auto* snapPage = new wxPanel(nb);
    snapPage->SetBackgroundColour(BG);
    auto* ss = new wxBoxSizer(wxVERTICAL);
    m_chkSnap       = new wxCheckBox(snapPage, wxID_ANY, "Enable snap to vertex");
    m_chkSnap->SetForegroundColour(*wxWHITE);
    m_chkSnap->SetBackgroundColour(BG);
    m_txtSnapRadius = makeFloat(snapPage, "8.0");
    ss->Add(m_chkSnap, 0, wxALL, 4);
    addRow(ss, snapPage, "Snap radius (px):", m_txtSnapRadius);
    snapPage->SetSizer(ss);
    nb->AddPage(snapPage, "Snap");

    /* ---- Undo tab ---- */
    auto* undoPage = new wxPanel(nb);
    undoPage->SetBackgroundColour(BG);
    auto* us = new wxBoxSizer(wxVERTICAL);
    m_spinUndo = new wxSpinCtrl(undoPage, wxID_ANY, "16", wxDefaultPosition, wxSize(70,-1), wxSP_ARROW_KEYS, 1, 256, 16);
    addRow(us, undoPage, "Undo depth:", m_spinUndo);
    undoPage->SetSizer(us);
    nb->AddPage(undoPage, "Undo");

    /* ---- Colors tab ---- */
    auto* colorPage = new wxPanel(nb);
    colorPage->SetBackgroundColour(BG);
    auto* cs = new wxBoxSizer(wxVERTICAL);
    m_cpkPoint     = new wxColourPickerCtrl(colorPage, wxID_ANY, *wxWHITE);
    m_cpkSelection = new wxColourPickerCtrl(colorPage, wxID_ANY, *wxYELLOW);
    addRow(cs, colorPage, "Vertex point color:",   m_cpkPoint);
    addRow(cs, colorPage, "Selection color:",       m_cpkSelection);
    colorPage->SetSizer(cs);
    nb->AddPage(colorPage, "Colors");

    /* ---- Paths tab ---- */
    auto* pathPage = new wxPanel(nb);
    pathPage->SetBackgroundColour(BG);
    auto* ps = new wxBoxSizer(wxVERTICAL);
    m_txtSoldatDir  = new wxTextCtrl(pathPage, wxID_ANY, "", wxDefaultPosition, wxSize(260,-1));
    m_txtPrefabsDir = new wxTextCtrl(pathPage, wxID_ANY, "", wxDefaultPosition, wxSize(260,-1));
    m_txtUncompDir  = new wxTextCtrl(pathPage, wxID_ANY, "", wxDefaultPosition, wxSize(260,-1));
    for (auto* t : {m_txtSoldatDir, m_txtPrefabsDir, m_txtUncompDir})
        t->SetFont(wxFont(8, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL, false, "Arial"));

    /* frmPreferences pairs each path box with a folder button -- picFolder,
       picUncomp and picPrefabs, all calling SelectFolder (frm:2488-2528). */
    addBrowseRow(ps, pathPage, "Soldat directory:",      m_txtSoldatDir,
                 "Select the Soldat game directory");
    addBrowseRow(ps, pathPage, "Prefabs directory:",     m_txtPrefabsDir,
                 "Select the prefabs directory");
    addBrowseRow(ps, pathPage, "Uncompiled directory:",  m_txtUncompDir,
                 "Select the uncompiled maps directory");

    auto* hint = new wxStaticText(pathPage, wxID_ANY,
        "The Soldat directory is the one holding Maps, Textures and\n"
        "Scenery-gfx.  Textures and scenery are loaded from there.");
    styleLabel(hint);
    ps->Add(hint, 0, wxALL, 6);
    pathPage->SetSizer(ps);
    nb->AddPage(pathPage, "Paths");

    /* ---- Blending tab (frmPreferences "Blending" frame) ---- */
    static const wxString kBlendNames[] = {
        "ZERO", "ONE", "SRCCOLOR", "INVSRCCOLOR",
        "DESTCOLOR", "INVDESTCOLOR", "SRCALPHA", "INVSRCALPHA"};
    const wxArrayString blendChoices(8, kBlendNames);

    auto* blendPage = new wxPanel(nb);
    blendPage->SetBackgroundColour(BG);
    auto* bs = new wxBoxSizer(wxVERTICAL);
    m_choPolySrc  = new wxChoice(blendPage, wxID_ANY, wxDefaultPosition, wxDefaultSize, blendChoices);
    m_choPolyDest = new wxChoice(blendPage, wxID_ANY, wxDefaultPosition, wxDefaultSize, blendChoices);
    m_choWireSrc  = new wxChoice(blendPage, wxID_ANY, wxDefaultPosition, wxDefaultSize, blendChoices);
    m_choWireDest = new wxChoice(blendPage, wxID_ANY, wxDefaultPosition, wxDefaultSize, blendChoices);
    addRow(bs, blendPage, "Polygon SRC:",   m_choPolySrc);
    addRow(bs, blendPage, "Polygon DEST:",  m_choPolyDest);
    addRow(bs, blendPage, "Wireframe SRC:", m_choWireSrc);
    addRow(bs, blendPage, "Wireframe DEST:", m_choWireDest);
    blendPage->SetSizer(bs);
    nb->AddPage(blendPage, "Blending");

    outer->Add(nb, 1, wxEXPAND | wxALL, 8);

    /* OK / Cancel */
    auto* btnSizer = new wxBoxSizer(wxHORIZONTAL);
    auto* btnOK     = new wxButton(this, wxID_OK,     "OK");
    auto* btnCancel = new wxButton(this, wxID_CANCEL, "Cancel");
    btnSizer->AddStretchSpacer();
    btnSizer->Add(btnOK,     0, wxALL, 4);
    btnSizer->Add(btnCancel, 0, wxALL, 4);
    outer->Add(btnSizer, 0, wxEXPAND | wxALL, 4);

    SetSizer(outer);

    btnOK    ->Bind(wxEVT_BUTTON, &PreferencesDlg::OnOK,     this);
    btnCancel->Bind(wxEVT_BUTTON, &PreferencesDlg::OnCancel, this);
}

void PreferencesDlg::populateFromPrefs() {
    if (m_txtMinZoom)   m_txtMinZoom  ->SetValue(wxString::Format("%.4g", m_prefs.minZoom));
    if (m_txtMaxZoom)   m_txtMaxZoom  ->SetValue(wxString::Format("%.4g", m_prefs.maxZoom));
    if (m_txtResetZoom) m_txtResetZoom->SetValue(wxString::Format("%.4g", m_prefs.resetZoom));
    if (m_spinSpacing)   m_spinSpacing  ->SetValue(m_prefs.gridSpacing);
    if (m_spinDivisions) m_spinDivisions->SetValue(m_prefs.gridDivisions);
    if (m_spinUndo)      m_spinUndo     ->SetValue(m_prefs.undoDepth);
    if (m_chkSnap)       m_chkSnap      ->SetValue(m_prefs.snapEnabled);
    if (m_txtSnapRadius) m_txtSnapRadius->SetValue(wxString::Format("%.4g", m_prefs.snapRadius));
    if (m_txtSoldatDir)  m_txtSoldatDir ->SetValue(m_prefs.soldatDir);
    if (m_txtPrefabsDir) m_txtPrefabsDir->SetValue(m_prefs.prefabsDir);
    if (m_txtUncompDir)  m_txtUncompDir ->SetValue(m_prefs.uncompDir);
    if (m_txtGridAlpha1) m_txtGridAlpha1->SetValue(wxString::Format("%.4g", m_prefs.gridAlpha1 * 100.0f));
    if (m_txtGridAlpha2) m_txtGridAlpha2->SetValue(wxString::Format("%.4g", m_prefs.gridAlpha2 * 100.0f));
    if (m_choPolySrc)  m_choPolySrc ->SetSelection(m_prefs.polyBlendSrc);
    if (m_choPolyDest) m_choPolyDest->SetSelection(m_prefs.polyBlendDest);
    if (m_choWireSrc)  m_choWireSrc ->SetSelection(m_prefs.wireBlendSrc);
    if (m_choWireDest) m_choWireDest->SetSelection(m_prefs.wireBlendDest);
}

void PreferencesDlg::applyToPrefs() {
    double v = 0;
    if (m_txtMinZoom && m_txtMinZoom->GetValue().ToDouble(&v))     m_prefs.minZoom      = static_cast<float>(v);
    if (m_txtMaxZoom && m_txtMaxZoom->GetValue().ToDouble(&v))     m_prefs.maxZoom      = static_cast<float>(v);
    if (m_txtResetZoom && m_txtResetZoom->GetValue().ToDouble(&v)) m_prefs.resetZoom    = static_cast<float>(v);
    if (m_spinSpacing)   m_prefs.gridSpacing   = m_spinSpacing->GetValue();
    if (m_spinDivisions) m_prefs.gridDivisions = m_spinDivisions->GetValue();
    if (m_spinUndo)      m_prefs.undoDepth     = m_spinUndo->GetValue();
    if (m_chkSnap)       m_prefs.snapEnabled   = m_chkSnap->GetValue();
    if (m_txtSnapRadius && m_txtSnapRadius->GetValue().ToDouble(&v)) m_prefs.snapRadius = static_cast<float>(v);
    if (m_txtSoldatDir)  m_prefs.soldatDir  = m_txtSoldatDir ->GetValue().ToStdString();
    if (m_txtPrefabsDir) m_prefs.prefabsDir = m_txtPrefabsDir->GetValue().ToStdString();
    if (m_txtUncompDir)  m_prefs.uncompDir  = m_txtUncompDir ->GetValue().ToStdString();
    if (m_txtGridAlpha1 && m_txtGridAlpha1->GetValue().ToDouble(&v)) m_prefs.gridAlpha1 = static_cast<float>(v / 100.0);
    if (m_txtGridAlpha2 && m_txtGridAlpha2->GetValue().ToDouble(&v)) m_prefs.gridAlpha2 = static_cast<float>(v / 100.0);
    if (m_choPolySrc  && m_choPolySrc ->GetSelection() != wxNOT_FOUND) m_prefs.polyBlendSrc  = m_choPolySrc ->GetSelection();
    if (m_choPolyDest && m_choPolyDest->GetSelection() != wxNOT_FOUND) m_prefs.polyBlendDest = m_choPolyDest->GetSelection();
    if (m_choWireSrc  && m_choWireSrc ->GetSelection() != wxNOT_FOUND) m_prefs.wireBlendSrc  = m_choWireSrc ->GetSelection();
    if (m_choWireDest && m_choWireDest->GetSelection() != wxNOT_FOUND) m_prefs.wireBlendDest = m_choWireDest->GetSelection();
}

/*
 * frmPreferences validates the paths before accepting them (frm:2121-2158):
 * the game directory must exist and must hold Maps, Textures and Scenery-gfx,
 * and the prefabs and uncompiled directories must exist.  Each failure shows
 * a message and leaves the dialog open rather than storing a path that
 * nothing will resolve against.
 */
/*
 * The texture/scenery resolver matches directory names case-insensitively, so
 * the check here has to as well: a Soldat install unpacked on a case-sensitive
 * filesystem often ends up with 'maps' or 'scenery-gfx'.
 */
static bool hasSubdir(const wxString& dir, const wxString& name) {
    if (wxFileName::DirExists(wxFileName(dir, name).GetFullPath())) return true;
    wxDir d(dir);
    if (!d.IsOpened()) return false;
    wxString found;
    for (bool more = d.GetFirst(&found, wxEmptyString, wxDIR_DIRS); more;
         more = d.GetNext(&found)) {
        if (found.IsSameAs(name, false)) return true;
    }
    return false;
}

bool PreferencesDlg::ValidatePaths() {
    auto complain = [this](const wxString& msg) {
        wxMessageBox(msg, "Preferences", wxOK | wxICON_EXCLAMATION, this);
        return false;
    };

    const wxString dir = m_txtSoldatDir ? m_txtSoldatDir->GetValue() : wxString();
    if (!dir.empty() && dir.ToStdString() != m_prefs.soldatDir) {
        if (!wxFileName::DirExists(dir))
            return complain("Soldat directory does not exist.");
        for (const char* sub : {"Maps", "Textures", "Scenery-gfx"}) {
            if (!hasSubdir(dir, sub))
                return complain(wxString::Format(
                    "'%s' folder does not exist in Soldat directory.", sub));
        }
    }

    const wxString prefabs = m_txtPrefabsDir ? m_txtPrefabsDir->GetValue() : wxString();
    if (!prefabs.empty() && !wxFileName::DirExists(prefabs))
        return complain("Prefabs directory does not exist.");

    const wxString uncomp = m_txtUncompDir ? m_txtUncompDir->GetValue() : wxString();
    if (!uncomp.empty() && !wxFileName::DirExists(uncomp))
        return complain("Uncompiled Maps directory does not exist.");

    return true;
}

void PreferencesDlg::OnOK(wxCommandEvent& /*e*/) {
    if (!ValidatePaths()) return;
    applyToPrefs();
    EndModal(wxID_OK);
}

void PreferencesDlg::OnCancel(wxCommandEvent& /*e*/) {
    EndModal(wxID_CANCEL);
}
