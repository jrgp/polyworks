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

#include <string>
#include <vector>

class MapSettingsDlg final : public wxDialog {
public:
    /* skinsPath: path to installer/skins/default (for texture enumeration) */
    MapSettingsDlg(wxWindow* parent, MapOptions& options,
                   const std::string& skinsPath);

    /* Returns the modified options on OK, original on Cancel */
    const MapOptions& result() const { return m_options; }

private:
    void populateTextureList();
    void syncJetComboFromValue();
    void onJetComboChange(wxCommandEvent&);
    void onJetTextChange(wxCommandEvent&);
    void onOK(wxCommandEvent&);
    void onCancel(wxCommandEvent&);
    void onBgColor1(wxCommandEvent&);
    void onBgColor2(wxCommandEvent&);

    MapOptions& m_options;          /* reference to caller's options */
    MapOptions  m_savedOptions;     /* snapshot for Cancel */
    std::string m_skinsPath;

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

    /* Jet-level numeric values matching VB6 */
    static const int kJetValues[];
    static const char* const kJetNames[];
    static const char* const kWeatherNames[];
    static const char* const kStepsNames[];
};
