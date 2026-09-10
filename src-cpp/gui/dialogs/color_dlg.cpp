/*
 * color_dlg.cpp — Port of frmColor.frm
 *
 * Implements a HSV spectrum picker + value/alpha sliders + RGB/hex fields.
 */
#include "color_dlg.h"

#include <wx/sizer.h>
#include <wx/dcclient.h>
#include <wx/dcbuffer.h>
#include <wx/stattext.h>
#include <wx/button.h>
#include <wx/image.h>

#include <algorithm>
#include <cmath>

static const wxColour BG(0x31, 0x3C, 0x4A);

/* ── HSV ↔ RGB helpers ─────────────────────────────────────────────── */

void ColorDlg::rgbToHsv(int r, int g, int b, float& h, float& s, float& v) {
    float rf = r / 255.0f, gf = g / 255.0f, bf = b / 255.0f;
    float mx = std::max({rf, gf, bf}), mn = std::min({rf, gf, bf});
    float delta = mx - mn;
    v = mx;
    s = (mx == 0.0f) ? 0.0f : delta / mx;
    if (delta == 0.0f) { h = 0.0f; return; }
    if (mx == rf)      h = std::fmod((gf - bf) / delta + 6.0f, 6.0f) / 6.0f;
    else if (mx == gf) h = ((bf - rf) / delta + 2.0f) / 6.0f;
    else               h = ((rf - gf) / delta + 4.0f) / 6.0f;
}

void ColorDlg::hsvToRgb(float h, float s, float v, int& r, int& g, int& b) {
    float hh = h * 6.0f;
    int   i  = static_cast<int>(hh) % 6;
    float f  = hh - std::floor(hh);
    float p  = v * (1 - s);
    float q  = v * (1 - s * f);
    float t  = v * (1 - s * (1 - f));
    float rf, gf, bf;
    switch (i) {
        case 0: rf=v; gf=t; bf=p; break;
        case 1: rf=q; gf=v; bf=p; break;
        case 2: rf=p; gf=v; bf=t; break;
        case 3: rf=p; gf=q; bf=v; break;
        case 4: rf=t; gf=p; bf=v; break;
        default: rf=v; gf=p; bf=q; break;
    }
    r = static_cast<int>(std::round(rf * 255));
    g = static_cast<int>(std::round(gf * 255));
    b = static_cast<int>(std::round(bf * 255));
}

/* ── SpectrumPanel ──────────────────────────────────────────────────── */

ColorDlg::SpectrumPanel::SpectrumPanel(wxWindow* parent, ColorDlg* owner)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(255, 255))
    , m_owner(owner)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    Bind(wxEVT_PAINT,        &SpectrumPanel::OnPaint, this);
    Bind(wxEVT_LEFT_DOWN,    &SpectrumPanel::OnMouse, this);
    Bind(wxEVT_LEFT_UP,      &SpectrumPanel::OnMouse, this);
    Bind(wxEVT_MOTION,       &SpectrumPanel::OnMouse, this);
    rebuildBitmap();
}

void ColorDlg::SpectrumPanel::SetHSV(float h, float s, float v) {
    m_hue = h; m_sat = s; m_val = v;
    rebuildBitmap();
    Refresh();
}

void ColorDlg::SpectrumPanel::rebuildBitmap() {
    const int W = 255, H = 255;
    wxImage img(W, H);
    unsigned char* data = img.GetData();

    for (int y = 0; y < H; ++y) {
        float sat = 1.0f - static_cast<float>(y) / (H - 1);
        for (int x = 0; x < W; ++x) {
            float hue = static_cast<float>(x) / (W - 1);
            int r, g, b;
            ColorDlg::hsvToRgb(hue, sat, m_val, r, g, b);
            int idx = (y * W + x) * 3;
            data[idx]   = static_cast<unsigned char>(r);
            data[idx+1] = static_cast<unsigned char>(g);
            data[idx+2] = static_cast<unsigned char>(b);
        }
    }
    m_bmp = wxBitmap(img);
}

