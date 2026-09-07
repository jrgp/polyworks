/*
 * map_settings_dlg.cpp — Map Settings dialog implementation.
 *
 * Layout (pixels converted from VB6 twips at 96dpi, /15):
 *   Dialog: 360 × 336
 *
 * Controls (approximate positions, matching VB6 layout):
 *   Row 0 (y≈32):  Label "Description", txtDesc  (map name)
 *   Row 1 (y≈64):  Label "Weather", cboWeather
 *   Row 2 (y≈112): Label "Jet Fuel", cboJet, txtJet (custom value)
 *   Row 3 (y≈144): Label "Grenades", cboGrenades
 *   Row 4 (y≈160): Label "Medikits", cboMedikits
 *   Row 5 (y≈184): Label "Texture", cboTexture
 *   Row 6 (y≈216): Label "Top BG Color", color picker 1
 *   Row 7 (y≈256): Label "Bottom BG Color", color picker 2
 *   Bottom (y≈304): OK button, Cancel button
 */
#include "map_settings_dlg.h"

#include <wx/filename.h>
#include <wx/dir.h>
#include <wx/stattext.h>
#include <wx/statbox.h>

#include <algorithm>
#include <cstring>

/* ---- Jet fuel levels (VB6 cboJet entries) ------------------------------ */

const int MapSettingsDlg::kJetValues[] = {0, 12, 45, 95, 190, 320, 800, 32766, -1};
const char* const MapSettingsDlg::kJetNames[] = {
    "None", "Minimal", "Very Low", "Low", "Normal", "High", "Maximum",
    "Infinite", "Custom", nullptr
};

/* ---- Weather options --------------------------------------------------- */
const char* const MapSettingsDlg::kWeatherNames[] = {
    "None", "Rain", "Sandstorm", "Snow", nullptr
};

/* ---- Helpers ----------------------------------------------------------- */

static wxColour argbToWx(uint32_t argb) {
    return wxColour(
        (argb >> 16) & 0xFF,  /* R */
        (argb >>  8) & 0xFF,  /* G */
         argb        & 0xFF   /* B */
    );
}

static uint32_t wxToArgb(const wxColour& c, uint8_t a = 0xFF) {
    return (uint32_t(a)      << 24)
         | (uint32_t(c.Red())   << 16)
         | (uint32_t(c.Green()) <<  8)
         | uint32_t(c.Blue());
}

/* ---- Constructor ------------------------------------------------------- */

