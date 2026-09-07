#pragma once
/*
 * waypoint_panel.h — Port of frmWaypoints.frm
 *
 * Floating modeless panel showing waypoint direction flags, path number,
 * special action, and path visibility toggles.
 * Original VB6: ClientWidth=208px, ClientHeight=160px, BackColor=0x4A3C31
 */

#include <wx/frame.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>

class MainFrame;

class WaypointPanel final : public wxFrame {
public:
    explicit WaypointPanel(MainFrame* parent);

    /* Direction flags for the next placed waypoint */
    bool GetLeft()  const { return m_left; }
    bool GetRight() const { return m_right; }
    bool GetUp()    const { return m_up; }
    bool GetDown()  const { return m_down; }
    bool GetM2()    const { return m_m2; }

    /* Path number (0 = any) */
    int  GetPathNum() const;

    /* Special action index */
    int  GetSpecial() const;

    /* Update UI to reflect a selected waypoint */
    void ShowWaypoint(bool left, bool right, bool up, bool down, bool m2,
                      int pathNum, int special);

    /* Clear all fields (no selection) */
    void Clear();

private:
    void buildUI();
    void OnLeft(wxCommandEvent& e);
    void OnRight(wxCommandEvent& e);
    void OnUp(wxCommandEvent& e);
    void OnDown(wxCommandEvent& e);
    void OnM2(wxCommandEvent& e);

    MainFrame*   m_mainFrame;
    wxCheckBox*  m_chkLeft   = nullptr;
    wxCheckBox*  m_chkRight  = nullptr;
    wxCheckBox*  m_chkUp     = nullptr;
    wxCheckBox*  m_chkDown   = nullptr;
    wxCheckBox*  m_chkM2     = nullptr;
    wxSpinCtrl*  m_spinPath  = nullptr;
    wxChoice*    m_cboSpecial = nullptr;

    bool m_left = false, m_right = false, m_up = false,
         m_down = false, m_m2 = false;
};
