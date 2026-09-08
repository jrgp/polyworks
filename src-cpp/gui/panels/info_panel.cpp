/*
 * info_panel.cpp — Port of frmInfo.frm
 *
 * See info_panel.h for the page inventory.  Value formatting follows the
 * original exactly (frmOpenSoldatMapEditor.frm:4695-4745 `GetInfo`), including
 * VB6's `Int(x * 100 + 0.5) / 100` style rounding, so displayed numbers match
 * the original editor.
 */

#include "geometry.h"
#include "info_panel.h"
#include "mainframe.h"
#include "undo_stack.h"

#include <wx/sizer.h>
#include <wx/menu.h>
#include <wx/dcclient.h>
#include <wx/colordlg.h>

#include <array>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <vector>

namespace {

/* frmInfo.frx cboPolyType list — same 26 entries as the Polygon Type menu. */
/* frmInfo.frx cboLevel list. */
const std::array<const char*, 3> kLevelNames{{"Back", "Middle", "Front"}};

const char* const kPageNames[InfoPanel::PAGE_COUNT] = {
    "Light", "Map", "Scenery", "Quad", "Transform", "Polygon"
};

constexpr int ID_PAGE_BASE = 5300;

/* VB6 `Int(v * m + 0.5) / m` — round-half-up then truncate toward zero.
   VB6's Int() floors, but all values reaching it here are non-negative. */
double vbRound(double v, double m) {
    return std::floor(v * m + 0.5) / m;
}

/* Parse a text field, returning `fallback` when it isn't a number.
   VB6 silently coerces junk to 0; we keep the previous value instead, which
   avoids destroying data when the user is mid-typing. */
double readNum(wxTextCtrl* c, double fallback) {
    double v = 0;
    if (c == nullptr) return fallback;
    wxString s = c->GetValue();
    if (s.empty()) return fallback;
    if (!s.ToDouble(&v)) return fallback;
    return v;
}

}  // namespace

InfoPanel::InfoPanel(MainFrame* parent, MapDocument& doc, UndoStack* undo)
    : wxFrame(parent, wxID_ANY, "Properties",
              wxDefaultPosition, wxSize(216, 250),
              wxFRAME_FLOAT_ON_PARENT | wxCAPTION | wxCLOSE_BOX |
              wxFRAME_NO_TASKBAR),
      m_mainFrame(parent),
      m_doc(doc),
      m_undo(undo) {
    SetBackgroundColour(wxColour(0x4A, 0x3C, 0x31));
    SetForegroundColour(*wxWHITE);
    buildUI();
}

/* ---- Construction ------------------------------------------------------ */

void InfoPanel::buildUI() {
    auto* outer = new wxBoxSizer(wxVERTICAL);

    /* Header: picPropMenu button + lblIndex + lblCoords (frmInfo.frm:34). */
    {
        auto* row = new wxBoxSizer(wxHORIZONTAL);
        m_btnPage = new wxButton(this, wxID_ANY, "Map",
                                 wxDefaultPosition, wxSize(84, 22));
        row->Add(m_btnPage, 0, wxALIGN_CENTER_VERTICAL);

        m_lblIndex = new wxStaticText(this, wxID_ANY, "");
        m_lblIndex->SetForegroundColour(*wxWHITE);
        row->Add(m_lblIndex, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 6);

        m_lblCoords = new wxStaticText(this, wxID_ANY, "");
        m_lblCoords->SetForegroundColour(*wxWHITE);
        row->Add(m_lblCoords, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, 6);

        outer->Add(row, 0, wxEXPAND | wxALL, 5);
    }

    m_btnPage->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        wxMenu menu;
        for (int i = 0; i < PAGE_COUNT; ++i) {
            menu.AppendRadioItem(ID_PAGE_BASE + i, kPageNames[i]);
            menu.Check(ID_PAGE_BASE + i, i == m_page);
        }
        menu.Bind(wxEVT_MENU, &InfoPanel::onPageMenu, this);
        PopupMenu(&menu);
    });

    for (int i = 0; i < PAGE_COUNT; ++i) {
        m_pages[i] = new wxPanel(this);
        m_pages[i]->SetBackgroundColour(GetBackgroundColour());
        m_pages[i]->SetForegroundColour(*wxWHITE);
        m_pages[i]->Hide();
        outer->Add(m_pages[i], 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
    }

    buildLightPage(m_pages[PAGE_LIGHT]);
    buildMapPage(m_pages[PAGE_MAP]);
    buildSceneryPage(m_pages[PAGE_SCENERY]);
    buildQuadPage(m_pages[PAGE_QUAD]);
    buildTransformPage(m_pages[PAGE_TRANSFORM]);
    buildPolyPage(m_pages[PAGE_POLYGON]);

    SetSizer(outer);
    ShowPage(PAGE_MAP);
    Refresh();
}

