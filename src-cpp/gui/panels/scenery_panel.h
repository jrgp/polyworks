#pragma once
/*
 * scenery_panel.h — Port of frmScenery.frm
 *
 * Floating modeless panel for scenery placement.
 * - Lists available scenery files
 * - Selects placement level (back/middle/front)
 * - Rotate / scale toggles
 * Original VB6: ClientWidth=208px, ClientHeight=170px, BackColor=0x4A3C31
 */

#include <wx/frame.h>
#include <wx/listbox.h>
#include <wx/stattext.h>
#include <wx/checkbox.h>
#include <wx/radiobut.h>
#include <wx/statbmp.h>

#include <string>
#include <vector>

class MainFrame;

class SceneryPanel final : public wxFrame {
public:
    SceneryPanel(MainFrame* parent, const wxString& soldatPath);

    /* Reload the scenery list from the Soldat Scenery-gfx directory. */
    void ListScenery(const wxString& soldatPath);

    /* Update the list to reflect current map's in-use scenery names. */
    void UpdateInUse(const std::vector<std::string>& names);

    /* Returns the currently selected scenery filename (empty if none). */
    wxString GetSelectedScenery() const;

    /* Placement settings */
    int  GetLevel() const { return m_level; }
    bool GetRotate() const { return m_rotate; }
    bool GetScale() const { return m_scale; }

    /* Used by the viewport's scenery context menu, which offers the same three
       options the panel does; the controls must follow. */
    void SetLevel(int level);
    void SetRotate(bool on);
    void SetScale(bool on);

private:
    void buildUI();
    static wxString NotFoundBitmapPath();
    void OnScenerySelect(wxCommandEvent& event);
    void OnListRightDown(wxMouseEvent& event);
    void OnLevelBack(wxCommandEvent& event);
    void OnLevelMiddle(wxCommandEvent& event);
    void OnLevelFront(wxCommandEvent& event);
    void OnRotate(wxCommandEvent& event);
    void OnScale(wxCommandEvent& event);

    MainFrame*   m_mainFrame;
    wxListBox*   m_lstScenery   = nullptr;
    wxStaticBitmap* m_preview   = nullptr;  /* picScenery */
    wxRadioButton* m_rbBack     = nullptr;
    wxRadioButton* m_rbMiddle   = nullptr;
    wxRadioButton* m_rbFront    = nullptr;
    wxCheckBox*  m_chkRotate    = nullptr;
    wxCheckBox*  m_chkScale     = nullptr;

    int  m_level  = 1;   /* 0=back,1=middle,2=front — matches SCENERY_BACK/MIDDLE/FRONT */
    bool m_rotate = false;
    bool m_scale  = false;
    wxString m_soldatPath;
    std::vector<std::string> m_inUse;
};
