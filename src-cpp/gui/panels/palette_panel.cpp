#include "palette_panel.h"

#include "../dialogs/color_dlg.h"

#include <wx/dcclient.h>
#include <wx/dcbuffer.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/menu.h>
#include <wx/filedlg.h>
#include <wx/msgdlg.h>
#include <wx/filename.h>
#include <wx/stdpaths.h>

#include <algorithm>
#include <vector>

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

    /* The original marks the selected cell with two overlaid shapes: shpSel2,
       a 16x16 black rectangle on the cell, and shpSel1, a 14x14 white one
       inset by a pixel (frmPalette.frm:278-292, positioned at frm:646).  The
       double outline is what makes the marker visible over both light and
       dark swatches. */
    if (m_selCol >= 0 && m_selRow >= 0) {
        const int x = m_selCol * 16;
        const int y = m_selRow * 16;
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.SetPen(wxPen(*wxBLACK, 1));
        dc.DrawRectangle(x, y, 16, 16);
        dc.SetPen(wxPen(*wxWHITE, 1));
        dc.DrawRectangle(x + 1, y + 1, 14, 14);
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

/* mnuNewColor > "Add to Palette" (frmPalette.frm:527-532) */
static const int kIdAddToPalette = wxID_HIGHEST + 501;

/* VB6 uses `appPath & "\\palettes\\"` for every palette file operation
   (frmPalette.frm:867, 904, 923; modConfig.bas:389).  `appPath` is
   `App.Path`, i.e. the directory holding the executable, so the palette
   directory travels with a portable installation. */
wxString PalettePanel::PalettesDir() {
    auto dirBelow = [](const wxString& base,
                       const std::vector<wxString>& segments) {
        wxFileName fn = wxFileName::DirName(base);
        for (const wxString& seg : segments) fn.AppendDir(seg);
        return fn.GetPath();
    };

    const wxString exeDir =
        wxFileName(wxStandardPaths::Get().GetExecutablePath()).GetPath();
    const wxString exeRelative = dirBelow(exeDir, {"palettes"});
    if (wxFileName::DirExists(exeRelative)) return exeRelative;

    /* Bundled layout: PolyWorks.app/Contents/Resources/palettes. */
    const wxString bundleRelative =
        dirBelow(wxStandardPaths::Get().GetResourcesDir(), {"palettes"});
    if (wxFileName::DirExists(bundleRelative)) return bundleRelative;

    /* Installed layout: <prefix>/bin/polyworks. */
    const wxString installRelative =
        dirBelow(wxFileName(exeDir, wxEmptyString).GetPath(),
                 {"share", "polyworks", "palettes"});
    if (wxFileName::DirExists(installRelative)) return installRelative;

    /* Development checkout: run from the repository root or from build/. */
    const wxString cwd = wxFileName::GetCwd();
    for (const std::vector<wxString>& rel :
         {std::vector<wxString>{"installer", "palettes"},
          std::vector<wxString>{"palettes"},
          std::vector<wxString>{"..", "installer", "palettes"}}) {
        const wxString candidate = dirBelow(cwd, rel);
        if (wxFileName::DirExists(candidate)) return candidate;
    }

    /* Nothing exists yet: SaveCurrentPalette creates the exe-relative one. */
    return exeRelative;
}

wxString PalettePanel::CurrentPalettePath() {
    const wxString dir = PalettesDir();
    if (dir.empty()) return {};
    return dir + wxFILE_SEP_PATH + "current.txt";
}

PalettePanel::PalettePanel(wxWindow* parent)
    : wxFrame(parent, wxID_ANY, "Color Palette",
              wxDefaultPosition, wxSize(208, 272),
              wxFRAME_FLOAT_ON_PARENT | wxCAPTION | wxCLOSE_BOX |
              wxFRAME_NO_TASKBAR | wxRESIZE_BORDER)
{
    SetBackgroundColour(kBg);
    BuildUI();
    /* frmPalette.Form_Load restores the working palette (frm:867). */
    LoadCurrentPalette();
}

void PalettePanel::LoadCurrentPalette() {
    const wxString path = CurrentPalettePath();
    if (!path.empty() && wxFileExists(path))
        m_grid->Load(path);
}

/* modConfig.bas:389 writes the working palette back out as part of
   SaveConfig, so edits survive a restart. */
void PalettePanel::SaveCurrentPalette() const {
    const wxString dir = PalettesDir();
    if (dir.empty()) return;
    if (!wxFileName::DirExists(dir) && !wxFileName::Mkdir(dir, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL))
        return;
    m_grid->Save(dir + wxFILE_SEP_PATH + "current.txt");
}

void PalettePanel::BuildUI() {
    /* frmPalette is 208x272 and lays its controls out in two columns
       (frmPalette.frm:20-449):

         left   x=8    the 63x63 colour swatch, then R:, G:, B:
         right  x=80   "Vertex Color:", the three colour-mode buttons,
                       then Radius:, Opacity: and the Mode: combo
         bottom x=8    the 192x96 palette grid

       Sizers rather than the original's absolute positions, because a desktop
       font is not Arial 8.25 at 96dpi, but the same grouping - the panel had
       been one tall single column, which is both unlike the original and
       needlessly deep. */
    auto* mainSz = new wxBoxSizer(wxVERTICAL);
    auto* topSz  = new wxBoxSizer(wxHORIZONTAL);

    auto makeLabel = [this](const wxString& text) {
        auto* lbl = new wxStaticText(this, wxID_ANY, text);
        lbl->SetForegroundColour(kWhite);
        lbl->SetBackgroundColour(kBg);
        return lbl;
    };
    auto makeField = [this](wxTextCtrl*& field, const wxString& val) {
        field = new wxTextCtrl(this, wxID_ANY, val,
                               wxDefaultPosition, wxSize(48, -1), wxTE_PROCESS_ENTER);
        return field;
    };

    /* ---- Left column: swatch over R / G / B ----------------------------- */
    auto* leftSz = new wxBoxSizer(wxVERTICAL);

    m_swatch = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(63, 63));
    m_swatch->SetBackgroundColour(*wxBLACK);
    leftSz->Add(m_swatch, 0, wxBOTTOM, 6);

    const char* rgbLabels[] = { "R:", "G:", "B:" };
    wxTextCtrl** rgbFields[] = { &m_txtR, &m_txtG, &m_txtB };
    for (int i = 0; i < 3; ++i) {
        auto* rowSz = new wxBoxSizer(wxHORIZONTAL);
        rowSz->Add(makeLabel(rgbLabels[i]), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
        rowSz->Add(makeField(*rgbFields[i], "0"), 0, wxALIGN_CENTER_VERTICAL);
        leftSz->Add(rowSz, 0, wxBOTTOM, 2);
    }
    topSz->Add(leftSz, 0, wxALL, 4);

    /* ---- Right column ---------------------------------------------------- */
    auto* rightSz = new wxBoxSizer(wxVERTICAL);

    /* lblPal(6), the heading over the colour-mode buttons (frm:301-320). */
    rightSz->Add(makeLabel("Vertex Color:"), 0, wxBOTTOM, 4);

    const char* modeLabels[] = { "Precision", "Normal", "Dynamic" };
    for (int i = 0; i < 3; ++i) {
        auto* rowSz = new wxBoxSizer(wxHORIZONTAL);

        m_colorMode[i] = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(16, 16));
        m_colorMode[i]->SetBackgroundColour(kBg);
        m_colorMode[i]->SetClientData(reinterpret_cast<void*>(static_cast<intptr_t>(i)));
        m_colorMode[i]->Bind(wxEVT_LEFT_UP, &PalettePanel::OnColorModeClicked, this);
        /* frmPalette shows which mode is active by drawing the picColorMode
           button pressed (frm:632).  Nothing but the panel colour said so
           here, and against the window background that was invisible, so the
           indicator is drawn explicitly. */
        m_colorMode[i]->Bind(wxEVT_PAINT, [this, i](wxPaintEvent&) {
            wxPaintDC dc(m_colorMode[i]);
            const wxSize sz = m_colorMode[i]->GetClientSize();
            const bool on = (m_colorModeIdx == i);
            dc.SetBrush(wxBrush(on ? kLblBack : kBg));
            dc.SetPen(wxPen(kLblBack));
            dc.DrawRectangle(0, 0, sz.x, sz.y);
            if (on) {
                dc.SetBrush(wxBrush(kWhite));
                dc.SetPen(*wxTRANSPARENT_PEN);
                dc.DrawRectangle(sz.x / 2 - 3, sz.y / 2 - 3, 6, 6);
            }
        });

        rowSz->Add(m_colorMode[i], 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
        rowSz->Add(makeLabel(modeLabels[i]), 0, wxALIGN_CENTER_VERTICAL);
        rightSz->Add(rowSz, 0, wxBOTTOM, 2);
    }

    rightSz->AddSpacer(4);

    auto* radiusSz = new wxBoxSizer(wxHORIZONTAL);
    radiusSz->Add(makeLabel("Radius:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
    radiusSz->AddStretchSpacer();
    radiusSz->Add(makeField(m_txtRadius, "8"), 0, wxALIGN_CENTER_VERTICAL);
    rightSz->Add(radiusSz, 0, wxEXPAND | wxBOTTOM, 2);

    auto* opacitySz = new wxBoxSizer(wxHORIZONTAL);
    opacitySz->Add(makeLabel("Opacity:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
    opacitySz->AddStretchSpacer();
    opacitySz->Add(makeField(m_txtOpacity, "100"), 0, wxALIGN_CENTER_VERTICAL);
    rightSz->Add(opacitySz, 0, wxEXPAND | wxBOTTOM, 2);

    auto* blendSz = new wxBoxSizer(wxHORIZONTAL);
    blendSz->Add(makeLabel("Mode:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
    m_cboBlend = new wxComboBox(this, wxID_ANY, wxEmptyString,
                                wxDefaultPosition, wxDefaultSize,
                                0, nullptr, wxCB_READONLY);
    /* cboBlendMode's six items live in frmPalette.frx at offset 0x16 and are
       the modes ApplyBlend implements (frm:9653): 0 normal, 1 multiply,
       2 screen, 3 darken, 4 lighten, 5 difference.  The panel used to offer
       three items named Normal/Additive/Subtractive, so half the modes were
       unreachable and two of the three were mislabelled - picking "Additive"
       ran multiply. */
    for (const char* name : { "Normal", "Multiply", "Screen",
                              "Darken", "Lighten", "Difference" })
        m_cboBlend->Append(name);
    m_cboBlend->SetSelection(0);
    blendSz->Add(m_cboBlend, 1, wxALIGN_CENTER_VERTICAL);
    rightSz->Add(blendSz, 0, wxEXPAND);

    topSz->Add(rightSz, 1, wxEXPAND | wxALL, 4);
    mainSz->Add(topSz, 0, wxEXPAND);

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

    /* Every numeric box in frmPalette selects its text on focus and validates
       on focus loss (txtRadius/txtRGB/txtOpacity _GotFocus and _LostFocus,
       frm:1014-1106).  Without the focus-loss half, a box could be left
       reading "500" while the value in force was still the old one - the
       control would be lying about the editor's state. */
    struct { wxTextCtrl* ctrl; int lo; int hi; } numeric[] = {
        { m_txtR,       0,   255 },
        { m_txtG,       0,   255 },
        { m_txtB,       0,   255 },
        { m_txtOpacity, 0,   100 },
        { m_txtRadius,  4,   128 },
    };
    for (const auto& n : numeric) {
        wxTextCtrl* ctrl = n.ctrl;
        const int lo = n.lo, hi = n.hi;
        ctrl->Bind(wxEVT_SET_FOCUS, [this, ctrl](wxFocusEvent& e) {
            e.Skip();
            m_focusText = ctrl->GetValue();
            ctrl->CallAfter([ctrl] { ctrl->SelectAll(); });
        });
        ctrl->Bind(wxEVT_KILL_FOCUS, [this, ctrl, lo, hi](wxFocusEvent& e) {
            e.Skip();
            long v = 0;
            if (!ctrl->GetValue().ToLong(&v)) {
                /* Non-numeric or empty: restore what was there on focus-in
                   (txtRGB) - the radius and opacity boxes do the same thing
                   with their stored value, which is that same text. */
                ctrl->ChangeValue(m_focusText);
                return;
            }
            const long clamped = std::min<long>(hi, std::max<long>(lo, v));
            if (clamped != v) ctrl->ChangeValue(wxString::Format("%ld", clamped));
            /* Re-emit so the clamped value actually reaches the viewport. */
            wxCommandEvent evt(wxEVT_TEXT, ctrl->GetId());
            evt.SetEventObject(ctrl);
            evt.SetString(ctrl->GetValue());
            ProcessWindowEvent(evt);
        });
    }

    m_grid->onSelect = [this](int /*col*/, int /*row*/, PaletteColor c) {
        SetValues(c.r, c.g, c.b);
        if (onColorSelected) onColorSelected(c.r, c.g, c.b);
    };
    m_grid->onRightClick = [this](int col, int row) {
        /* picPalette_MouseDown with Button = 2 pops mnuNewColor, whose single
           item "Add to Palette" calls NewPaletteColor (frmPalette.frm:982-996).
           The menu is not decoration: without it a stray right-click would
           overwrite a palette entry with no way to decline. */
        wxMenu menu;
        menu.Append(kIdAddToPalette, "Add to Palette");
        menu.Bind(wxEVT_MENU, [this, col, row](wxCommandEvent&) {
            long r = 0, g = 0, b = 0;
            m_txtR->GetValue().ToLong(&r);
            m_txtG->GetValue().ToLong(&g);
            m_txtB->GetValue().ToLong(&b);
            m_grid->SetColor(col, row, {static_cast<uint8_t>(r),
                                        static_cast<uint8_t>(g),
                                        static_cast<uint8_t>(b)});
            m_grid->SetSelection(col, row);
            SaveCurrentPalette();
        }, kIdAddToPalette);
        m_grid->PopupMenu(&menu);
    };

    /* frmPalette.picColor_Click opens the colour picker on the current colour
       (frmPalette.frm:998).  The original shows frmColor non-modally but
       disables every other window while it is up (frmColor.ChangeColor,
       frm:722-739); a modal dialog is the portable equivalent. */
    m_swatch->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent&) {
        uint8_t r = 0, g = 0, b = 0;
        GetCurrentColor(r, g, b);
        ColorDlg dlg(this, 0xFF000000u | (static_cast<unsigned>(r) << 16) |
                            (static_cast<unsigned>(g) << 8) | b);
        if (dlg.ShowModal() != wxID_OK) return;
        const unsigned c = dlg.GetColor();
        const uint8_t nr = static_cast<uint8_t>((c >> 16) & 0xFF);
        const uint8_t ng = static_cast<uint8_t>((c >> 8) & 0xFF);
        const uint8_t nb = static_cast<uint8_t>(c & 0xFF);
        SetValues(nr, ng, nb);
        CheckPalette(nr, ng, nb);
        if (onColorChanged) onColorChanged(nr, ng, nb);
    });
}

void PaletteGrid::SetSelection(int col, int row) {
    m_selCol = col;
    m_selRow = row;
    Refresh(false);
}

void PalettePanel::CheckPalette(uint8_t r, uint8_t g, uint8_t b) {
    const auto& colors = m_grid->GetColors();
    for (int i = 0; i < 72; ++i) {
        if (colors[i].r == r && colors[i].g == g && colors[i].b == b) {
            m_grid->SetSelection(i % 12, i / 12);
            return;
        }
    }
    m_grid->SetSelection(-1, -1);
}

void PalettePanel::SetValues(uint8_t r, uint8_t g, uint8_t b) {
    m_txtR->ChangeValue(wxString::Format("%d", r));
    m_txtG->ChangeValue(wxString::Format("%d", g));
    m_txtB->ChangeValue(wxString::Format("%d", b));
    SyncSwatchColor();
}

void PalettePanel::Refresh(uint8_t radius, float opacity, int blendMode, uint8_t colorMode) {
    /* frmPalette.RefreshPalette stores the radius and shows it (frm:614), and
       the radius box is clamped to 4..128 (frm:1052). */
    m_txtRadius->ChangeValue(wxString::Format("%d", std::min(128, std::max(4, static_cast<int>(radius)))));
    /* ApplyBlend treats anything outside 0..5 as black (frm:9680), but the
       combo would assert on an out-of-range index, so a hand-edited ini is
       clamped back to Normal rather than being allowed through. */
    if (blendMode < 0 || blendMode >= static_cast<int>(m_cboBlend->GetCount()))
        blendMode = 0;
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

void PalettePanel::GetCurrentColor(uint8_t& r, uint8_t& g, uint8_t& b) const {
    long rv = 0, gv = 0, bv = 0;
    m_txtR->GetValue().ToLong(&rv);
    m_txtG->GetValue().ToLong(&gv);
    m_txtB->GetValue().ToLong(&bv);
    r = static_cast<uint8_t>(rv);
    g = static_cast<uint8_t>(gv);
    b = static_cast<uint8_t>(bv);
}

/* ---- Event handlers ---------------------------------------------------- */

void PalettePanel::OnLoadPalette(wxCommandEvent&) {
    /* frmPalette.frm:904 sets commonDialog.InitDir to <appPath>\palettes. */
    wxFileDialog dlg(this, "Load Palette", PalettesDir(), wxEmptyString,
                     "Text files (*.txt)|*.txt", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() == wxID_OK)
        m_grid->Load(dlg.GetPath());
}

void PalettePanel::OnSavePalette(wxCommandEvent&) {
    /* frmPalette.frm:923 uses the same directory for saving. */
    wxFileDialog dlg(this, "Save Palette", PalettesDir(), "current",
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