/* Small helper shared by the property pages: a right-aligned label plus a
   control in a 2-column flex grid. */
namespace {
wxFlexGridSizer* makeGrid(wxPanel* p) {
    auto* g = new wxFlexGridSizer(2, 4, 6);
    g->AddGrowableCol(1, 1);
    p->SetSizer(g);
    return g;
}
void addLabel(wxPanel* p, wxFlexGridSizer* g, const char* text) {
    auto* l = new wxStaticText(p, wxID_ANY, text);
    l->SetForegroundColour(*wxWHITE);
    g->Add(l, 0, wxALIGN_CENTER_VERTICAL);
}
}  // namespace

void InfoPanel::buildMapPage(wxPanel* p) {
    auto* g = makeGrid(p);
    auto row = [&](wxStaticText*& f, const char* label) {
        addLabel(p, g, label);
        f = new wxStaticText(p, wxID_ANY, "0");
        f->SetForegroundColour(*wxWHITE);
        g->Add(f, 1, wxALIGN_CENTER_VERTICAL);
    };
    /* Order and capacity limits match frmInfo lblCount(0..6) and the
       assignments at frmOpenSoldatMapEditor.frm:2356-2362. */
    row(m_lblPolys,      "Polygons:");
    row(m_lblScenery,    "Scenery:");
    row(m_lblSpawns,     "Spawns:");
    row(m_lblColliders,  "Colliders:");
    row(m_lblWaypoints,  "Waypoints:");
    row(m_lblConnects,   "Connections:");
    row(m_lblDimensions, "Dimensions:");
}

void InfoPanel::buildPolyPage(wxPanel* p) {
    auto* g = makeGrid(p);

    addLabel(p, g, "Type:");
    m_polyType = new wxChoice(p, wxID_ANY);
    for (int i = 0; i < POLY_TYPE_COUNT; ++i) m_polyType->Append(polyTypeName(i));
    g->Add(m_polyType, 1, wxEXPAND);

    auto field = [&](wxTextCtrl*& c, const char* label) {
        addLabel(p, g, label);
        c = new wxTextCtrl(p, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                           wxTE_PROCESS_ENTER);
        g->Add(c, 1, wxEXPAND);
    };
    field(m_bounciness,  "Bounciness %:");
    field(m_textureU,    "Texture X:");
    field(m_textureV,    "Texture Y:");
    field(m_vertexAlpha, "Opacity %:");

    for (wxTextCtrl* c : {m_bounciness, m_textureU, m_textureV, m_vertexAlpha}) {
        c->Bind(wxEVT_TEXT_ENTER, &InfoPanel::onPolyApply, this);
        c->Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent& e) {
            wxCommandEvent dummy;
            onPolyApply(dummy);
            e.Skip();
        });
    }
    m_polyType->Bind(wxEVT_CHOICE, &InfoPanel::onPolyApply, this);
}

