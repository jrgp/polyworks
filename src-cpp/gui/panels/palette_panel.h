#pragma once

#include <wx/frame.h>
#include <wx/textctrl.h>
#include <wx/combobox.h>
#include <wx/panel.h>
#include <array>

/* ---- PaletteColor ------------------------------------------------------ */
struct PaletteColor { uint8_t r = 0, g = 0, b = 0; };

/* ---- PaletteGrid: draws a 12x6 grid of clickable color swatches -------- */
class PaletteGrid final : public wxPanel {
public:
    PaletteGrid(wxWindow* parent, wxWindowID id = wxID_ANY);

    /* Get/set the 12×6 grid */
    const std::array<PaletteColor, 72>& GetColors() const { return m_colors; }
    void SetColor(int col, int row, PaletteColor c);
    void Clear();
    void Load(const wxString& path);
    void Save(const wxString& path) const;

    /* Get selected cell (-1 if none) */
    int GetSelCol() const { return m_selCol; }
    int GetSelRow() const { return m_selRow; }
    /* Move the selection marker without firing onSelect; -1 clears it. */
    void SetSelection(int col, int row);

    /* Callback: called when a cell is left-clicked */
    std::function<void(int col, int row, PaletteColor c)> onSelect;
    /* Callback: called when a cell is right-clicked (to set color) */
    std::function<void(int col, int row)> onRightClick;

private:
    void OnPaint(wxPaintEvent&);
    void OnMouse(wxMouseEvent&);

    std::array<PaletteColor, 72> m_colors{};
    int m_selCol = -1, m_selRow = -1;
};

/* ---- PalettePanel: port of frmPalette ---------------------------------- */
class PalettePanel final : public wxFrame {
public:
    explicit PalettePanel(wxWindow* parent);

    /* Called to sync displayed RGB values with an external color source */
    void SetValues(uint8_t r, uint8_t g, uint8_t b);
    /* frmPalette.CheckPalette: park the selection marker on the swatch
       holding this colour, or off the grid when none does (frm:99-121). */
    void CheckPalette(uint8_t r, uint8_t g, uint8_t b);
    /* Refresh grid + controls from current state */
    void Refresh(uint8_t r, float opacity, int blendMode, uint8_t colorMode);

    int  GetRadius() const;
    float GetOpacity() const;
    int  GetBlendMode() const;
    uint8_t GetColorMode() const;
    void GetCurrentColor(uint8_t& r, uint8_t& g, uint8_t& b) const;

    /* <appPath>/palettes — where the original keeps its palette files. */
    static wxString PalettesDir();
    static wxString CurrentPalettePath();
    void LoadCurrentPalette();
    void SaveCurrentPalette() const;

    /* Callbacks into the main editor */
    std::function<void(uint8_t r, uint8_t g, uint8_t b)> onColorSelected;
    std::function<void(uint8_t r, uint8_t g, uint8_t b)> onColorChanged;
    std::function<void(int channel, int value)> onChannelChanged;
    std::function<void(int blendMode)> onBlendModeChanged;
    std::function<void(uint8_t mode)> onColorModeChanged;
    std::function<void(int radius)> onRadiusChanged;

private:
    void BuildUI();
    void SyncSwatchColor();
    void OnLoadPalette(wxCommandEvent&);
    void OnSavePalette(wxCommandEvent&);
    void OnClearPalette(wxCommandEvent&);
    void OnRGBChange(wxCommandEvent& event);
    void OnOpacityChange(wxCommandEvent&);
    void OnRadiusChange(wxCommandEvent&);
    void OnBlendModeChange(wxCommandEvent&);
    void OnColorModeClicked(wxMouseEvent& event);

    PaletteGrid* m_grid         = nullptr;
    wxPanel*     m_swatch        = nullptr; /* current color preview */
    wxTextCtrl*  m_txtR          = nullptr;
    wxTextCtrl*  m_txtG          = nullptr;
    wxTextCtrl*  m_txtB          = nullptr;
    wxTextCtrl*  m_txtOpacity    = nullptr;
    wxTextCtrl*  m_txtRadius     = nullptr;
    wxComboBox*  m_cboBlend      = nullptr;
    wxPanel*     m_colorMode[3]{};  /* Precision, Normal, Dynamic */

    /* ToolSettings/ColorMode defaults to 1 = Normal (modConfig.bas:149). */
    uint8_t      m_colorModeIdx = 1;
    int          m_radius       = 8;
};
