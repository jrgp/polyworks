#pragma once
/*
 * color_dlg.h — Port of frmColor.frm
 *
 * HSV/RGB colour picker dialog.
 * Original VB6: ClientWidth=472px, ClientHeight=376px, BackColor=0x4A3C31
 *
 * Public interface:
 *   - Construct with initial RGBA colour
 *   - Show modal; retrieve result colour after ShowModal() == wxID_OK
 */

#include <wx/dialog.h>
#include <wx/panel.h>
#include <wx/textctrl.h>
#include <wx/slider.h>
#include <wx/stattext.h>
#include <wx/bitmap.h>

class ColorDlg final : public wxDialog {
public:
    /* color: packed 0xAARRGGBB */
    ColorDlg(wxWindow* parent, unsigned int color = 0xFFFFFFFF);

    /* Returns the chosen color as 0xAARRGGBB after wxID_OK. */
    unsigned int GetColor() const { return m_color; }

private:
    /* A 2-D gradient panel for Hue/Saturation selection. */
    class SpectrumPanel : public wxPanel {
    public:
        explicit SpectrumPanel(wxWindow* parent, ColorDlg* owner);
        void SetHSV(float h, float s, float v);
    private:
        void OnPaint(wxPaintEvent& e);
        void OnMouse(wxMouseEvent& e);
        void rebuildBitmap();
        ColorDlg* m_owner;
        float m_hue = 0, m_sat = 1, m_val = 1;
        wxBitmap m_bmp;
    };

    /* A 1-D gradient bar for Value/Alpha. */
    class GradientBar : public wxPanel {
    public:
        GradientBar(wxWindow* parent, ColorDlg* owner, bool isAlpha);
        void SetValue(float v);
        float GetValue() const { return m_value; }
    private:
        void OnPaint(wxPaintEvent& e);
        void OnMouse(wxMouseEvent& e);
        ColorDlg* m_owner;
        bool      m_isAlpha;
        float     m_value = 1.0f;
    };

    void buildUI();
    void updateAll();           /* sync all controls from m_h,m_s,m_v,m_a */
    void setFromRGB(int r, int g, int b, int a = 255);
    void setFromHSV(float h, float s, float v, float a = 1.0f);
    static void rgbToHsv(int r, int g, int b, float& h, float& s, float& v);
    static void hsvToRgb(float h, float s, float v, int& r, int& g, int& b);

    void OnRGBText(wxCommandEvent& e);
    void OnHexText(wxCommandEvent& e);
    void OnOK(wxCommandEvent& e);
    void OnCancel(wxCommandEvent& e);

    friend class SpectrumPanel;
    friend class GradientBar;

    unsigned int  m_color = 0xFFFFFFFF;
    float         m_h = 0, m_s = 1, m_v = 1, m_a = 1;
    bool          m_updating = false;

    SpectrumPanel* m_spectrum   = nullptr;
    GradientBar*   m_valBar     = nullptr;
    GradientBar*   m_alphaBar   = nullptr;
    wxPanel*       m_colorSwatch = nullptr;

    wxTextCtrl*    m_txtR   = nullptr;
    wxTextCtrl*    m_txtG   = nullptr;
    wxTextCtrl*    m_txtB   = nullptr;
    wxTextCtrl*    m_txtA   = nullptr;
    wxTextCtrl*    m_txtHex = nullptr;
};