void ColorDlg::SpectrumPanel::OnPaint(wxPaintEvent& /*e*/) {
    wxAutoBufferedPaintDC dc(this);
    if (m_bmp.IsOk()) dc.DrawBitmap(m_bmp, 0, 0);

    /* Draw crosshair at current hue/sat */
    const int W = GetClientSize().x, H = GetClientSize().y;
    int cx = static_cast<int>(m_hue * (W - 1));
    int cy = static_cast<int>((1.0f - m_sat) * (H - 1));
    dc.SetPen(wxPen(*wxWHITE, 1));
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    dc.DrawCircle(cx, cy, 5);
}

void ColorDlg::SpectrumPanel::OnMouse(wxMouseEvent& e) {
    if (!e.LeftIsDown()) return;
    const auto sz = GetClientSize();
    float h = std::clamp(static_cast<float>(e.GetX()) / (sz.x - 1), 0.0f, 1.0f);
    float s = 1.0f - std::clamp(static_cast<float>(e.GetY()) / (sz.y - 1), 0.0f, 1.0f);
    m_owner->setFromHSV(h, s, m_owner->m_v, m_owner->m_a);
    m_owner->updateAll();
}

/* ── GradientBar ────────────────────────────────────────────────────── */

ColorDlg::GradientBar::GradientBar(wxWindow* parent, ColorDlg* owner, bool isAlpha)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(20, 255))
    , m_owner(owner), m_isAlpha(isAlpha)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    Bind(wxEVT_PAINT,     &GradientBar::OnPaint, this);
    Bind(wxEVT_LEFT_DOWN, &GradientBar::OnMouse, this);
    Bind(wxEVT_LEFT_UP,   &GradientBar::OnMouse, this);
    Bind(wxEVT_MOTION,    &GradientBar::OnMouse, this);
}

void ColorDlg::GradientBar::SetValue(float v) {
    m_value = v;
    Refresh();
}

void ColorDlg::GradientBar::OnPaint(wxPaintEvent& /*e*/) {
    wxAutoBufferedPaintDC dc(this);
    const auto sz = GetClientSize();
    const int W = sz.x, H = sz.y;
    for (int y = 0; y < H; ++y) {
        float t = 1.0f - static_cast<float>(y) / (H - 1);
        wxColour col;
        if (m_isAlpha) {
            int a = static_cast<int>(t * 255);
            col = wxColour(a, a, a);
        } else {
            int r, g, b;
            ColorDlg::hsvToRgb(m_owner->m_h, m_owner->m_s, t, r, g, b);
            col = wxColour(r, g, b);
        }
        dc.SetPen(wxPen(col, 1));
        dc.DrawLine(0, y, W, y);
    }
    /* Arrow indicator */
    int arrowY = static_cast<int>((1.0f - m_value) * (H - 1));
    dc.SetPen(wxPen(*wxWHITE, 1));
    dc.DrawLine(0, arrowY, W, arrowY);
}

void ColorDlg::GradientBar::OnMouse(wxMouseEvent& e) {
    if (!e.LeftIsDown()) return;
    const int H = GetClientSize().y;
    float t = 1.0f - std::clamp(static_cast<float>(e.GetY()) / (H - 1), 0.0f, 1.0f);
    m_value = t;
    if (m_isAlpha)
        m_owner->m_a = t;
    else
        m_owner->m_v = t;
    m_owner->updateAll();
}

/* ── ColorDlg ───────────────────────────────────────────────────────── */

ColorDlg::ColorDlg(wxWindow* parent, unsigned int color)
    : wxDialog(parent, wxID_ANY, "Color",
               wxDefaultPosition, wxDefaultSize,
               wxDEFAULT_DIALOG_STYLE)
    , m_color(color)
{
    int r = (color >> 16) & 0xFF;
    int g = (color >>  8) & 0xFF;
    int b = (color >>  0) & 0xFF;
    int a = (color >> 24) & 0xFF;
    rgbToHsv(r, g, b, m_h, m_s, m_v);
    m_a = a / 255.0f;

    SetBackgroundColour(BG);
    buildUI();
    updateAll();
}