void InfoPanel::buildSceneryPage(wxPanel* p) {
    auto* g = makeGrid(p);
    auto field = [&](wxTextCtrl*& c, const char* label) {
        addLabel(p, g, label);
        c = new wxTextCtrl(p, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                           wxTE_PROCESS_ENTER);
        g->Add(c, 1, wxEXPAND);
    };
    field(m_scenScaleX, "Scaling X %:");
    field(m_scenScaleY, "Scaling Y %:");
    field(m_scenAlpha,  "Opacity %:");
    field(m_scenRot,    "Rotation:");

    addLabel(p, g, "Level:");
    m_scenLevel = new wxChoice(p, wxID_ANY);
    for (const char* n : kLevelNames) m_scenLevel->Append(n);
    g->Add(m_scenLevel, 1, wxEXPAND);

    for (wxTextCtrl* c : {m_scenScaleX, m_scenScaleY, m_scenAlpha, m_scenRot}) {
        c->Bind(wxEVT_TEXT_ENTER, &InfoPanel::onScenApply, this);
        c->Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent& e) {
            wxCommandEvent dummy;
            onScenApply(dummy);
            e.Skip();
        });
    }
    m_scenLevel->Bind(wxEVT_CHOICE, &InfoPanel::onScenApply, this);
}

void InfoPanel::buildLightPage(wxPanel* p) {
    auto* g = makeGrid(p);
    auto field = [&](wxTextCtrl*& c, const char* label) {
        addLabel(p, g, label);
        c = new wxTextCtrl(p, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                           wxTE_PROCESS_ENTER);
        g->Add(c, 1, wxEXPAND);
    };
    field(m_lightZ,     "Z-coord:");
    field(m_lightRange, "Range:");
    field(m_lightInten, "Intensity %:");

    addLabel(p, g, "Color:");
    m_lightColor = new wxPanel(p, wxID_ANY, wxDefaultPosition, wxSize(40, 20));
    m_lightColor->SetBackgroundColour(*wxWHITE);
    g->Add(m_lightColor, 0, wxALIGN_CENTER_VERTICAL);

    /* frmInfo picLight_Click opens the colour dialog (frm:4488). */
    m_lightColor->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent&) {
        wxColourData cd;
        cd.SetColour(m_lightColor->GetBackgroundColour());
        wxColourDialog dlg(this, &cd);
        if (dlg.ShowModal() != wxID_OK) return;
        wxColour c = dlg.GetColourData().GetColour();
        m_lightColor->SetBackgroundColour(c);
        m_lightColor->Refresh();
        pushUndo();
        for (auto& l : m_doc.lights) {
            if (!l.selected) continue;
            l.r = c.Red();
            l.g = c.Green();
            l.b = c.Blue();
        }
        if (m_mainFrame) m_mainFrame->RefreshViewport();
    });

    for (wxTextCtrl* c : {m_lightZ, m_lightRange, m_lightInten}) {
        c->Bind(wxEVT_TEXT_ENTER, &InfoPanel::onLightApply, this);
        c->Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent& e) {
            wxCommandEvent dummy;
            onLightApply(dummy);
            e.Skip();
        });
    }
}

void InfoPanel::buildQuadPage(wxPanel* p) {
    auto* g = makeGrid(p);
    auto field = [&](wxTextCtrl*& c, const char* label) {
        addLabel(p, g, label);
        c = new wxTextCtrl(p, wxID_ANY, "0", wxDefaultPosition, wxDefaultSize,
                           wxTE_READONLY);
        g->Add(c, 1, wxEXPAND);
    };
    /* frmInfo txtQuadX(0..1) / txtQuadY(0..1) show the texture quad in
       texel units (frmOpenSoldatMapEditor.frm:4294-4298).  Read-only here:
       the original only ever writes them from the loaded texture size. */
    field(m_quadX0, "Left:");
    field(m_quadY0, "Top:");
    field(m_quadX1, "Right:");
    field(m_quadY1, "Bottom:");
}

