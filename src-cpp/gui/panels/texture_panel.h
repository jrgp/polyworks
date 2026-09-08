#pragma once
/*
 * texture_panel.h — Port of frmTexture.frm
 *
 * Floating panel showing the map texture with a selectable UV region.
 * Original VB6: ClientWidth=80px, ClientHeight=288px, BackColor=0x4A3C31
 * picTexture: 64×256 at (8,24)
 */

#include <wx/frame.h>
#include <wx/panel.h>

class MainFrame;

class TexturePanel final : public wxFrame {
public:
    explicit TexturePanel(MainFrame* parent);

    /* Load a new texture image (path to BMP/PNG). */
    void SetTexture(const wxString& path);

    /* Set the current UV selection rectangle (0..1 range). */
    void SetTexCoords(float u1, float v1, float u2, float v2);
    /* Normalised rectangle the user has dragged out over the texture; this is
       what mnuCustomX / mnuCustomY feed into Textured Quad creation. */
    bool GetSelection(float& u1, float& v1, float& u2, float& v2) const;

    float GetU1() const { return m_u1; }
    float GetV1() const { return m_v1; }
    float GetU2() const { return m_u2; }
    float GetV2() const { return m_v2; }

private:
    class TexView : public wxPanel {
    public:
        TexView(wxWindow* parent, TexturePanel* owner);
        void SetBitmap(const wxBitmap& bmp);
        void SetSelection(float u1, float v1, float u2, float v2);
    private:
        void OnPaint(wxPaintEvent& e);
        void OnMouse(wxMouseEvent& e);
        TexturePanel* m_owner;
        wxBitmap      m_bmp;
        float m_u1 = 0, m_v1 = 0, m_u2 = 1, m_v2 = 1;
        bool  m_dragging = false;
        int   m_startX = 0, m_startY = 0;
    };

    MainFrame* m_mainFrame;
    TexView*   m_texView = nullptr;
    float m_u1 = 0, m_v1 = 0, m_u2 = 1, m_v2 = 1;
};