void ColorDlg::buildUI() {
    auto* outer = new wxBoxSizer(wxVERTICAL);
    auto* row1  = new wxBoxSizer(wxHORIZONTAL);

    /* Spectrum picker */
    m_spectrum = new SpectrumPanel(this, this);
    row1->Add(m_spectrum, 0, wxALL, 4);

    /* Gradient bars (value + alpha) */
    auto* bars = new wxBoxSizer(wxHORIZONTAL);
    m_valBar   = new GradientBar(this, this, false);
    m_alphaBar = new GradientBar(this, this, true);
    bars->Add(m_valBar,   0, wxALL, 2);
    bars->Add(m_alphaBar, 0, wxALL, 2);
    row1->Add(bars, 0, wxALL, 4);

    /* Color swatch + labels */
    auto* infoCol = new wxBoxSizer(wxVERTICAL);
    m_colorSwatch = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(64, 64));
    infoCol->Add(m_colorSwatch, 0, wxALL, 4);

    auto addField = [&](const wxString& label, wxTextCtrl*& ctrl) {
        auto* row = new wxBoxSizer(wxHORIZONTAL);
        auto* lbl = new wxStaticText(this, wxID_ANY, label, wxDefaultPosition, wxSize(20, -1));
        lbl->SetForegroundColour(*wxWHITE);
        lbl->SetFont(wxFont(8, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL, false, "Arial"));
        ctrl = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxSize(42, -1));
        ctrl->SetFont(wxFont(8, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL, false, "Arial"));
        ctrl->Bind(wxEVT_TEXT, &ColorDlg::OnRGBText, this);
        row->Add(lbl, 0, wxALIGN_CENTER_VERTICAL);
        row->Add(ctrl, 0);
        infoCol->Add(row, 0, wxALL, 2);
    };
    addField("R:", m_txtR);
    addField("G:", m_txtG);
    addField("B:", m_txtB);
    addField("A:", m_txtA);

    auto* hexRow = new wxBoxSizer(wxHORIZONTAL);
    auto* hexLbl = new wxStaticText(this, wxID_ANY, "#");
    hexLbl->SetForegroundColour(*wxWHITE);
    m_txtHex = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxSize(70, -1));
    m_txtHex->SetFont(wxFont(8, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL, false, "Arial"));
    m_txtHex->Bind(wxEVT_TEXT, &ColorDlg::OnHexText, this);
    hexRow->Add(hexLbl, 0, wxALIGN_CENTER_VERTICAL);
    hexRow->Add(m_txtHex, 0);
    infoCol->Add(hexRow, 0, wxALL, 2);

    row1->Add(infoCol, 0, wxALL, 4);
    outer->Add(row1, 0, wxEXPAND);

    /* OK / Cancel */
    auto* btnRow = new wxBoxSizer(wxHORIZONTAL);
    auto* btnOK     = new wxButton(this, wxID_OK,     "OK");
    auto* btnCancel = new wxButton(this, wxID_CANCEL, "Cancel");
    btnRow->AddStretchSpacer();
    btnRow->Add(btnOK,     0, wxALL, 4);
    btnRow->Add(btnCancel, 0, wxALL, 4);
    outer->Add(btnRow, 0, wxEXPAND | wxALL, 4);

    SetSizer(outer);
    Fit();

    /*
     * GTK only knows a text control's real height once the widget has been
     * realised, so the fit above can come out short and clip the hex row
     * behind the button bar.  Re-fit the first time the dialog is shown.
     */
    Bind(wxEVT_SHOW, [this](wxShowEvent& e) {
        e.Skip();
        if (!e.IsShown() || m_fitted) return;
        m_fitted = true;
        CallAfter([this] {
            const wxSize best = GetBestSize();
            if (best.y > GetSize().y || best.x > GetSize().x)
                SetClientSize(wxSize(std::max(best.x, GetSize().x),
                                     std::max(best.y, GetSize().y)));
        });
    });

    btnOK    ->Bind(wxEVT_BUTTON, &ColorDlg::OnOK,     this);
    btnCancel->Bind(wxEVT_BUTTON, &ColorDlg::OnCancel, this);
}

