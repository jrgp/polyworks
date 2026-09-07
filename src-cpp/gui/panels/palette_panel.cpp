#include "palette_panel.h"

#include <wx/dcclient.h>
#include <wx/dcbuffer.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/menu.h>
#include <wx/filedlg.h>
#include <wx/msgdlg.h>

#include <fstream>
#include <functional>

/* ========================================================================
 * PaletteGrid
 * ======================================================================== */

PaletteGrid::PaletteGrid(wxWindow* parent, wxWindowID id)
    : wxPanel(parent, id, wxDefaultPosition, wxSize(192, 96))
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    Bind(wxEVT_PAINT,         &PaletteGrid::OnPaint, this);
    Bind(wxEVT_LEFT_DOWN,     &PaletteGrid::OnMouse, this);
    Bind(wxEVT_RIGHT_DOWN,    &PaletteGrid::OnMouse, this);
}

void PaletteGrid::SetColor(int col, int row, PaletteColor c) {
    if (col < 0 || col > 11 || row < 0 || row > 5) return;
    m_colors[row * 12 + col] = c;
    Refresh();
}

void PaletteGrid::Clear() {
    m_colors.fill({0, 0, 0});
    m_selCol = m_selRow = -1;
    Refresh();
}

void PaletteGrid::Load(const wxString& path) {
    std::ifstream f(path.ToStdString());
    if (!f.is_open()) return;
    for (int row = 0; row < 6; ++row) {
        for (int col = 0; col < 12; ++col) {
            int r, g, b; char comma;
            if (f >> r >> comma >> g >> comma >> b) {
                m_colors[row * 12 + col] = {
                    static_cast<uint8_t>(r),
                    static_cast<uint8_t>(g),
                    static_cast<uint8_t>(b)
                };
            }
        }
    }
    m_selCol = m_selRow = -1;
    Refresh();
}

void PaletteGrid::Save(const wxString& path) const {
    std::ofstream f(path.ToStdString());
    if (!f.is_open()) return;
    for (int row = 0; row < 6; ++row)
        for (int col = 0; col < 12; ++col) {
            const auto& c = m_colors[row * 12 + col];
            f << (int)c.r << ", " << (int)c.g << ", " << (int)c.b << "\n";
        }
}

void PaletteGrid::OnPaint(wxPaintEvent&) {
    wxAutoBufferedPaintDC dc(this);
    dc.SetBackground(wxBrush(*wxBLACK));
    dc.Clear();

    for (int row = 0; row < 6; ++row) {
        for (int col = 0; col < 12; ++col) {
            const auto& c = m_colors[row * 12 + col];
            dc.SetBrush(wxBrush(wxColour(c.r, c.g, c.b)));
            dc.SetPen(*wxTRANSPARENT_PEN);
            dc.DrawRectangle(col * 16, row * 16, 16, 16);
        }
    }

    /* Draw selection indicator */
    if (m_selCol >= 0 && m_selRow >= 0) {
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.SetPen(wxPen(*wxWHITE, 1));
        dc.DrawRectangle(m_selCol * 16, m_selRow * 16, 16, 16);
    }
}

void PaletteGrid::OnMouse(wxMouseEvent& event) {
    int col = event.GetX() / 16;
    int row = event.GetY() / 16;
    if (col < 0 || col > 11 || row < 0 || row > 5) return;

    if (event.LeftDown()) {
        m_selCol = col; m_selRow = row;
        Refresh();
        if (onSelect) onSelect(col, row, m_colors[row * 12 + col]);
    } else if (event.RightDown()) {
        m_selCol = col; m_selRow = row;
        Refresh();
        if (onRightClick) onRightClick(col, row);
    }
}

/* ========================================================================
 * PalettePanel
 * ======================================================================== */

static const wxColour kBg(0x31, 0x3C, 0x4A);      /* BGR 0x4A3C31 */
static const wxColour kLblBack(0x3D, 0x4B, 0x61);  /* BGR 0x614B3D */
static const wxColour kWhite(*wxWHITE);

