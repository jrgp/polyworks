#pragma once
/*
 * preferences_dlg.h — Port of frmPreferences.frm
 *
 * Modal preferences dialog.
 * Original VB6: ClientWidth=585px, ClientHeight=545px, BackColor=0x4A3C31
 */

#include <wx/dialog.h>
#include <wx/spinctrl.h>
#include <wx/textctrl.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/button.h>
#include <wx/stattext.h>
#include <wx/clrpicker.h>

struct AppPrefs {
    /* Zoom */
    float   minZoom      = 0.0625f;
    float   maxZoom      = 16.0f;
    float   resetZoom    = 1.0f;

    /* Grid */
    int     gridSpacing  = 10;
    int     gridDivisions = 4;
    unsigned int gridColor1 = 0xFF606060;
    unsigned int gridColor2 = 0xFF404040;
    float   gridAlpha1   = 0.5f;
    float   gridAlpha2   = 0.3f;

    /* Snap */
    bool    snapEnabled  = false;
    float   snapRadius   = 8.0f;

    /* Undo */
    int     undoDepth    = 16;

    /* Paths */
    std::string soldatDir;
    std::string prefabsDir;
    std::string uncompDir;

    /* Colors */
    unsigned int pointColor     = 0xFFFFFFFF;
    unsigned int selectionColor = 0xFFFFFF00;
};

class PreferencesDlg final : public wxDialog {
public:
    PreferencesDlg(wxWindow* parent, AppPrefs& prefs);

private:
    void buildUI();
    void populateFromPrefs();
    void applyToPrefs();
    void OnOK(wxCommandEvent& e);
    void OnCancel(wxCommandEvent& e);

    AppPrefs& m_prefs;

    wxTextCtrl*  m_txtMinZoom    = nullptr;
    wxTextCtrl*  m_txtMaxZoom    = nullptr;
    wxTextCtrl*  m_txtResetZoom  = nullptr;
    wxSpinCtrl*  m_spinSpacing   = nullptr;
    wxSpinCtrl*  m_spinDivisions = nullptr;
    wxSpinCtrl*  m_spinUndo      = nullptr;
    wxCheckBox*  m_chkSnap       = nullptr;
    wxTextCtrl*  m_txtSnapRadius = nullptr;
    wxTextCtrl*  m_txtSoldatDir  = nullptr;
    wxTextCtrl*  m_txtPrefabsDir = nullptr;
    wxColourPickerCtrl* m_cpkGrid1     = nullptr;
    wxColourPickerCtrl* m_cpkGrid2     = nullptr;
    wxColourPickerCtrl* m_cpkPoint     = nullptr;
    wxColourPickerCtrl* m_cpkSelection = nullptr;
};