void InfoPanel::buildTransformPage(wxPanel* p) {
    auto* g = makeGrid(p);
    auto field = [&](wxTextCtrl*& c, const char* label, const char* init) {
        addLabel(p, g, label);
        c = new wxTextCtrl(p, wxID_ANY, init, wxDefaultPosition, wxDefaultSize,
                           wxTE_PROCESS_ENTER);
        g->Add(c, 1, wxEXPAND);
    };
    field(m_trRotate, "Rotation:", "0");
    field(m_trScaleX, "Scaling X %:", "100");
    field(m_trScaleY, "Scaling Y %:", "100");

    /* frmInfo txtRotate/txtScale apply a one-shot transform about the
       selection centre, the same transform the Rotate/Scale tools drive
       interactively (frmOpenSoldatMapEditor.frm ApplyTransform:7245). */
    auto apply = [this](wxCommandEvent&) {
        if (m_noChange) return;
        double deg = readNum(m_trRotate, 0.0);
        double sx  = readNum(m_trScaleX, 100.0) / 100.0;
        double sy  = readNum(m_trScaleY, 100.0) / 100.0;
        if (deg == 0.0 && sx == 1.0 && sy == 1.0) return;

        MapDocument::TransformSession s;
        m_doc.beginTransform(s);
        if (s.empty()) return;
        pushUndo();
        m_doc.applyTransform(s, static_cast<float>(sx), static_cast<float>(sy),
                             static_cast<float>(deg * 3.14159265358979 / 180.0));
        if (m_mainFrame) m_mainFrame->RefreshViewport();
    };
    for (wxTextCtrl* c : {m_trRotate, m_trScaleX, m_trScaleY})
        c->Bind(wxEVT_TEXT_ENTER, apply);
}

/* ---- Page switching ---------------------------------------------------- */

void InfoPanel::ShowPage(int page) {
    if (page < 0 || page >= PAGE_COUNT) return;
    for (int i = 0; i < PAGE_COUNT; ++i)
        if (m_pages[i]) m_pages[i]->Show(i == page);
    m_page = page;
    if (m_btnPage) m_btnPage->SetLabel(kPageNames[page]);
    Layout();
}

void InfoPanel::onPageMenu(wxCommandEvent& e) {
    ShowPage(e.GetId() - ID_PAGE_BASE);
    Refresh();
}

/* VB6 GetInfo: with nothing selected, show the Light page if lights are
   selected, otherwise the Map page.  With polygons selected show Polygon,
   with only scenery selected show Scenery. */
void InfoPanel::autoSelectPage() {
    int selPolys = 0, selScen = 0, selLights = 0;
    for (const auto& p : m_doc.polys)   if (p.anySelected()) ++selPolys;
    for (const auto& s : m_doc.scenery) if (s.selected)      ++selScen;
    for (const auto& l : m_doc.lights)  if (l.selected)      ++selLights;

    /* Never fight the user: only auto-switch away from a page whose subject
       is no longer selected.  The original does the same by only calling
       mnuProp_Click from GetInfo. */
    if (selPolys == 0 && selScen == 0) {
        if (selLights > 0) {
            if (m_page != PAGE_LIGHT) ShowPage(PAGE_LIGHT);
        } else if (m_page == PAGE_POLYGON || m_page == PAGE_SCENERY ||
                   m_page == PAGE_LIGHT) {
            ShowPage(PAGE_MAP);
        }
        return;
    }
    if (selPolys > 0) {
        if (m_page == PAGE_MAP || m_page == PAGE_LIGHT || m_page == PAGE_SCENERY)
            ShowPage(PAGE_POLYGON);
    } else if (m_page == PAGE_MAP || m_page == PAGE_LIGHT ||
               m_page == PAGE_POLYGON) {
        ShowPage(PAGE_SCENERY);
    }
}

/* ---- Refresh ----------------------------------------------------------- */