PalettePanel::PalettePanel(wxWindow* parent)
    : wxFrame(parent, wxID_ANY, "Color Palette",
              wxDefaultPosition, wxSize(208, 272),
              wxFRAME_FLOAT_ON_PARENT | wxCAPTION | wxCLOSE_BOX |
              wxFRAME_NO_TASKBAR | wxRESIZE_BORDER)
{
    SetBackgroundColour(kBg);
    BuildUI();
}

void PalettePanel::BuildUI() {
    auto* mainSz = new wxBoxSizer(wxVERTICAL);

    /* ---- Top section: swatch + color mode + RGB/radius/opacity ---------- */
    auto* topSz = new wxBoxSizer(wxHORIZONTAL);

    /* Current color swatch (63×63) */
    m_swatch = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(63, 63));
    m_swatch->SetBackgroundColour(*wxBLACK);
    topSz->Add(m_swatch, 0, wxALL, 4);

    /* Right side: color mode + controls */
    auto* rightSz = new wxBoxSizer(wxVERTICAL);

    /* Color mode row (Precision / Normal / Dynamic as toggle panels) */
    const char* modeLabels[] = { "Precision", "Normal", "Dynamic" };
    for (int i = 0; i < 3; ++i) {
        auto* rowSz = new wxBoxSizer(wxHORIZONTAL);

        m_colorMode[i] = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(16, 16));
        m_colorMode[i]->SetBackgroundColour(kBg);
        m_colorMode[i]->SetClientData(reinterpret_cast<void*>(static_cast<intptr_t>(i)));
        m_colorMode[i]->Bind(wxEVT_LEFT_UP, &PalettePanel::OnColorModeClicked, this);

        auto* lbl = new wxStaticText(this, wxID_ANY, modeLabels[i]);
        lbl->SetForegroundColour(kWhite);
        lbl->SetBackgroundColour(kBg);

        rowSz->Add(m_colorMode[i], 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
        rowSz->Add(lbl, 0, wxALIGN_CENTER_VERTICAL);
        rightSz->Add(rowSz, 0, wxBOTTOM, 2);
    }

    /* R, G, B, Opacity, Radius fields */
    auto addField = [&](const wxString& label, wxTextCtrl*& field, const wxString& val) {
        auto* rowSz = new wxBoxSizer(wxHORIZONTAL);
        auto* lbl = new wxStaticText(this, wxID_ANY, label);
        lbl->SetForegroundColour(kWhite);
        lbl->SetBackgroundColour(kBg);
        field = new wxTextCtrl(this, wxID_ANY, val,
                               wxDefaultPosition, wxSize(48, -1), wxTE_PROCESS_ENTER);
        rowSz->Add(lbl, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
        rowSz->Add(field, 0, wxALIGN_CENTER_VERTICAL);
        rightSz->Add(rowSz, 0, wxBOTTOM, 2);
    };

    addField("R:", m_txtR, "0");
    addField("G:", m_txtG, "0");
    addField("B:", m_txtB, "0");
    addField("Opacity:", m_txtOpacity, "100");
    addField("Radius:", m_txtRadius, "8");

    topSz->Add(rightSz, 1, wxEXPAND | wxALL, 4);
    mainSz->Add(topSz, 0, wxEXPAND);

    /* ---- Blend mode combo ---------------------------------------------- */
    auto* blendSz = new wxBoxSizer(wxHORIZONTAL);
    auto* blendLbl = new wxStaticText(this, wxID_ANY, "Mode:");
    blendLbl->SetForegroundColour(kWhite);
    blendLbl->SetBackgroundColour(kBg);

    m_cboBlend = new wxComboBox(this, wxID_ANY, wxEmptyString,
                                wxDefaultPosition, wxSize(96, -1),
                                0, nullptr, wxCB_READONLY);
    m_cboBlend->Append("Normal");
    m_cboBlend->Append("Additive");
    m_cboBlend->Append("Subtractive");
    m_cboBlend->SetSelection(0);

    blendSz->Add(blendLbl, 0, wxALIGN_CENTER_VERTICAL | wxALL, 4);
    blendSz->Add(m_cboBlend, 0, wxALIGN_CENTER_VERTICAL | wxALL, 4);
    mainSz->Add(blendSz, 0, wxEXPAND);

    /* ---- Palette grid -------------------------------------------------- */
    m_grid = new PaletteGrid(this);
    mainSz->Add(m_grid, 0, wxALL, 4);

    SetSizerAndFit(mainSz);

    /* ---- Palette menu -------------------------------------------------- */
    auto* menuBar = new wxMenuBar();
    auto* palMenu = new wxMenu();
    palMenu->Append(wxID_FILE1, "Load Palette...");
    palMenu->Append(wxID_FILE2, "Save Palette...");
    palMenu->AppendSeparator();
    palMenu->Append(wxID_CLEAR, "Clear");
    menuBar->Append(palMenu, "Palette");
    SetMenuBar(menuBar);

    /* ---- Wire events --------------------------------------------------- */
    Bind(wxEVT_MENU, &PalettePanel::OnLoadPalette,  this, wxID_FILE1);
    Bind(wxEVT_MENU, &PalettePanel::OnSavePalette,  this, wxID_FILE2);
    Bind(wxEVT_MENU, &PalettePanel::OnClearPalette, this, wxID_CLEAR);

    Bind(wxEVT_TEXT, &PalettePanel::OnRGBChange,      this, m_txtR->GetId());
    Bind(wxEVT_TEXT, &PalettePanel::OnRGBChange,      this, m_txtG->GetId());
    Bind(wxEVT_TEXT, &PalettePanel::OnRGBChange,      this, m_txtB->GetId());
    Bind(wxEVT_TEXT, &PalettePanel::OnOpacityChange,  this, m_txtOpacity->GetId());
    Bind(wxEVT_TEXT, &PalettePanel::OnRadiusChange,   this, m_txtRadius->GetId());
    Bind(wxEVT_COMBOBOX, &PalettePanel::OnBlendModeChange, this, m_cboBlend->GetId());

    m_grid->onSelect = [this](int /*col*/, int /*row*/, PaletteColor c) {
        SetValues(c.r, c.g, c.b);
        if (onColorSelected) onColorSelected(c.r, c.g, c.b);
    };
    m_grid->onRightClick = [this](int col, int row) {
        /* Set the current RGB into that cell */
        long r = 0, g = 0, b = 0;
        m_txtR->GetValue().ToLong(&r);
        m_txtG->GetValue().ToLong(&g);
        m_txtB->GetValue().ToLong(&b);
        m_grid->SetColor(col, row, {static_cast<uint8_t>(r),
                                    static_cast<uint8_t>(g),
                                    static_cast<uint8_t>(b)});
    };

    /* Swatch click opens color picker (delegate to owner via callback) */
    m_swatch->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent&) {
        long r = 0, g = 0, b = 0;
        m_txtR->GetValue().ToLong(&r);
        m_txtG->GetValue().ToLong(&g);
        m_txtB->GetValue().ToLong(&b);
        if (onColorChanged) onColorChanged(static_cast<uint8_t>(r),
                                            static_cast<uint8_t>(g),
                                            static_cast<uint8_t>(b));
    });
}

