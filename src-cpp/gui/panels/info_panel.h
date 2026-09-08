#pragma once
/*
 * info_panel.h — Port of frmInfo.frm
 *
 * The original is a floating property *editor* with six mutually exclusive
 * `picProp` pages selected via the `mnuProperties` popup (frmInfo.frm:1842
 * `mnuProp_Click`), and auto-switched by `GetInfo` in the main editor form
 * (frmOpenSoldatMapEditor.frm:4668).  The pages are:
 *
 *   0 Light      txtLightProp: Z-coord, Range, Intensity %, picLight colour
 *   1 Map        lblCount(0..6): counts + dimensions
 *   2 Scenery    txtScenProp: Scaling X/Y %, Opacity %, Rotation deg, cboLevel
 *   3 Quad       txtQuadX/txtQuadY: texture quad corners + dimensions
 *   4 Transform  txtRotate, txtScale(0..1)
 *   5 Polygon    cboPolyType, txtBounciness, txtTexture(0..1), txtVertexAlpha
 *
 * Original VB6: ClientWidth=3120, ClientHeight=3120 -> ~208x208 px @96dpi.
 */

#include "map_document.h"

#include <wx/frame.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/choice.h>
#include <wx/panel.h>
#include <wx/button.h>

class MainFrame;
class UndoStack;

class InfoPanel final : public wxFrame {
public:
    /* Page indices match frmInfo.frm's picProp control-array indices. */
    enum Page {
        PAGE_LIGHT     = 0,
        PAGE_MAP       = 1,
        PAGE_SCENERY   = 2,
        PAGE_QUAD      = 3,
        PAGE_TRANSFORM = 4,
        PAGE_POLYGON   = 5,
        PAGE_COUNT     = 6
    };

    InfoPanel(MainFrame* parent, MapDocument& doc, UndoStack* undo = nullptr);

    /* Call after any edit to update displayed values. */
    void Refresh();

    /* VB6 frmInfo.mnuProp_Click: show exactly one page. */
    void ShowPage(int page);
    int  CurrentPage() const { return m_page; }

private:
    void buildUI();
    void buildMapPage(wxPanel* p);
    void buildPolyPage(wxPanel* p);
    void buildSceneryPage(wxPanel* p);
    void buildLightPage(wxPanel* p);
    void buildQuadPage(wxPanel* p);
    void buildTransformPage(wxPanel* p);

    /* VB6 GetInfo (frmOpenSoldatMapEditor.frm:4668) auto-page selection. */
    void autoSelectPage();

    /* Commit handlers - mirror frmInfo's txt*_Change / cbo*_Click. */
    void onPolyApply(wxCommandEvent&);
    void onScenApply(wxCommandEvent&);
    void onLightApply(wxCommandEvent&);
    void onPageMenu(wxCommandEvent&);

    void pushUndo();

    MainFrame*   m_mainFrame;
    MapDocument& m_doc;
    UndoStack*   m_undo   = nullptr;
    int          m_page   = PAGE_MAP;

    /* Set while Refresh() is writing into controls, so the resulting
       wxEVT_TEXT / wxEVT_CHOICE events don't write back into the model.
       This is VB6 frmInfo.noChange (frmOpenSoldatMapEditor.frm:4676). */
    bool         m_noChange = false;

    wxPanel* m_pages[PAGE_COUNT] = {};

    /* Header (always visible) */
    wxStaticText* m_lblIndex  = nullptr;
    wxStaticText* m_lblCoords = nullptr;
    wxButton*     m_btnPage   = nullptr;

    /* Page 1 - map counts */
    wxStaticText* m_lblPolys      = nullptr;
    wxStaticText* m_lblScenery    = nullptr;
    wxStaticText* m_lblSpawns     = nullptr;
    wxStaticText* m_lblColliders  = nullptr;
    wxStaticText* m_lblWaypoints  = nullptr;
    wxStaticText* m_lblConnects   = nullptr;
    wxStaticText* m_lblDimensions = nullptr;

    /* Page 5 - polygon */
    wxChoice*   m_polyType    = nullptr;
    wxTextCtrl* m_bounciness  = nullptr;
    wxTextCtrl* m_textureU    = nullptr;
    wxTextCtrl* m_textureV    = nullptr;
    wxTextCtrl* m_vertexAlpha = nullptr;

    /* Page 2 - scenery */
    wxTextCtrl* m_scenScaleX  = nullptr;
    wxTextCtrl* m_scenScaleY  = nullptr;
    wxTextCtrl* m_scenAlpha   = nullptr;
    wxTextCtrl* m_scenRot     = nullptr;
    wxChoice*   m_scenLevel   = nullptr;

    /* Page 0 - light */
    wxTextCtrl* m_lightZ      = nullptr;
    wxTextCtrl* m_lightRange  = nullptr;
    wxTextCtrl* m_lightInten  = nullptr;
    wxPanel*    m_lightColor  = nullptr;

    /* Page 3 - quad */
    wxTextCtrl* m_quadX0 = nullptr;
    wxTextCtrl* m_quadY0 = nullptr;
    wxTextCtrl* m_quadX1 = nullptr;
    wxTextCtrl* m_quadY1 = nullptr;

    /* Page 4 - transform */
    wxTextCtrl* m_trRotate = nullptr;
    wxTextCtrl* m_trScaleX = nullptr;
    wxTextCtrl* m_trScaleY = nullptr;
};