void InfoPanel::Refresh() {
    if (m_lblPolys == nullptr) return;

    m_noChange = true;
    autoSelectPage();

    /* --- Map page counts (frmOpenSoldatMapEditor.frm:2356-2362) --- */
    int sceneryElements = 0;
    {
        std::vector<int> seen;
        for (const auto& s : m_doc.scenery) {
            if (std::find(seen.begin(), seen.end(), s.style) == seen.end())
                seen.push_back(s.style);
        }
        sceneryElements = static_cast<int>(seen.size());
    }
    int connections = 0;
    for (const auto& w : m_doc.waypoints)
        connections += static_cast<int>(w.connections.size());

    m_lblPolys->SetLabel(wxString::Format("%d", (int)m_doc.polys.size()));
    m_lblScenery->SetLabel(wxString::Format("%d/500 (%d)",
                                            (int)m_doc.scenery.size(),
                                            sceneryElements));
    m_lblSpawns->SetLabel(wxString::Format("%d/128", (int)m_doc.spawns.size()));
    m_lblColliders->SetLabel(wxString::Format("%d/128",
                                              (int)m_doc.colliders.size()));
    m_lblWaypoints->SetLabel(wxString::Format("%d/500",
                                              (int)m_doc.waypoints.size()));
    m_lblConnects->SetLabel(wxString::Format("%d", connections));

    /* GetMapDimensions: bounding box of all polygon vertices. */
    {
        float minX = 0, minY = 0, maxX = 0, maxY = 0;
        bool any = false;
        for (const auto& p : m_doc.polys) {
            for (const auto& v : p.v) {
                if (!any) { minX = maxX = v.world.x; minY = maxY = v.world.y; any = true; }
                minX = std::min(minX, v.world.x);
                maxX = std::max(maxX, v.world.x);
                minY = std::min(minY, v.world.y);
                maxY = std::max(maxY, v.world.y);
            }
        }
        m_lblDimensions->SetLabel(
            wxString::Format("%dx%d", (int)(maxX - minX), (int)(maxY - minY)));
    }

    /* --- Selected polygon (frm:4715-4730) --- */
    const EditorPoly* selPoly = nullptr;
    int selPolyIdx = -1, selPolyVtx = -1, selPolyCount = 0;
    for (size_t i = 0; i < m_doc.polys.size(); ++i) {
        if (!m_doc.polys[i].anySelected()) continue;
        ++selPolyCount;
        if (selPoly != nullptr) continue;
        selPoly = &m_doc.polys[i];
        selPolyIdx = static_cast<int>(i);
        for (int j = 0; j < 3; ++j)
            if (selPoly->v[j].selected) { selPolyVtx = j; break; }
    }

    if (selPoly != nullptr && selPolyVtx >= 0) {
        const EditorVertex& v = selPoly->v[selPolyVtx];
        m_polyType->SetSelection(std::min<int>(selPoly->polyType,
                                               POLY_TYPE_COUNT - 1));

        /* Int((Perp.Z - 1) * 100), floored at 0. */
        int bounce = (int)((selPoly->bounciness[selPolyVtx] - 1.0f) * 100.0f);
        if (bounce < 0) bounce = 0;
        m_bounciness->SetValue(wxString::Format("%d", bounce));
        /* Only editable for POLY_BOUNCY, exactly as the original. */
        m_bounciness->Enable(selPoly->polyType == POLY_BOUNCY);

        m_textureU->SetValue(wxString::Format("%g", vbRound(v.tu, 10000.0)));
        m_textureV->SetValue(wxString::Format("%g", vbRound(v.tv, 10000.0)));
        m_vertexAlpha->SetValue(
            wxString::Format("%g", vbRound(v.alpha / 255.0 * 100.0, 100.0)));
        m_lblCoords->SetLabel(
            wxString::Format("%g, %g", vbRound(v.world.x, 100.0),
                                       vbRound(v.world.y, 100.0)));
    } else if (selPoly == nullptr) {
        m_bounciness->SetValue("");
        m_textureU->SetValue("");
        m_textureV->SetValue("");
        m_vertexAlpha->SetValue("");
    }

    /* --- Selected scenery (frm:4737-4746) --- */
    const EditorScenery* selScen = nullptr;
    int selScenIdx = -1, selScenCount = 0;
    for (size_t i = 0; i < m_doc.scenery.size(); ++i) {
        if (!m_doc.scenery[i].selected) continue;
        ++selScenCount;
        if (selScen != nullptr) continue;
        selScen = &m_doc.scenery[i];
        selScenIdx = static_cast<int>(i);
    }
    if (selScen != nullptr) {
        m_scenScaleX->SetValue(
            wxString::Format("%g", vbRound(selScen->scaleX * 100.0, 100.0)));
        m_scenScaleY->SetValue(
            wxString::Format("%g", vbRound(selScen->scaleY * 100.0, 100.0)));
        m_scenAlpha->SetValue(
            wxString::Format("%g", vbRound(selScen->alpha / 255.0 * 100.0, 10.0)));
        m_scenRot->SetValue(
            wxString::Format("%g", vbRound(selScen->rotation * 180.0 /
                                           3.14159265358979, 10.0)));
        m_scenLevel->SetSelection(std::min(std::max(selScen->level, 0), 2));
        if (selPoly == nullptr) {
            m_lblCoords->SetLabel(
                wxString::Format("%g, %g", vbRound(selScen->x, 100.0),
                                           vbRound(selScen->y, 100.0)));
        }
    } else {
        m_scenScaleX->SetValue("");
        m_scenScaleY->SetValue("");
        m_scenAlpha->SetValue("");
        m_scenRot->SetValue("");
    }

    /* --- Selected light (frm:4695-4697) --- */
    const EditorLight* selLight = nullptr;
    for (const auto& l : m_doc.lights)
        if (l.selected) { selLight = &l; break; }
    if (selLight != nullptr) {
        m_lightZ->SetValue(wxString::Format("%g", (double)selLight->z));
        m_lightRange->SetValue(wxString::Format("%d", selLight->range));
        m_lightInten->SetValue(
            wxString::Format("%g", vbRound(selLight->intensity * 100.0, 10.0)));
        m_lightColor->SetBackgroundColour(
            wxColour(selLight->r, selLight->g, selLight->b));
        m_lightColor->Refresh();
    }

    /* --- lblIndex (frm:4752-4759) --- */
    if (selPolyCount == 1 && selScenCount == 0)
        m_lblIndex->SetLabel(wxString::Format("#%d", selPolyIdx));
    else if (selPolyCount == 0 && selScenCount == 1)
        m_lblIndex->SetLabel(wxString::Format("#%d", selScenIdx));
    else
        m_lblIndex->SetLabel("");

    if (selPoly == nullptr && selScen == nullptr) m_lblCoords->SetLabel("");

    m_noChange = false;
}