void PalettePanel::SetValues(uint8_t r, uint8_t g, uint8_t b) {
    m_txtR->ChangeValue(wxString::Format("%d", r));
    m_txtG->ChangeValue(wxString::Format("%d", g));
    m_txtB->ChangeValue(wxString::Format("%d", b));
    SyncSwatchColor();
}

void PalettePanel::Refresh(uint8_t /*r*/, float opacity, int blendMode, uint8_t colorMode) {
    m_cboBlend->SetSelection(blendMode);
    m_txtOpacity->ChangeValue(wxString::Format("%.0f", opacity * 100.0f));
    m_colorModeIdx = colorMode;
    /* Update color-mode toggle highlights */
    for (int i = 0; i < 3; ++i) {
        m_colorMode[i]->SetBackgroundColour(i == colorMode ? kLblBack : kBg);
        m_colorMode[i]->wxWindow::Refresh();
    }
    SyncSwatchColor();
}

void PalettePanel::SyncSwatchColor() {
    long r = 0, g = 0, b = 0;
    m_txtR->GetValue().ToLong(&r);
    m_txtG->GetValue().ToLong(&g);
    m_txtB->GetValue().ToLong(&b);
    m_swatch->SetBackgroundColour(wxColour(
        static_cast<unsigned char>(r),
        static_cast<unsigned char>(g),
        static_cast<unsigned char>(b)));
    m_swatch->wxWindow::Refresh();
}

