#pragma once
/*
 * map_settings_dlg.h — Map Settings dialog (port of VB6 frmMap).
 *
 * Original: 360×336px (5400/15 × 5040/15), modal, fixed single border.
 * Controls: map name, weather, jet fuel, grenades, medikits,
 *           texture selector (combo), background colors (top/bottom).
 */

#include "map_document.h"

#include <wx/dialog.h>
#include <wx/textctrl.h>
#include <wx/combobox.h>
#include <wx/button.h>
#include <wx/stattext.h>
#include <wx/sizer.h>
#include <wx/colordlg.h>
#include <wx/colourdata.h>
#include <wx/clrpicker.h>
#include <wx/statbmp.h>

#include <string>
#include <vector>

class MapSettingsDlg final : public wxDialog {
public:
    /* soldatDir: the configured game directory; frmMap.LoadTextures enumerates
       <OpenSoldatDir>/textures/*.bmp and *.png (frmMap.frm:550/585).
       skinsPath: installer/skins/default, used as a fallback when no game
       directory has been configured yet. */
    MapSettingsDlg(wxWindow* parent, MapOptions& options,
                   const std::string& skinsPath,
                   const std::string& soldatDir = std::string());

    /* Returns the modified options on OK, original on Cancel */
    const MapOptions& result() const { return m_options; }

private:
    void populateTextureList();
    void updateTexturePreview();
    wxString findTextureFile(const wxString& name) const;
    void syncJetComboFromValue();
    void onJetComboChange(wxCommandEvent&);
    void onJetTextChange(wxCommandEvent&);
    void onOK(wxCommandEvent&);
    void onCancel(wxCommandEvent&);

    MapOptions& m_options;          /* reference to caller's options */
    MapOptions  m_savedOptions;     /* snapshot for Cancel */
    std::string m_skinsPath;
    std::string m_soldatDir;

    wxTextCtrl*  m_mapName   = nullptr;
    wxComboBox*  m_weather   = nullptr;
    wxComboBox*  m_steps     = nullptr;
    wxComboBox*  m_jet       = nullptr;
    wxTextCtrl*  m_jetCustom = nullptr;
    wxComboBox*  m_grenades  = nullptr;
    wxComboBox*  m_medikits  = nullptr;
    wxComboBox*  m_texture   = nullptr;
    wxColourPickerCtrl* m_bgColor1 = nullptr;
    wxColourPickerCtrl* m_bgColor2 = nullptr;
    /* frmMap picTexture: a 128x128 preview of the selected texture. */
    wxStaticBitmap*     m_preview  = nullptr;

    /* Jet-level numeric values matching VB6 */
    static const int kJetValues[];
    static const char* const kJetNames[];
    static const char* const kWeatherNames[];
    static const char* const kStepsNames[];
};