MapSettingsDlg::MapSettingsDlg(wxWindow* parent, MapOptions& options,
                                const std::string& skinsPath)
    : wxDialog(parent, wxID_ANY, "Map Settings",
               wxDefaultPosition, wxSize(380, 380),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      m_options(options),
      m_savedOptions(options),
      m_skinsPath(skinsPath)
{
    SetBackgroundColour(wxColour(0x4A, 0x3C, 0x31));
    SetForegroundColour(*wxWHITE);

    auto* outer = new wxBoxSizer(wxVERTICAL);
    auto* grid  = new wxFlexGridSizer(0, 2, 6, 8);
    grid->AddGrowableCol(1, 1);

    auto addLabel = [&](const char* txt) {
        auto* lbl = new wxStaticText(this, wxID_ANY, txt);
        lbl->SetForegroundColour(*wxWHITE);
        grid->Add(lbl, 0, wxALIGN_CENTER_VERTICAL);
    };

    /* Map name */
    addLabel("Map Name:");
    m_mapName = new wxTextCtrl(this, wxID_ANY, options.mapName);
    m_mapName->SetMaxLength(38);
    grid->Add(m_mapName, 1, wxEXPAND);

    /* Weather */
    addLabel("Weather:");
    m_weather = new wxComboBox(this, wxID_ANY, wxEmptyString,
                               wxDefaultPosition, wxDefaultSize,
                               0, nullptr, wxCB_READONLY);
    for (int i = 0; kWeatherNames[i]; ++i)
        m_weather->Append(kWeatherNames[i]);
    m_weather->SetSelection(std::min(options.weather, (uint8_t)3));
    grid->Add(m_weather, 1, wxEXPAND);

    /* Jet fuel */
    addLabel("Jet Fuel:");
    {
        auto* jetRow = new wxBoxSizer(wxHORIZONTAL);
        m_jet = new wxComboBox(this, wxID_ANY, wxEmptyString,
                               wxDefaultPosition, wxDefaultSize,
                               0, nullptr, wxCB_READONLY);
        for (int i = 0; kJetNames[i]; ++i)
            m_jet->Append(kJetNames[i]);
        jetRow->Add(m_jet, 1, wxEXPAND);

        m_jetCustom = new wxTextCtrl(this, wxID_ANY,
                                     wxString::Format("%d", options.startJet),
                                     wxDefaultPosition, wxSize(70, -1));
        jetRow->Add(m_jetCustom, 0, wxLEFT, 6);
        grid->Add(jetRow, 1, wxEXPAND);
    }
    syncJetComboFromValue();

    /* Grenades */
    addLabel("Grenade Packs:");
    m_grenades = new wxComboBox(this, wxID_ANY, wxEmptyString,
                                wxDefaultPosition, wxDefaultSize,
                                0, nullptr, wxCB_READONLY);
    for (int i = 0; i <= 4; ++i)
        m_grenades->Append(wxString::Format("%d", i));
    m_grenades->SetSelection(std::min(options.grenadePacks, (uint8_t)4));
    grid->Add(m_grenades, 1, wxEXPAND);

    /* Medikits */
    addLabel("Medikits:");
    m_medikits = new wxComboBox(this, wxID_ANY, wxEmptyString,
                                wxDefaultPosition, wxDefaultSize,
                                0, nullptr, wxCB_READONLY);
    for (int i = 0; i <= 4; ++i)
        m_medikits->Append(wxString::Format("%d", i));
    m_medikits->SetSelection(std::min(options.medikits, (uint8_t)4));
    grid->Add(m_medikits, 1, wxEXPAND);

    /* Texture */
    addLabel("Texture:");
    m_texture = new wxComboBox(this, wxID_ANY, options.textureName);
    grid->Add(m_texture, 1, wxEXPAND);
    populateTextureList();

    /* Background colors */
    addLabel("Top BG Color:");
    m_bgColor1 = new wxColourPickerCtrl(this, wxID_ANY,
                                         argbToWx(options.bgColor1));
    grid->Add(m_bgColor1, 1, wxEXPAND);

    addLabel("Bottom BG Color:");
    m_bgColor2 = new wxColourPickerCtrl(this, wxID_ANY,
                                         argbToWx(options.bgColor2));
    grid->Add(m_bgColor2, 1, wxEXPAND);

    outer->Add(grid, 1, wxEXPAND | wxALL, 12);

    /* OK / Cancel buttons */
    auto* btnSizer = new wxStdDialogButtonSizer();
    auto* btnOK    = new wxButton(this, wxID_OK, "OK");
    auto* btnCancel= new wxButton(this, wxID_CANCEL, "Cancel");
    btnSizer->AddButton(btnOK);
    btnSizer->AddButton(btnCancel);
    btnSizer->Realize();
    outer->Add(btnSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);

    SetSizer(outer);
    Fit();
    Centre();

    Bind(wxEVT_COMBOBOX, &MapSettingsDlg::onJetComboChange, this, m_jet->GetId());
    Bind(wxEVT_TEXT, &MapSettingsDlg::onJetTextChange, this, m_jetCustom->GetId());
    Bind(wxEVT_BUTTON, &MapSettingsDlg::onOK, this, wxID_OK);
    Bind(wxEVT_BUTTON, &MapSettingsDlg::onCancel, this, wxID_CANCEL);
}

/* ---- Texture list ------------------------------------------------------- */

void MapSettingsDlg::populateTextureList() {
    m_texture->Clear();

    /* Enumerate .bmp/.png files in the textures folder */
    wxArrayString files;
    if (!m_skinsPath.empty()) {
        /* skinsPath is e.g. ".../skins/default"; textures one level up */
        wxString textureDir = wxFileName(m_skinsPath).GetPath();
        textureDir = wxFileName(textureDir, "textures").GetFullPath();

        if (wxFileName::DirExists(textureDir)) {
            wxDir::GetAllFiles(textureDir, &files, "*.bmp", wxDIR_FILES);
            wxDir::GetAllFiles(textureDir, &files, "*.png", wxDIR_FILES);
        }

        /* Also look in the skins/default directory itself */
        wxDir::GetAllFiles(m_skinsPath, &files, "*.bmp", wxDIR_FILES);
    }

    /* Add just filenames (not full paths) */
    wxArrayString names;
    for (const auto& f : files) {
        wxString name = wxFileName(f).GetFullName();
        if (names.Index(name) == wxNOT_FOUND)
            names.Add(name);
    }
    names.Sort();
    for (const auto& n : names)
        m_texture->Append(n);

    /* Ensure the current texture name is in the list */
    if (!m_options.textureName.empty()) {
        wxString cur(m_options.textureName);
        if (m_texture->FindString(cur) == wxNOT_FOUND)
            m_texture->Insert(cur, 0);
        m_texture->SetValue(cur);
    }
}

/* ---- Jet sync ---------------------------------------------------------- */

void MapSettingsDlg::syncJetComboFromValue() {
    long val = 0;
    m_jetCustom->GetValue().ToLong(&val);
    for (int i = 0; kJetValues[i] >= 0; ++i) {
        if (kJetValues[i] == (int)val) {
            m_jet->SetSelection(i);
            m_jetCustom->Enable(false);
            return;
        }
    }
    /* Custom */
    m_jet->SetSelection(8);
    m_jetCustom->Enable(true);
}

void MapSettingsDlg::onJetComboChange(wxCommandEvent&) {
    int sel = m_jet->GetSelection();
    if (sel >= 0 && sel < 8) {
        m_jetCustom->SetValue(wxString::Format("%d", kJetValues[sel]));
        m_jetCustom->Enable(false);
    } else {
        m_jetCustom->Enable(true);
    }
}

void MapSettingsDlg::onJetTextChange(wxCommandEvent&) {
    syncJetComboFromValue();
}

/* ---- OK / Cancel ------------------------------------------------------- */

void MapSettingsDlg::onOK(wxCommandEvent&) {
    m_options.mapName = m_mapName->GetValue().ToStdString();
    if (m_options.mapName.size() > 38) m_options.mapName.resize(38);

    m_options.weather      = static_cast<uint8_t>(std::max(0, m_weather->GetSelection()));
    m_options.grenadePacks = static_cast<uint8_t>(std::max(0, m_grenades->GetSelection()));
    m_options.medikits     = static_cast<uint8_t>(std::max(0, m_medikits->GetSelection()));
    m_options.textureName  = m_texture->GetValue().ToStdString();

    long jet = 190;
    m_jetCustom->GetValue().ToLong(&jet);
    m_options.startJet = static_cast<int32_t>(jet);

    m_options.bgColor1 = wxToArgb(m_bgColor1->GetColour());
    m_options.bgColor2 = wxToArgb(m_bgColor2->GetColour());

    EndModal(wxID_OK);
}

void MapSettingsDlg::onCancel(wxCommandEvent&) {
    m_options = m_savedOptions;  /* restore */
    EndModal(wxID_CANCEL);
}