/* ---- Commit ------------------------------------------------------------ */

void InfoPanel::pushUndo() {
    if (m_undo != nullptr) m_undo->push(m_doc);
}

void InfoPanel::onPolyApply(wxCommandEvent&) {
    if (m_noChange) return;

    /* Find the reference vertex the values were read from, so a no-op commit
       (focus loss without editing) doesn't dirty the document. */
    EditorPoly* ref = nullptr;
    int refVtx = -1;
    for (auto& p : m_doc.polys) {
        if (!p.anySelected()) continue;
        ref = &p;
        for (int j = 0; j < 3; ++j) if (p.v[j].selected) { refVtx = j; break; }
        break;
    }
    if (ref == nullptr || refVtx < 0) return;

    int   newType  = m_polyType->GetSelection();
    double bounce  = readNum(m_bounciness,
                             (ref->bounciness[refVtx] - 1.0f) * 100.0f);
    double tu      = readNum(m_textureU, ref->v[refVtx].tu);
    double tv      = readNum(m_textureV, ref->v[refVtx].tv);
    double alphaPc = readNum(m_vertexAlpha,
                             ref->v[refVtx].alpha / 255.0 * 100.0);

    if (bounce < 0) bounce = 0;
    if (alphaPc < 0) alphaPc = 0;
    if (alphaPc > 100) alphaPc = 100;

    uint8_t newAlpha = static_cast<uint8_t>(alphaPc / 100.0 * 255.0 + 0.5);
    float   newBounce = static_cast<float>(1.0 + bounce / 100.0);

    bool changed =
        (newType >= 0 && newType != ref->polyType) ||
        std::fabs(ref->v[refVtx].tu - tu) > 1e-6 ||
        std::fabs(ref->v[refVtx].tv - tv) > 1e-6 ||
        ref->v[refVtx].alpha != newAlpha ||
        std::fabs(ref->bounciness[refVtx] - newBounce) > 1e-6;
    if (!changed) return;

    pushUndo();

    for (auto& p : m_doc.polys) {
        if (!p.anySelected()) continue;
        if (newType >= 0) p.polyType = static_cast<uint8_t>(newType);
        for (int j = 0; j < 3; ++j) {
            if (!p.v[j].selected) continue;
            p.v[j].tu    = static_cast<float>(tu);
            p.v[j].tv    = static_cast<float>(tv);
            p.v[j].alpha = newAlpha;
            /* VB6 ApplyBounciness (frm:4880) writes every edge of the poly. */
            if (p.polyType == POLY_BOUNCY) p.bounciness[j] = newBounce;
        }
    }
    m_doc.modified = true;
    if (m_mainFrame) m_mainFrame->RefreshViewport();
}