int   PalettePanel::GetRadius()    const { long v = 8;  m_txtRadius->GetValue().ToLong(&v); return static_cast<int>(v); }
float PalettePanel::GetOpacity()   const { long v = 100; m_txtOpacity->GetValue().ToLong(&v); return v / 100.0f; }
int   PalettePanel::GetBlendMode() const { return m_cboBlend->GetSelection(); }
uint8_t PalettePanel::GetColorMode() const { return m_colorModeIdx; }

/* ---- Event handlers ---------------------------------------------------- */

void PalettePanel::OnLoadPalette(wxCommandEvent&) {
    wxFileDialog dlg(this, "Load Palette", wxEmptyString, wxEmptyString,
                     "Text files (*.txt)|*.txt", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() == wxID_OK)
        m_grid->Load(dlg.GetPath());
}

void PalettePanel::OnSavePalette(wxCommandEvent&) {
    wxFileDialog dlg(this, "Save Palette", wxEmptyString, "current",
                     "Text files (*.txt)|*.txt", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dlg.ShowModal() == wxID_OK)
        m_grid->Save(dlg.GetPath());
}

void PalettePanel::OnClearPalette(wxCommandEvent&) {
    m_grid->Clear();
}

void PalettePanel::OnRGBChange(wxCommandEvent& event) {
    int channel = -1;
    if (event.GetId() == m_txtR->GetId()) channel = 0;
    else if (event.GetId() == m_txtG->GetId()) channel = 1;
    else if (event.GetId() == m_txtB->GetId()) channel = 2;
    if (channel < 0) return;

    SyncSwatchColor();

    long val = 0;
    event.GetString().ToLong(&val);
    if (val < 0 || val > 255) return;
    if (onChannelChanged) onChannelChanged(channel, static_cast<int>(val));
}

void PalettePanel::OnOpacityChange(wxCommandEvent&) {
    /* Delegate via a synthesized "channel 3" for opacity */
    long v = 0; m_txtOpacity->GetValue().ToLong(&v);
    if (v >= 0 && v <= 100 && onChannelChanged)
        onChannelChanged(3, static_cast<int>(v));
}

void PalettePanel::OnRadiusChange(wxCommandEvent&) {
    long v = 0; m_txtRadius->GetValue().ToLong(&v);
    if (v >= 4 && v <= 128) {
        m_radius = static_cast<int>(v);
        if (onRadiusChanged) onRadiusChanged(m_radius);
    }
}

void PalettePanel::OnBlendModeChange(wxCommandEvent&) {
    if (onBlendModeChanged) onBlendModeChanged(m_cboBlend->GetSelection());
}

void PalettePanel::OnColorModeClicked(wxMouseEvent& event) {
    int idx = static_cast<int>(reinterpret_cast<intptr_t>(
        static_cast<wxPanel*>(event.GetEventObject())->GetClientData()));
    m_colorModeIdx = static_cast<uint8_t>(idx);
    for (int i = 0; i < 3; ++i) {
        m_colorMode[i]->SetBackgroundColour(i == idx ? kLblBack : kBg);
        m_colorMode[i]->wxWindow::Refresh();
    }
    if (onColorModeChanged) onColorModeChanged(m_colorModeIdx);
}