void ColorDlg::setFromHSV(float h, float s, float v, float a) {
    m_h = h; m_s = s; m_v = v; m_a = a;
}

void ColorDlg::setFromRGB(int r, int g, int b, int a) {
    rgbToHsv(r, g, b, m_h, m_s, m_v);
    m_a = a / 255.0f;
}

void ColorDlg::updateAll() {
    m_updating = true;

    int r, g, b;
    hsvToRgb(m_h, m_s, m_v, r, g, b);
    int a = static_cast<int>(std::round(m_a * 255));
    m_color = (static_cast<unsigned int>(a) << 24) |
              (static_cast<unsigned int>(r) << 16) |
              (static_cast<unsigned int>(g) <<  8) |
              (static_cast<unsigned int>(b));

    /* RGB fields */
    if (m_txtR) m_txtR->SetValue(wxString::Format("%d", r));
    if (m_txtG) m_txtG->SetValue(wxString::Format("%d", g));
    if (m_txtB) m_txtB->SetValue(wxString::Format("%d", b));
    if (m_txtA) m_txtA->SetValue(wxString::Format("%d", a));
    if (m_txtHex) m_txtHex->SetValue(wxString::Format("%02X%02X%02X%02X", a, r, g, b));

    /* Swatch */
    if (m_colorSwatch) {
        m_colorSwatch->SetBackgroundColour(wxColour(r, g, b, a));
        m_colorSwatch->Refresh();
    }

    /* Spectrum + bars */
    if (m_spectrum) m_spectrum->SetHSV(m_h, m_s, m_v);
    if (m_valBar)   { m_valBar->SetValue(m_v); m_valBar->Refresh(); }
    if (m_alphaBar) { m_alphaBar->SetValue(m_a); m_alphaBar->Refresh(); }

    m_updating = false;
}

void ColorDlg::OnRGBText(wxCommandEvent& /*e*/) {
    if (m_updating || !m_txtR || !m_txtG || !m_txtB || !m_txtA) return;
    long r = 0, g = 0, b = 0, a = 255;
    m_txtR->GetValue().ToLong(&r);
    m_txtG->GetValue().ToLong(&g);
    m_txtB->GetValue().ToLong(&b);
    m_txtA->GetValue().ToLong(&a);
    r = std::clamp(r, 0L, 255L);
    g = std::clamp(g, 0L, 255L);
    b = std::clamp(b, 0L, 255L);
    a = std::clamp(a, 0L, 255L);
    setFromRGB(static_cast<int>(r), static_cast<int>(g),
               static_cast<int>(b), static_cast<int>(a));
    updateAll();
}

void ColorDlg::OnHexText(wxCommandEvent& /*e*/) {
    if (m_updating || !m_txtHex) return;
    const wxString hex = m_txtHex->GetValue();
    if (hex.length() < 6) return;
    unsigned long val = 0;
    if (!hex.ToULong(&val, 16)) return;
    int a = 255, r = 0, g = 0, b = 0;
    if (hex.length() == 8) {
        a = (val >> 24) & 0xFF;
        r = (val >> 16) & 0xFF;
        g = (val >>  8) & 0xFF;
        b = (val >>  0) & 0xFF;
    } else {
        r = (val >> 16) & 0xFF;
        g = (val >>  8) & 0xFF;
        b = (val >>  0) & 0xFF;
    }
    setFromRGB(r, g, b, a);
    updateAll();
}

void ColorDlg::OnOK(wxCommandEvent& /*e*/) {
    EndModal(wxID_OK);
}

void ColorDlg::OnCancel(wxCommandEvent& /*e*/) {
    EndModal(wxID_CANCEL);
}
