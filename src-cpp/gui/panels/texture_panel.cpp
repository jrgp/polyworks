/*
 * texture_panel.cpp — Port of frmTexture.frm
 */
#include "texture_panel.h"
#include "gui/mainframe.h"

#include <wx/sizer.h>
#include <wx/dcclient.h>
#include <wx/dcbuffer.h>

static const wxColour BG(0x31, 0x3C, 0x4A);

/* ── TexView ───────────────────────────────────────────────────────── */

TexturePanel::TexView::TexView(wxWindow* parent, TexturePanel* owner)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(64, 256))
    , m_owner(owner)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    Bind(wxEVT_PAINT,     &TexView::OnPaint, this);
    Bind(wxEVT_LEFT_DOWN, &TexView::OnMouse, this);
    Bind(wxEVT_LEFT_UP,   &TexView::OnMouse, this);
    Bind(wxEVT_MOTION,    &TexView::OnMouse, this);
}

void TexturePanel::TexView::SetBitmap(const wxBitmap& bmp) {
    m_bmp = bmp;
    Refresh();
}

void TexturePanel::TexView::SetSelection(float u1, float v1, float u2, float v2) {
    m_u1 = u1; m_v1 = v1; m_u2 = u2; m_v2 = v2;
    Refresh();
}

void TexturePanel::TexView::OnPaint(wxPaintEvent& /*e*/) {
    wxAutoBufferedPaintDC dc(this);
    const auto sz = GetClientSize();
    dc.SetBackground(wxBrush(BG));
    dc.Clear();

    if (m_bmp.IsOk()) {
        /* Scale texture to fill the view */
        wxImage img = m_bmp.ConvertToImage();
        img.Rescale(sz.x, sz.y, wxIMAGE_QUALITY_NORMAL);
        dc.DrawBitmap(wxBitmap(img), 0, 0);
    }

    /* Draw UV selection rect */
    int sx1 = static_cast<int>(m_u1 * sz.x);
    int sy1 = static_cast<int>(m_v1 * sz.y);
    int sx2 = static_cast<int>(m_u2 * sz.x);
    int sy2 = static_cast<int>(m_v2 * sz.y);
    dc.SetPen(wxPen(*wxYELLOW, 1));
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    dc.DrawRectangle(sx1, sy1, sx2 - sx1, sy2 - sy1);
}

void TexturePanel::TexView::OnMouse(wxMouseEvent& e) {
    const auto sz = GetClientSize();
    float u = static_cast<float>(e.GetX()) / sz.x;
    float v = static_cast<float>(e.GetY()) / sz.y;
    u = std::clamp(u, 0.0f, 1.0f);
    v = std::clamp(v, 0.0f, 1.0f);

    if (e.LeftDown()) {
        m_dragging = true;
        m_startX = e.GetX(); m_startY = e.GetY();
        m_u1 = u; m_v1 = v; m_u2 = u; m_v2 = v;
        CaptureMouse();
    } else if (e.LeftUp() && m_dragging) {
        m_dragging = false;
        if (HasCapture()) ReleaseMouse();
        m_u2 = u; m_v2 = v;
        if (m_u1 > m_u2) std::swap(m_u1, m_u2);
        if (m_v1 > m_v2) std::swap(m_v1, m_v2);
        m_owner->m_u1 = m_u1; m_owner->m_v1 = m_v1;
        m_owner->m_u2 = m_u2; m_owner->m_v2 = m_v2;
    } else if (e.Dragging() && m_dragging) {
        float u2 = static_cast<float>(e.GetX()) / sz.x;
        float v2 = static_cast<float>(e.GetY()) / sz.y;
        m_u2 = std::clamp(u2, 0.0f, 1.0f);
        m_v2 = std::clamp(v2, 0.0f, 1.0f);
    }
    Refresh();
}

/* ── TexturePanel ──────────────────────────────────────────────────── */

TexturePanel::TexturePanel(MainFrame* parent)
    : wxFrame(parent, wxID_ANY, "Texture",
              wxDefaultPosition, wxDefaultSize,
              wxFRAME_FLOAT_ON_PARENT | wxFRAME_NO_TASKBAR |
              wxSYSTEM_MENU | wxCAPTION | wxCLOSE_BOX | wxCLIP_CHILDREN)
    , m_mainFrame(parent)
{
    SetBackgroundColour(BG);

    m_texView = new TexView(this, this);

    auto* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(m_texView, 0, wxALL, 8);
    SetSizer(sizer);
    Fit();
}

void TexturePanel::SetTexture(const wxString& path) {
    if (!m_texView) return;
    wxImage img(path);
    if (img.IsOk()) {
        m_texView->SetBitmap(wxBitmap(img));
    }
}

void TexturePanel::SetTexCoords(float u1, float v1, float u2, float v2) {
    m_u1 = u1; m_v1 = v1; m_u2 = u2; m_v2 = v2;
    if (m_texView) m_texView->SetSelection(u1, v1, u2, v2);
}