void InfoPanel::onScenApply(wxCommandEvent&) {
    if (m_noChange) return;

    EditorScenery* ref = nullptr;
    for (auto& s : m_doc.scenery) if (s.selected) { ref = &s; break; }
    if (ref == nullptr) return;

    double sx    = readNum(m_scenScaleX, ref->scaleX * 100.0) / 100.0;
    double sy    = readNum(m_scenScaleY, ref->scaleY * 100.0) / 100.0;
    double alpha = readNum(m_scenAlpha, ref->alpha / 255.0 * 100.0);
    double rotDg = readNum(m_scenRot,
                           ref->rotation * 180.0 / 3.14159265358979);
    int    level = m_scenLevel->GetSelection();

    if (alpha < 0) alpha = 0;
    if (alpha > 100) alpha = 100;
    uint8_t newAlpha = static_cast<uint8_t>(alpha / 100.0 * 255.0 + 0.5);
    float   newRot   = static_cast<float>(rotDg * 3.14159265358979 / 180.0);

    bool changed =
        std::fabs(ref->scaleX - sx) > 1e-6 ||
        std::fabs(ref->scaleY - sy) > 1e-6 ||
        ref->alpha != newAlpha ||
        std::fabs(ref->rotation - newRot) > 1e-6 ||
        (level >= 0 && level != ref->level);
    if (!changed) return;

    pushUndo();
    for (auto& s : m_doc.scenery) {
        if (!s.selected) continue;
        s.scaleX   = static_cast<float>(sx);
        s.scaleY   = static_cast<float>(sy);
        s.alpha    = newAlpha;
        s.rotation = newRot;
        if (level >= 0) s.level = level;
    }
    m_doc.modified = true;
    if (m_mainFrame) m_mainFrame->RefreshViewport();
}

void InfoPanel::onLightApply(wxCommandEvent&) {
    if (m_noChange) return;

    EditorLight* ref = nullptr;
    for (auto& l : m_doc.lights) if (l.selected) { ref = &l; break; }
    if (ref == nullptr) return;

    double z     = readNum(m_lightZ, ref->z);
    double range = readNum(m_lightRange, ref->range);
    double inten = readNum(m_lightInten, ref->intensity * 100.0) / 100.0;

    bool changed = std::fabs(ref->z - z) > 1e-6 ||
                   ref->range != (int)range ||
                   std::fabs(ref->intensity - inten) > 1e-6;
    if (!changed) return;

    pushUndo();
    for (auto& l : m_doc.lights) {
        if (!l.selected) continue;
        l.z         = static_cast<float>(z);
        l.range     = static_cast<int>(range);
        l.intensity = static_cast<float>(inten);
    }
    m_doc.modified = true;
    if (m_mainFrame) m_mainFrame->RefreshViewport();
}
