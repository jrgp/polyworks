#include "gl_viewport.h"

#include "mainframe.h"
#include "geometry.h"
#include "undo_stack.h"

#include <wx/dcbuffer.h>
#include <wx/dcclient.h>
#include <wx/dcmemory.h>
#include <wx/settings.h>
#include <wx/filename.h>
#include <wx/image.h>
#include <wx/menu.h>

#include <cmath>

/* Tool constants (must match mainframe.h / kToolInfo ordering) */
constexpr int TOOL_MOVE      = 0;
constexpr int TOOL_CREATE    = 1;
constexpr int TOOL_VSELECT   = 2;
constexpr int TOOL_PSELECT   = 3;
constexpr int TOOL_VCOLOR    = 4;
constexpr int TOOL_PCOLOR    = 5;
constexpr int TOOL_TEXTURE   = 6;
constexpr int TOOL_SCENERY   = 7;
constexpr int TOOL_WAYPOINT  = 8;
constexpr int TOOL_OBJECTS   = 9;
constexpr int TOOL_COLORPICK = 10;
constexpr int TOOL_SKETCH    = 11;
constexpr int TOOL_LIGHTS    = 12;
constexpr int TOOL_DEPTHMAP  = 13;

/* Virtual tool constants — active tool as modified by held keys */
constexpr int TOOL_HAND      = 14;  /* Space: temporary pan */
constexpr int TOOL_VSELADD   = 15;  /* Shift + VSELECT: add to vertex selection */
constexpr int TOOL_VSELSUB   = 16;  /* Alt  + VSELECT: subtract from vertex selection */
constexpr int TOOL_PSELADD   = 17;  /* Shift + PSELECT: add to poly selection */
constexpr int TOOL_PSELSUB   = 18;  /* Alt  + PSELECT: subtract from poly selection */
constexpr int TOOL_SCALE     = 19;  /* Ctrl + MOVE: scale selection */
constexpr int TOOL_ROTATE    = 20;  /* Alt  + MOVE: rotate selection */
constexpr int TOOL_CONNECT   = 21;  /* Shift + WAYPOINT: connect waypoints */
constexpr int TOOL_QUAD      = 22;  /* Textured-quad creation */
constexpr int TOOL_PIXPICKER = 23;  /* Ctrl + COLORPICK: pick a screen pixel */
constexpr int TOOL_LITPICKER = 24;  /* Alt  + COLORPICK: pick the lit colour */
constexpr int TOOL_ERASER    = 25;  /* Ctrl + SKETCH: erase sketch lines */
constexpr int TOOL_SMUDGE    = 26;  /* Alt  + SKETCH: smudge sketch lines */

constexpr float kDragThreshold = 4.0f;

#if PW_HAS_WX_GLCANVAS && PW_HAS_OPENGL_HEADERS
namespace {
wxGLAttributes BuildGlAttributes() {
    wxGLAttributes glAttrs;
    glAttrs.PlatformDefaults().Defaults().EndList();
    return glAttrs;
}
}  // namespace
#endif

GlViewport::GlViewport(MainFrame* parent, MapDocument& document, UndoStack& undoStack)
#if PW_HAS_WX_GLCANVAS && PW_HAS_OPENGL_HEADERS
    : GlViewportBase(parent, BuildGlAttributes(), wxID_ANY, wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE | wxWANTS_CHARS),
#else
    : GlViewportBase(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE | wxWANTS_CHARS),
#endif
      m_mainFrame(parent),
      m_document(document),
      m_undoStack(undoStack) {
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetBackgroundColour(wxColour(0x2B, 0x1A, 0x0E));
    SetFocus();

#if PW_HAS_WX_GLCANVAS && PW_HAS_OPENGL_HEADERS
    m_renderer.setTextureManager(&m_texMgr);
    if (IsDisplaySupported(BuildGlAttributes())) {
        m_glContext = new wxGLContext(this);
    }
#endif

    Bind(wxEVT_PAINT,       &GlViewport::OnPaint,      this);
    Bind(wxEVT_MOTION,      &GlViewport::OnMouseMove,  this);
    Bind(wxEVT_MOUSEWHEEL,  &GlViewport::OnMouseWheel, this);
    Bind(wxEVT_LEFT_DOWN,   &GlViewport::OnLeftDown,   this);
    Bind(wxEVT_LEFT_UP,     &GlViewport::OnLeftUp,     this);
    Bind(wxEVT_MIDDLE_DOWN, &GlViewport::OnMiddleDown, this);
    Bind(wxEVT_MIDDLE_UP,   &GlViewport::OnMiddleUp,   this);
    Bind(wxEVT_RIGHT_DOWN,  &GlViewport::OnRightDown,  this);
    Bind(wxEVT_KEY_DOWN,    &GlViewport::OnKeyDown,    this);
    Bind(wxEVT_KEY_UP,      &GlViewport::OnKeyUp,      this);
}

GlViewport::~GlViewport() {
#if PW_HAS_WX_GLCANVAS && PW_HAS_OPENGL_HEADERS
    if (m_glContext != nullptr) {
        m_glContext->SetCurrent(*this);
        m_texMgr.clear();
    }
    delete m_glContext;
    m_glContext = nullptr;
#endif
}

void GlViewport::setSkinsPath(const std::string& path) {
#if PW_HAS_WX_GLCANVAS && PW_HAS_OPENGL_HEADERS
    m_texMgr.setBasePath(path);
#else
    (void)path;
#endif
    loadCursors(path);
}

/* Maps tool index to cursor filename (no extension).  Index matches the VB6
   TOOL_* constants (frm:1313-1341); the ImageList in frmOpenSoldatMapEditor
   loads exactly these 27 files (frm:1626-1653). */
static const char* kToolCursorNames[27] = {
    "move",        /* TOOL_MOVE      = 0 */
    "create",      /* TOOL_CREATE    = 1 */
    "vselect",     /* TOOL_VSELECT   = 2 */
    "pselect",     /* TOOL_PSELECT   = 3 */
    "vcolor",      /* TOOL_VCOLOR    = 4 */
    "pcolor",      /* TOOL_PCOLOR    = 5 */
    "texture",     /* TOOL_TEXTURE   = 6 */
    "scenery",     /* TOOL_SCENERY   = 7 */
    "waypoint",    /* TOOL_WAYPOINT  = 8 */
    "objects",     /* TOOL_OBJECTS   = 9 */
    "colorpicker", /* TOOL_COLORPICK = 10 */
    "sketch",      /* TOOL_SKETCH    = 11 */
    "light",       /* TOOL_LIGHTS    = 12 */
    "depthmap",    /* TOOL_DEPTHMAP  = 13 */
    "hand",        /* TOOL_HAND      = 14 */
    "vseladd",     /* TOOL_VSELADD   = 15 */
    "vselsub",     /* TOOL_VSELSUB   = 16 */
    "pseladd",     /* TOOL_PSELADD   = 17 */
    "pselsub",     /* TOOL_PSELSUB   = 18 */
    "scale",       /* TOOL_SCALE     = 19 */
    "rotate",      /* TOOL_ROTATE    = 20 */
    "connect",     /* TOOL_CONNECT   = 21 */
    "quad",        /* TOOL_QUAD      = 22 */
    "pixpicker",   /* TOOL_PIXPICKER = 23 */
    "litpicker",   /* TOOL_LITPICKER = 24 */
    "eraser",      /* TOOL_ERASER    = 25 */
    "smudge",      /* TOOL_SMUDGE    = 26 */
};

void GlViewport::loadCursors(const std::string& skinsPath) {
    if (skinsPath.empty()) return;
    const wxString cursorDir = wxString::FromUTF8(skinsPath) + wxFILE_SEP_PATH + "cursors";
    for (int i = 0; i < kNumTools; ++i) {
        wxString curPath = cursorDir + wxFILE_SEP_PATH
                           + wxString::FromUTF8(kToolCursorNames[i]) + ".cur";
        if (!wxFileExists(curPath)) continue;
        wxImage img(curPath, wxBITMAP_TYPE_CUR);
        if (img.IsOk())
            m_toolCursors[i] = wxCursor(img);
    }
    applyToolCursor();
}

void GlViewport::applyToolCursor() {
    /* VB6 SetCursor is always called with currentFunction (the
       modifier-adjusted tool), not the tool selected in the palette
       (frm:5037, frmTools:496), so Shift over the vertex-select tool really
       does show the "add to selection" cursor. */
    int idx = m_spaceDown ? TOOL_HAND : m_currentFunction;
    if (idx >= 0 && idx < kNumTools) {
        const wxCursor& c = m_toolCursors[idx];
        if (c.IsOk()) {
            SetCursor(c);
            return;
        }
    }
    /* No skin available: fall back to a stock cursor that conveys the same
       intent rather than leaving the platform default everywhere. */
    switch (idx) {
    case TOOL_HAND:     SetCursor(wxCursor(wxCURSOR_HAND));        return;
    case TOOL_CREATE:
    case TOOL_QUAD:     SetCursor(wxCursor(wxCURSOR_CROSS));       return;
    case TOOL_COLORPICK:
    case TOOL_PIXPICKER:
    case TOOL_LITPICKER:SetCursor(wxCursor(wxCURSOR_CROSS));       return;
    case TOOL_SCALE:    SetCursor(wxCursor(wxCURSOR_SIZENWSE));    return;
    case TOOL_MOVE:     SetCursor(wxCursor(wxCURSOR_SIZING));      return;
    default:            SetCursor(wxNullCursor);                   return;
    }
}

void GlViewport::addTexturePath(const std::string& path) {
#if PW_HAS_WX_GLCANVAS && PW_HAS_OPENGL_HEADERS
    m_texMgr.addSearchPath(path);
#else
    (void)path;
#endif
}

std::pair<int,int> GlViewport::getTextureSize(const MapDocument& doc) {
#if PW_HAS_WX_GLCANVAS && PW_HAS_OPENGL_HEADERS
    if (!doc.options.textureName.empty()) {
        const GLuint id = m_texMgr.loadTexture(doc.options.textureName);
        if (id != 0) {
            int w = 0, h = 0;
            m_texMgr.getSize(id, w, h);
            if (w > 0 && h > 0) return {w, h};
        }
    }
#else
    (void)doc;
#endif
    return {0, 0};
}

void GlViewport::setActiveTool(int tool) {
    if (m_activeTool == tool) return;
    if (m_state == ViewportState::CreatingPoly)
        CancelCreation();
    m_state = ViewportState::Idle;
    m_activeTool = tool;
    m_currentFunction = tool;
    applyToolCursor();
}

void GlViewport::setPaintColor(uint8_t r, uint8_t g, uint8_t b,
                                float opacity, int blendMode, float radius) {
    m_paintR = r; m_paintG = g; m_paintB = b;
    m_paintOpacity   = opacity;
    m_paintBlendMode = blendMode;
    m_paintRadius    = radius;
}

int GlViewport::ComputeCurrentFunction(bool shiftDown, bool ctrlDown, bool altDown) const {
    /*
     * Replicates the VB6 DirectInput key poller (frm:10715-10793).  The
     * original evaluates Space, then Shift, then Ctrl, then Alt and returns
     * on the first one that is down, so that precedence is reproduced here.
     * Space-pan is handled separately as it never reaches this path.
     */
    if (shiftDown) {
        switch (m_activeTool) {
        case TOOL_VSELECT:   return TOOL_VSELADD;
        case TOOL_PSELECT:   return TOOL_PSELADD;
        case TOOL_WAYPOINT:  return TOOL_CONNECT;
        case TOOL_COLORPICK: return TOOL_PIXPICKER;
        /* Shift + SKETCH stays on SKETCH but switches it to straight-line
           mode by anchoring sketch(0); handled in the sketch tool itself. */
        default: return m_activeTool;
        }
    }

    if (ctrlDown) {
        switch (m_activeTool) {
        case TOOL_MOVE:   return TOOL_SCALE;
        case TOOL_SKETCH: return TOOL_SMUDGE;
        default:
            if (m_activeTool > TOOL_MOVE) return TOOL_MOVE;
            return m_activeTool;
        }
    }

    if (altDown) {
        switch (m_activeTool) {
        case TOOL_MOVE:      return TOOL_ROTATE;
        case TOOL_VSELECT:   return TOOL_VSELSUB;
        case TOOL_PSELECT:   return TOOL_PSELSUB;
        case TOOL_VCOLOR:    return TOOL_COLORPICK;
        case TOOL_PCOLOR:    return TOOL_COLORPICK;
        case TOOL_DEPTHMAP:  return TOOL_COLORPICK;
        case TOOL_COLORPICK: return TOOL_LITPICKER;
        case TOOL_SKETCH:    return TOOL_ERASER;
        default:             return TOOL_VSELECT;
        }
    }

    return m_activeTool;
}

/* ---- Paint -------------------------------------------------------------- */

void GlViewport::OnPaint(wxPaintEvent& /*event*/) {
#if PW_HAS_WX_GLCANVAS && PW_HAS_OPENGL_HEADERS
    wxPaintDC dc(this);
    if (m_glContext == nullptr) {
        DrawFallback(dc);
        return;
    }

    m_glContext->SetCurrent(*this);
    const wxSize size = GetClientSize();
    glViewport(0, 0, size.x, size.y);
    glClearColor(43.0f/255.0f, 26.0f/255.0f, 14.0f/255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, size.x, size.y, 0.0, -1.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    if (!m_initialized) {
        m_renderer.initialize();
        m_initialized = true;
    }

    m_renderer.setMoveToolActive(m_activeTool == TOOL_MOVE);
    m_renderer.renderAll(m_document, size.x, size.y, m_document.viewSettings);

    /* Draw creation vertices */
    if (m_state == ViewportState::CreatingPoly && m_creationVertCount > 0) {
        glDisable(GL_TEXTURE_2D);
        glColor4ub(0, 220, 255, 255);
        glPointSize(6.0f);
        glBegin(GL_POINTS);
        for (int i = 0; i < m_creationVertCount; ++i) {
            Vec2 s = m_document.worldToScreen(m_creationVerts[i]);
            glVertex2f(s.x, s.y);
        }
        glEnd();
        glPointSize(1.0f);
        if (m_creationVertCount > 1) {
            glBegin(GL_LINE_STRIP);
            for (int i = 0; i < m_creationVertCount; ++i) {
                Vec2 s = m_document.worldToScreen(m_creationVerts[i]);
                glVertex2f(s.x, s.y);
            }
            glEnd();
        }
    }

    /* Draw rubber-band selection rectangle */
    if (m_state == ViewportState::RubberBanding && m_didDrag) {
        Vec2 a = m_document.worldToScreen(m_rubberA);
        Vec2 b = m_document.worldToScreen(m_rubberB);
        glDisable(GL_TEXTURE_2D);
        glEnable(GL_LINE_STIPPLE);
        glLineStipple(1, 0xF0F0);
        glColor4ub(255, 255, 0, 255);
        glBegin(GL_LINE_LOOP);
        glVertex2f(a.x, a.y);
        glVertex2f(b.x, a.y);
        glVertex2f(b.x, b.y);
        glVertex2f(a.x, b.y);
        glEnd();
        glDisable(GL_LINE_STIPPLE);
    }

    SwapBuffers();
#else
    wxAutoBufferedPaintDC dc(this);
    DrawFallback(dc);
#endif
}

/* ---- Mouse events ------------------------------------------------------- */

void GlViewport::OnMouseMove(wxMouseEvent& event) {
    const Vec2 world = m_document.screenToWorld(
        {static_cast<float>(event.GetX()), static_cast<float>(event.GetY())});
    if (m_mainFrame != nullptr)
        m_mainFrame->UpdateMouseWorldPosition(world);

    if (m_panning && (event.MiddleIsDown() || IsSpacePanGesture(event))) {
        UpdatePan(event.GetPosition());
        return;
    }
    if (m_panning && !event.MiddleIsDown() && !event.LeftIsDown())
        EndPan();

    if (event.LeftIsDown())
        HandleMouseMoveEdit(event);

    event.Skip();
}

void GlViewport::OnMouseWheel(wxMouseEvent& event) {
    const int dir = event.GetWheelRotation() > 0 ? 1 : -1;
    const float newZoom = snapZoom(m_document.zoom, dir);
    m_document.setZoom(newZoom, static_cast<float>(event.GetX()), static_cast<float>(event.GetY()));
    if (m_mainFrame != nullptr)
        m_mainFrame->UpdateStatusBar();
    Refresh(false);
}

void GlViewport::OnLeftDown(wxMouseEvent& event) {
    SetFocus();
    if (IsSpacePanGesture(event)) {
        BeginPan(event.GetPosition());
        return;
    }
    HandleLeftDownEdit(event);
    event.Skip();
}

void GlViewport::OnLeftUp(wxMouseEvent& event) {
    if (m_panning && !event.MiddleIsDown()) {
        EndPan();
        return;
    }
    HandleLeftUpEdit(event);
    event.Skip();
}

void GlViewport::OnMiddleDown(wxMouseEvent& event) {
    SetFocus();
    BeginPan(event.GetPosition());
}

void GlViewport::OnMiddleUp(wxMouseEvent& event) {
    EndPan();
    event.Skip();
}

void GlViewport::OnRightDown(wxMouseEvent& event) {
    const Vec2 world = m_document.screenToWorld(
        {static_cast<float>(event.GetX()), static_cast<float>(event.GetY())});

    auto refresh = [this]() {
        if (m_mainFrame != nullptr) {
            m_mainFrame->UpdateStatusBar();
            m_mainFrame->UpdateTitle();
        }
        Refresh(false);
    };

    if (m_currentFunction == TOOL_CREATE || m_currentFunction == TOOL_QUAD) {
        /* VB6 frm:11115 — right-click during creation offers the polygon type
           for the *next* polygon; it does not cancel the in-progress one
           (Escape does that). */
        wxMenu menu;
        const int cur = (m_mainFrame != nullptr) ? m_mainFrame->GetCreationPolyType() : 0;
        for (int i = 0; i < POLY_TYPE_COUNT; ++i) {
            wxMenuItem* item = menu.AppendRadioItem(wxID_ANY, polyTypeName(i));
            if (i == cur) item->Check(true);
            const int type = i;
            menu.Bind(wxEVT_MENU, [this, type](wxCommandEvent&) {
                if (m_mainFrame != nullptr) m_mainFrame->SetCreationPolyType(type);
            }, item->GetId());
        }
        menu.AppendSeparator();
        wxMenuItem* quadItem = menu.AppendCheckItem(wxID_ANY, "Textured Quad");
        quadItem->Check(m_activeTool == TOOL_QUAD);
        menu.Bind(wxEVT_MENU, [this, quadItem](wxCommandEvent&) {
            setActiveTool(quadItem->IsChecked() ? TOOL_QUAD : TOOL_CREATE);
            if (m_mainFrame != nullptr) m_mainFrame->UpdateStatusBar();
        }, quadItem->GetId());
        PopupMenu(&menu);
        return;
    }

    if (m_currentFunction == TOOL_MOVE || m_currentFunction == TOOL_SCALE ||
        m_currentFunction == TOOL_ROTATE) {
        /* VB6 mnuMove (frm:619-631): choose where scale/rotate pivot from. */
        wxMenu menu;
        using RC = MapDocument::RCenterMode;
        struct { const char* label; RC mode; } kItems[] = {
            {"Set Reference Point",    RC::Set},
            {"Center Reference Point", RC::Center},
            {"Fixed Reference Point",  RC::Fixed},
        };
        for (const auto& e : kItems) {
            wxMenuItem* item = menu.AppendRadioItem(wxID_ANY, e.label);
            if (m_document.rCenterMode == e.mode) item->Check(true);
            const RC mode = e.mode;
            menu.Bind(wxEVT_MENU, [this, mode, world, refresh](wxCommandEvent&) {
                m_document.rCenterMode = mode;
                if (mode == RC::Set)
                    m_document.rCenter = world;   /* VB6 uses mouseCoords */
                else
                    m_document.rCenter = m_document.selectionCenter();
                refresh();
            }, item->GetId());
        }
        PopupMenu(&menu);
        return;
    }

    if (m_currentFunction == TOOL_WAYPOINT || m_currentFunction == TOOL_CONNECT) {
        /* VB6 mnuWaypoint (frm:633-655): the movement flags applied to
           newly-created waypoints. */
        wxMenu menu;
        static const char* kWayTypes[5] = {"Left", "Right", "Up", "Down", "Fly"};
        for (int i = 0; i < 5; ++i) {
            wxMenuItem* item = menu.AppendCheckItem(wxID_ANY, kWayTypes[i]);
            if (m_mainFrame != nullptr) item->Check(m_mainFrame->GetWaypointType(i));
            const int idx = i;
            menu.Bind(wxEVT_MENU, [this, idx, item](wxCommandEvent&) {
                if (m_mainFrame != nullptr)
                    m_mainFrame->SetWaypointType(idx, item->IsChecked());
            }, item->GetId());
        }
        PopupMenu(&menu);
        return;
    }

    /* Spawn-type names matching VB6 mnuSpawn index 0-16 */
    static const char* kSpawnNames[17] = {
        "Player Spawn",    "Alpha Team",      "Bravo Team",
        "Charlie Team",    "Delta Team",      "Alpha Flag",
        "Bravo Flag",      "Grenade Kit",     "Medikit",
        "Cluster Grenades","Vest",            "Flame Kit",
        "Berserker",       "Predator",        "Point Match Flag",
        "Rambo Bow",       "Stat Gun",
    };

    if (m_activeTool == TOOL_OBJECTS) {
        /* Objects context menu: choose spawn type, collider, or gostek */
        wxMenu menu;
        const int currentTeam = (m_mainFrame != nullptr) ? m_mainFrame->GetCurrentSpawnTeam() : 0;

        for (int i = 0; i < 17; ++i) {
            wxMenuItem* item = menu.AppendRadioItem(wxID_ANY, kSpawnNames[i]);
            if (i == currentTeam) item->Check(true);
            const int team = i;
            menu.Bind(wxEVT_MENU, [this, team](wxCommandEvent&) {
                if (m_mainFrame != nullptr)
                    m_mainFrame->SetCurrentSpawnTeam(team);
                m_document.showGostek = false;
            }, item->GetId());
        }
        menu.AppendSeparator();
        wxMenuItem* colliderItem = menu.AppendRadioItem(wxID_ANY, "Collider");
        if (currentTeam == -1) colliderItem->Check(true);  /* -1 = collider mode */
        menu.Bind(wxEVT_MENU, [this](wxCommandEvent&) {
            if (m_mainFrame != nullptr)
                m_mainFrame->SetCurrentSpawnTeam(-1);  /* -1 signals collider placement */
            m_document.showGostek = false;
        }, colliderItem->GetId());

        menu.AppendSeparator();
        /* VB6 mnuGostek (frm:14461): a soldier silhouette placed as a size
           reference.  Selecting it while already on drops the marker back to
           the origin, exactly as the original does. */
        wxMenuItem* gostekItem = menu.AppendCheckItem(wxID_ANY, "Gostek");
        gostekItem->Check(m_document.showGostek);
        menu.Bind(wxEVT_MENU, [this, refresh](wxCommandEvent&) {
            if (m_document.showGostek) {
                m_document.gostek = Vec2{0, 0};
            } else {
                m_document.showGostek = true;
                if (m_mainFrame != nullptr) m_mainFrame->SetCurrentSpawnTeam(-2);
            }
            refresh();
        }, gostekItem->GetId());

        PopupMenu(&menu);
        return;
    }

    if (m_activeTool == TOOL_VSELECT || m_activeTool == TOOL_PSELECT) {
        /* Vertex/Polygon select context menu */
        wxMenu menu;

        auto appendCmd = [&](const char* label, std::function<void()> fn) {
            wxMenuItem* item = menu.Append(wxID_ANY, label);
            const int id = item->GetId();
            menu.Bind(wxEVT_MENU, [fn](wxCommandEvent&) { fn(); }, id);
        };

        appendCmd("Duplicate", [this]() {
            if (!m_document.anySelected()) return;
            m_undoStack.push(m_document);
            m_document.duplicateSelected(32.0f, 0.0f);  /* VB6 mnuDuplicate: +32 X only */
            m_document.markModified();
            if (m_mainFrame != nullptr) { m_mainFrame->UpdateStatusBar(); m_mainFrame->UpdateTitle(); }
            Refresh(false);
        });
        appendCmd("Copy", [this]() {
            m_document.copySelected();
        });
        appendCmd("Paste", [this]() {
            if (!m_document.hasClipboard()) return;
            m_undoStack.push(m_document);
            m_document.pasteSelected();
            m_document.markModified();
            if (m_mainFrame != nullptr) { m_mainFrame->UpdateStatusBar(); m_mainFrame->UpdateTitle(); }
            Refresh(false);
        });
        appendCmd("Clear", [this]() {
            if (!m_document.anySelected()) return;
            m_undoStack.push(m_document);
            m_document.deleteSelected();
            m_document.markModified();
            if (m_mainFrame != nullptr) { m_mainFrame->UpdateStatusBar(); m_mainFrame->UpdateTitle(); }
            Refresh(false);
        });

        menu.AppendSeparator();

        /* Arrange submenu */
        wxMenu* arrange = new wxMenu();
        auto appendArrange = [&](const char* label, std::function<void()> fn) {
            wxMenuItem* item = arrange->Append(wxID_ANY, label);
            arrange->Bind(wxEVT_MENU, [fn](wxCommandEvent&) { fn(); }, item->GetId());
        };
        appendArrange("Bring To Front", [this]() {
            m_undoStack.push(m_document);
            m_document.bringSelectedToFront();
            m_document.markModified();
            Refresh(false);
        });
        appendArrange("Bring Forward", [this]() {
            m_undoStack.push(m_document);
            m_document.bringSelectedForward();
            m_document.markModified();
            Refresh(false);
        });
        appendArrange("Send Backward", [this]() {
            m_undoStack.push(m_document);
            m_document.sendSelectedBackward();
            m_document.markModified();
            Refresh(false);
        });
        appendArrange("Send To Back", [this]() {
            m_undoStack.push(m_document);
            m_document.sendSelectedToBack();
            m_document.markModified();
            Refresh(false);
        });
        menu.AppendSubMenu(arrange, "Arrange");

        /* Transform submenu */
        wxMenu* transform = new wxMenu();
        auto appendTransform = [&](const char* label, std::function<void()> fn) {
            wxMenuItem* item = transform->Append(wxID_ANY, label);
            transform->Bind(wxEVT_MENU, [fn](wxCommandEvent&) { fn(); }, item->GetId());
        };
        appendTransform("Rotate 180\xC2\xB0", [this]() {
            if (!m_document.anySelected()) return;
            m_undoStack.push(m_document);
            m_document.rotateSelected(180.0f);
            m_document.markModified();
            Refresh(false);
        });
        appendTransform("Rotate 90\xC2\xB0 CW", [this]() {
            if (!m_document.anySelected()) return;
            m_undoStack.push(m_document);
            m_document.rotateSelected(90.0f);
            m_document.markModified();
            Refresh(false);
        });
        appendTransform("Rotate 90\xC2\xB0 CCW", [this]() {
            if (!m_document.anySelected()) return;
            m_undoStack.push(m_document);
            m_document.rotateSelected(-90.0f);
            m_document.markModified();
            Refresh(false);
        });
        transform->AppendSeparator();
        appendTransform("Flip Horizontal", [this]() {
            if (!m_document.anySelected()) return;
            m_undoStack.push(m_document);
            m_document.flipSelected(true, false);
            m_document.markModified();
            Refresh(false);
        });
        appendTransform("Flip Vertical", [this]() {
            if (!m_document.anySelected()) return;
            m_undoStack.push(m_document);
            m_document.flipSelected(false, true);
            m_document.markModified();
            Refresh(false);
        });
        menu.AppendSubMenu(transform, "Transform");

        PopupMenu(&menu);
        return;
    }

    if (m_activeTool == TOOL_SCENERY) {
        /* Scenery context menu: Rotate/Scale toggles + Back/Middle/Front level */
        wxMenu menu;
        bool rot   = (m_mainFrame != nullptr) ? m_mainFrame->GetSceneryRotate() : false;
        bool scale = (m_mainFrame != nullptr) ? m_mainFrame->GetSceneryScale()  : false;
        int  level = (m_mainFrame != nullptr) ? m_mainFrame->GetSceneryLevel()  : 0;

        wxMenuItem* rotItem   = menu.AppendCheckItem(wxID_ANY, "Rotate");
        wxMenuItem* scaleItem = menu.AppendCheckItem(wxID_ANY, "Scale");
        rotItem->Check(rot);
        scaleItem->Check(scale);
        menu.Bind(wxEVT_MENU, [this, rotItem](wxCommandEvent&) {
            if (m_mainFrame != nullptr)
                m_mainFrame->SetSceneryRotate(rotItem->IsChecked());
        }, rotItem->GetId());
        menu.Bind(wxEVT_MENU, [this, scaleItem](wxCommandEvent&) {
            if (m_mainFrame != nullptr)
                m_mainFrame->SetSceneryScale(scaleItem->IsChecked());
        }, scaleItem->GetId());

        menu.AppendSeparator();

        static const char* kLevelNames[3] = {"Back", "Middle", "Front"};
        for (int i = 0; i < 3; ++i) {
            wxMenuItem* lvl = menu.AppendRadioItem(wxID_ANY, kLevelNames[i]);
            if (i == level) lvl->Check(true);
            const int lv = i;
            menu.Bind(wxEVT_MENU, [this, lv](wxCommandEvent&) {
                if (m_mainFrame != nullptr)
                    m_mainFrame->SetSceneryLevel(lv);
            }, lvl->GetId());
        }
        PopupMenu(&menu);
        return;
    }

    event.Skip();
}

void GlViewport::OnKeyDown(wxKeyEvent& event) {
    const int key = event.GetKeyCode();

    if (key == WXK_ESCAPE) {
        /* VB6 frm:10954: Escape cancels a pending creation, quad or waypoint
           anchor if one exists; only otherwise does it clear the selection. */
        if (m_state == ViewportState::CreatingPoly ||
            m_document.currentWaypoint >= 0) {
            CancelCreation();
        } else {
            m_document.clearSelection();
        }
        Refresh(false);
        return;
    }

    if (key == WXK_TAB) {
        /* VB6 frm:10951 -> TabPressed: cycle the single selected polygon /
           vertex / scenery item.  Shift reverses direction. */
        if (m_document.cycleSelection(event.ShiftDown())) Refresh(false);
        return;
    }

    if (key == WXK_SPACE) {
        m_spaceDown = true;
        SetCursor(wxCursor(wxCURSOR_HAND));
        event.Skip();
        return;
    }

    /* Arrow-key nudge is owned by MainFrame::OnKeyDown so that the step size is
       identical whether or not the canvas has focus (VB6 handled arrows in a
       single global key poller). */

    /* Recompute effective function when modifier state changes */
    int fn = ComputeCurrentFunction(event.ShiftDown(), event.ControlDown(), event.AltDown());
    if (fn != m_currentFunction) {
        m_currentFunction = fn;
        applyToolCursor();
    }

    event.Skip();
}

void GlViewport::OnKeyUp(wxKeyEvent& event) {
    const int key = event.GetKeyCode();

    if (key == WXK_SPACE) {
        m_spaceDown = false;
        applyToolCursor();
        event.Skip();
        return;
    }

    /* Restore effective function when modifier is released */
    int fn = ComputeCurrentFunction(event.ShiftDown(), event.ControlDown(), event.AltDown());
    if (fn != m_currentFunction) {
        m_currentFunction = fn;
        applyToolCursor();
    }

    event.Skip();
}

/* ---- Editing interaction ------------------------------------------------ */

float GlViewport::WorldTolerance() const {
    return 6.0f / m_document.zoom;
}

void GlViewport::HandleLeftDownEdit(const wxMouseEvent& event) {
    const Vec2 world = m_document.screenToWorld(
        {static_cast<float>(event.GetX()), static_cast<float>(event.GetY())});

    /* Compute effective function from held modifiers */
    m_currentFunction = ComputeCurrentFunction(
        event.ShiftDown(), event.ControlDown(), event.AltDown());

    m_dragWorldStart = world;
    m_dragWorldLast  = world;
    m_didDrag        = false;

    if (!HasCapture()) CaptureMouse();

    /* Map current function to SelectMode for selection tools */
    auto selectMode = [this]() -> MapDocument::SelectMode {
        if (m_currentFunction == TOOL_VSELADD || m_currentFunction == TOOL_PSELADD)
            return MapDocument::SelectMode::Add;
        if (m_currentFunction == TOOL_VSELSUB || m_currentFunction == TOOL_PSELSUB)
            return MapDocument::SelectMode::Subtract;
        return MapDocument::SelectMode::Replace;
    };

    switch (m_currentFunction) {

    case TOOL_CREATE:
        AddCreationVertex(world);
        return;

    case TOOL_VSELECT:
    case TOOL_VSELADD:
    case TOOL_VSELSUB:
    case TOOL_MOVE: {
        MapDocument::SelectMode mode = selectMode();
        bool hit = m_document.selectVertexAt(world, WorldTolerance(), mode);
        if (hit) {
            m_state = ViewportState::Dragging;
            m_undoStack.push(m_document);
        } else {
            if (mode == MapDocument::SelectMode::Replace)
                m_document.clearSelection();
            m_state   = ViewportState::RubberBanding;
            m_rubberA = world;
            m_rubberB = world;
        }
        Refresh(false);
        return;
    }

    case TOOL_PSELECT:
    case TOOL_PSELADD:
    case TOOL_PSELSUB: {
        MapDocument::SelectMode mode = selectMode();
        bool hit = m_document.selectPolyAt(world, mode);
        if (hit) {
            m_state = ViewportState::Dragging;
            m_undoStack.push(m_document);
        } else {
            if (mode == MapDocument::SelectMode::Replace)
                m_document.clearSelection();
            m_state   = ViewportState::RubberBanding;
            m_rubberA = world;
            m_rubberB = world;
        }
        Refresh(false);
        return;
    }

    case TOOL_PCOLOR: {
        /* ColorFill: colors all selected vertices, or all vertices of clicked poly */
        m_undoStack.push(m_document);
        bool applied = m_document.applyColorToSelected(
            m_paintR, m_paintG, m_paintB, m_paintOpacity, m_paintBlendMode);
        if (!applied)
            m_document.applyColorToPolyAt(world, m_paintR, m_paintG, m_paintB,
                                           m_paintOpacity, m_paintBlendMode);
        if (m_mainFrame != nullptr) m_mainFrame->UpdateTitle();
        Refresh(false);
        return;
    }

    case TOOL_VCOLOR: {
        /* VertexColoring: initiate a paint-drag session */
        m_undoStack.push(m_document);
        m_state = ViewportState::Dragging;  /* re-use Dragging state for continuous paint */
        float worldRadius = m_paintRadius / m_document.zoom;
        m_document.applyColorToVerticesNear(world, worldRadius,
                                             m_paintR, m_paintG, m_paintB,
                                             m_paintOpacity, m_paintBlendMode);
        if (m_mainFrame != nullptr) m_mainFrame->UpdateTitle();
        Refresh(false);
        return;
    }

    case TOOL_OBJECTS: {
        const int team = (m_mainFrame != nullptr) ? m_mainFrame->GetCurrentSpawnTeam() : 0;
        if (m_document.showGostek) {
            /* VB6 frm:11292: while the gostek reference is active, clicking
               just repositions the silhouette; nothing is added to the map. */
            m_document.gostek = world;
        } else if (team < 0) {
            m_undoStack.push(m_document);
            m_document.addCollider(world.x, world.y);
            m_document.markModified();
        } else {
            m_undoStack.push(m_document);
            m_document.addSpawn(world.x, world.y, team);
            m_document.markModified();
        }
        if (m_mainFrame != nullptr) { m_mainFrame->UpdateStatusBar(); m_mainFrame->UpdateTitle(); }
        Refresh(false);
        return;
    }

    case TOOL_LIGHTS:
        m_undoStack.push(m_document);
        m_document.addLight(world.x, world.y);
        m_document.markModified();
        if (m_mainFrame != nullptr) { m_mainFrame->UpdateStatusBar(); m_mainFrame->UpdateTitle(); }
        Refresh(false);
        return;

    case TOOL_WAYPOINT:
        m_undoStack.push(m_document);
        {
            /* VB6 frm:11322-11345 stamps the mnuWayType flags onto the new
               waypoint and, when a waypoint is already anchored, chains a
               connection from it. */
            m_document.addWaypoint(world.x, world.y);
            EditorWaypoint& wp = m_document.waypoints.back();
            if (m_mainFrame != nullptr) {
                wp.left  = m_mainFrame->GetWaypointType(0);
                wp.right = m_mainFrame->GetWaypointType(1);
                wp.up    = m_mainFrame->GetWaypointType(2);
                wp.down  = m_mainFrame->GetWaypointType(3);
                wp.m2    = m_mainFrame->GetWaypointType(4);
            }
            const int newIdx = static_cast<int>(m_document.waypoints.size()) - 1;
            const int prev   = m_document.currentWaypoint;
            if (prev >= 0 && prev < newIdx &&
                m_document.waypoints[static_cast<size_t>(prev)].connections.size() < 20) {
                m_document.waypoints[static_cast<size_t>(prev)]
                    .connections.push_back(wp.id);
            }
            m_document.currentWaypoint = newIdx;
        }
        m_document.markModified();
        if (m_mainFrame != nullptr) { m_mainFrame->UpdateStatusBar(); m_mainFrame->UpdateTitle(); }
        Refresh(false);
        return;

    case TOOL_CONNECT:
        /* VB6 CreateConnection (frm:7887): anchor on a waypoint, then link
           subsequent clicks to it. */
        m_undoStack.push(m_document);
        if (!m_document.connectWaypointAt(world, 8.0f / m_document.zoom))
            m_undoStack.pop();
        if (m_mainFrame != nullptr) { m_mainFrame->UpdateStatusBar(); m_mainFrame->UpdateTitle(); }
        Refresh(false);
        return;

    case TOOL_COLORPICK:
    case TOOL_LITPICKER:
    case TOOL_PIXPICKER: {
        /* VB6 ColorPicker / LightPicker / DepthPicker (frm:7711-7860).
           The Depthmap tool's Alt-pick reads the vertex depth instead of a
           colour; every picker samples the vertex nearest the cursor among
           the polygons that contain it. */
        int pi = -1, vi = -1;
        if (m_document.pickVertexInPoly(world, 32.0f, pi, vi)) {
            const EditorVertex& v = m_document.polys[static_cast<size_t>(pi)].v[vi];
            if (m_activeTool == TOOL_DEPTHMAP) {
                int d = static_cast<int>(v.z);
                d = d < 0 ? 0 : (d > 255 ? 255 : d);
                const uint8_t g = static_cast<uint8_t>(d);
                if (m_mainFrame != nullptr) m_mainFrame->SetPaintColorFromPicker(g, g, g);
            } else if (m_mainFrame != nullptr) {
                m_mainFrame->SetPaintColorFromPicker(v.r, v.g, v.b);
            }
        }
        Refresh(false);
        return;
    }

    case TOOL_DEPTHMAP:
        /* VB6 EditDepthMap (frm:7662): paints the red channel of the current
           colour into the vertex depth, with the palette brush radius. */
        m_undoStack.push(m_document);
        m_state = ViewportState::Dragging;
        m_document.applyDepthNear(world, m_paintRadius / m_document.zoom,
                                  static_cast<float>(m_paintR), m_paintOpacity);
        if (m_mainFrame != nullptr) m_mainFrame->UpdateTitle();
        Refresh(false);
        return;

    case TOOL_TEXTURE:
        /* VB6 StretchingTexture (frm:7860) slides the UVs of the selected
           vertices as the mouse drags. */
        if (!m_document.anySelected()) return;
        m_undoStack.push(m_document);
        m_state = ViewportState::Dragging;
        return;

    case TOOL_ERASER:
        m_undoStack.push(m_document);
        m_state = ViewportState::Dragging;
        if (!m_document.eraseSketchAt(world, m_paintRadius / m_document.zoom)) {
            /* Keep the snapshot: the drag may still erase on a later move. */
        }
        if (m_mainFrame != nullptr) m_mainFrame->UpdateTitle();
        Refresh(false);
        return;

    case TOOL_SMUDGE:
        m_undoStack.push(m_document);
        m_state = ViewportState::Dragging;
        return;

    case TOOL_SCALE:
    case TOOL_ROTATE:
        /* Ctrl-drag scales and Alt-drag rotates the current selection about
           the centre of its bounding rectangle (VB6 Scaling / Rotating,
           frm:7170 / frm:7355). */
        if (!m_document.anySelected()) return;
        m_undoStack.push(m_document);
        BeginTransformDrag(world);
        return;

    case TOOL_SKETCH:
        /* VB6 frm:11370: unmodified sketching is freehand (StartSketch +
           LinkSketch); Shift anchors a single straight line. */
        m_undoStack.push(m_document);
        m_state   = ViewportState::Sketching;
        m_rubberA = world;
        m_rubberB = world;
        m_sketchStraight = event.ShiftDown();
        if (!m_sketchStraight)
            m_document.beginSketchStroke(world);
        if (m_mainFrame != nullptr) m_mainFrame->UpdateTitle();
        Refresh(false);
        return;

    case TOOL_SCENERY: {
        if (m_mainFrame == nullptr) return;
        int idx = m_mainFrame->GetOrAddSelectedSceneryIndex();
        if (idx == 0) return;
        m_undoStack.push(m_document);
        const int level = m_mainFrame->GetSceneryLevel();
        m_document.addSceneryInstance(idx, world.x, world.y, level);
        m_document.markModified();
        m_mainFrame->UpdateStatusBar();
        m_mainFrame->UpdateTitle();
        Refresh(false);
        return;
    }

    default:
        break;
    }
}

void GlViewport::BeginTransformDrag(Vec2 world) {
    m_document.beginTransform(m_transform);
    if (m_transform.empty()) { m_state = ViewportState::Idle; return; }
    m_state          = ViewportState::Transforming;
    m_dragWorldStart = world;
    m_dragWorldLast  = world;
    m_didDrag        = false;
}

void GlViewport::UpdateTransformDrag(Vec2 world, bool shiftDown) {
    const Vec2  c     = m_transform.center;
    const Vec2  start = m_dragWorldStart;

    if (m_currentFunction == TOOL_ROTATE) {
        /* VB6 Rotating (frm:7374-7404): angle delta between the drag origin
           and the cursor, both measured from the rotation centre.  Shift
           quantises the total rotation to 15° steps. */
        float a0 = std::atan2(start.y - c.y, start.x - c.x);
        float a1 = std::atan2(world.y - c.y, world.x - c.x);
        float delta = a1 - a0;
        if (shiftDown) {
            const float kPi = 3.14159265358979f;
            float deg = delta * 180.0f / kPi;
            delta = std::floor((deg + 7.5f) / 15.0f) * 15.0f / 180.0f * kPi;
        }
        m_document.applyTransform(m_transform, 1.0f, 1.0f, delta);
        return;
    }

    /* TOOL_SCALE — VB6 Scaling (frm:7189-7209). */
    float sx = 1.0f, sy = 1.0f;
    if (shiftDown) {
        /* Ctrl+Shift: proportional scale on both axes. */
        float dnx = start.x - c.x, dny = start.y - c.y;
        float num, den;
        if (dnx * dny > 0) {
            num = (world.x - c.x) + (world.y - c.y);
            den = dnx + dny;
        } else {
            num = (world.x - c.x) - (world.y - c.y);
            den = dnx - dny;
        }
        sx = sy = (den != 0.0f) ? num / den : 1.0f;
    } else {
        if (start.x != c.x) sx = 1.0f + (world.x - start.x) / (start.x - c.x);
        if (start.y != c.y) sy = 1.0f + (world.y - start.y) / (start.y - c.y);
    }
    m_document.applyTransform(m_transform, sx, sy, 0.0f);
}

void GlViewport::HandleMouseMoveEdit(const wxMouseEvent& event) {
    const Vec2 world = m_document.screenToWorld(
        {static_cast<float>(event.GetX()), static_cast<float>(event.GetY())});

    /* Update effective tool when modifiers change during a move */
    int newFn = ComputeCurrentFunction(
        event.ShiftDown(), event.ControlDown(), event.AltDown());
    if (newFn != m_currentFunction) {
        m_currentFunction = newFn;
        applyToolCursor();
    }

    float dx = world.x - m_dragWorldStart.x;
    float dy = world.y - m_dragWorldStart.y;
    float screenDist = std::sqrt(dx*dx + dy*dy) * m_document.zoom;
    if (screenDist > kDragThreshold)
        m_didDrag = true;

    if (m_state == ViewportState::Transforming) {
        if (m_didDrag)
            UpdateTransformDrag(world, event.ShiftDown());
        m_dragWorldLast = world;
        Refresh(false);
        return;
    }

    if (m_state == ViewportState::Dragging && m_didDrag) {
        if (m_currentFunction == TOOL_VCOLOR) {
            /* Continuous vertex color painting while dragging */
            float worldRadius = m_paintRadius / m_document.zoom;
            m_document.applyColorToVerticesNear(world, worldRadius,
                                                 m_paintR, m_paintG, m_paintB,
                                                 m_paintOpacity, m_paintBlendMode);
            if (m_mainFrame != nullptr) m_mainFrame->UpdateTitle();
        } else if (m_currentFunction == TOOL_DEPTHMAP) {
            m_document.applyDepthNear(world, m_paintRadius / m_document.zoom,
                                      static_cast<float>(m_paintR), m_paintOpacity);
            if (m_mainFrame != nullptr) m_mainFrame->UpdateTitle();
        } else if (m_currentFunction == TOOL_ERASER) {
            m_document.eraseSketchAt(world, m_paintRadius / m_document.zoom);
            if (m_mainFrame != nullptr) m_mainFrame->UpdateTitle();
        } else if (m_currentFunction == TOOL_SMUDGE) {
            m_document.smudgeSketchAt(world,
                                      world.x - m_dragWorldLast.x,
                                      world.y - m_dragWorldLast.y,
                                      m_paintRadius / m_document.zoom);
            m_dragWorldLast = world;
            if (m_mainFrame != nullptr) m_mainFrame->UpdateTitle();
            Refresh(false);
            return;
        } else if (m_currentFunction == TOOL_TEXTURE) {
            /* VB6 StretchingTexture (frm:7869-7871) divides the screen-space
               delta by the zoom *and* the texture dimensions, so the texture
               tracks the cursor 1:1 regardless of zoom. */
            Vec2 target = world;
            if (event.ShiftDown()) {
                if (std::fabs(world.x - m_dragWorldStart.x) >=
                    std::fabs(world.y - m_dragWorldStart.y))
                    target.y = m_dragWorldStart.y;
                else
                    target.x = m_dragWorldStart.x;
            }
            auto [texW, texH] = getTextureSize(m_document);
            const float tw = texW > 0 ? static_cast<float>(texW) : 1.0f;
            const float th = texH > 0 ? static_cast<float>(texH) : 1.0f;
            m_document.offsetTextureOnSelected((target.x - m_dragWorldLast.x) / tw,
                                               (target.y - m_dragWorldLast.y) / th);
            m_dragWorldLast = target;
            if (m_mainFrame != nullptr) m_mainFrame->UpdateTitle();
            Refresh(false);
            return;
        } else {
            /* VB6 constrains movement to one axis while Shift is held
               (frm:11469-11474): whichever axis has moved further wins. */
            Vec2 target = world;
            if (event.ShiftDown()) {
                if (std::fabs(world.x - m_dragWorldStart.x) >=
                    std::fabs(world.y - m_dragWorldStart.y))
                    target.y = m_dragWorldStart.y;
                else
                    target.x = m_dragWorldStart.x;
            }
            float moveDx = target.x - m_dragWorldLast.x;
            float moveDy = target.y - m_dragWorldLast.y;
            m_document.moveSelected(moveDx, moveDy);
            m_dragWorldLast = target;
            if (m_mainFrame != nullptr) {
                m_mainFrame->UpdateStatusBar();
                m_mainFrame->UpdateTitle();
            }
            if (m_state != ViewportState::Idle) Refresh(false);
            return;
        }
    }

    if (m_state == ViewportState::RubberBanding ||
        m_state == ViewportState::Sketching) {
        m_rubberB = world;
        /* VB6 LinkSketch (frm:8133) commits a segment and starts a fresh one
           every time the cursor gets more than 16 world units from the
           current segment's origin, giving freehand strokes. */
        if (m_state == ViewportState::Sketching && !m_sketchStraight) {
            if (m_document.extendSketchStroke(world))
                m_rubberA = world;
        }
    }

    m_dragWorldLast = world;
    if (m_state != ViewportState::Idle)
        Refresh(false);
}

void GlViewport::HandleLeftUpEdit(const wxMouseEvent& event) {
    const Vec2 world = m_document.screenToWorld(
        {static_cast<float>(event.GetX()), static_cast<float>(event.GetY())});

    if (HasCapture()) ReleaseMouse();

    /* Determine select mode for rubber-band completion */
    auto rubberMode = [this]() -> MapDocument::SelectMode {
        if (m_currentFunction == TOOL_VSELADD || m_currentFunction == TOOL_PSELADD)
            return MapDocument::SelectMode::Add;
        if (m_currentFunction == TOOL_VSELSUB || m_currentFunction == TOOL_PSELSUB)
            return MapDocument::SelectMode::Subtract;
        return MapDocument::SelectMode::Replace;
    };

    if (m_state == ViewportState::RubberBanding && m_didDrag) {
        bool isPoly = (m_currentFunction == TOOL_PSELECT ||
                       m_currentFunction == TOOL_PSELADD ||
                       m_currentFunction == TOOL_PSELSUB);
        if (isPoly)
            m_document.selectPolysInRect(m_rubberA, m_rubberB, rubberMode());
        else
            m_document.selectVerticesInRect(m_rubberA, m_rubberB, rubberMode());
    }

    if (m_state == ViewportState::Sketching) {
        /* Straight-line mode commits the whole drag as one segment; freehand
           mode has already committed its segments during the move, so only
           the trailing stub needs flushing. */
        if (m_sketchStraight) {
            if (m_didDrag) {
                m_document.addSketchLine(m_rubberA, world);
                m_document.markModified();
            } else {
                m_undoStack.pop();
            }
        } else {
            m_document.extendSketchStroke(world, true);
            m_document.markModified();
        }
        m_sketchStraight = false;
        if (m_mainFrame != nullptr) m_mainFrame->UpdateTitle();
    }

    /* VCOLOR drag ends: undo was pushed at drag start, no extra action needed */

    if (m_state == ViewportState::Transforming && !m_didDrag)
        m_undoStack.pop();  /* click without drag: nothing changed */

    /* VB6 calls SnapSelected on mouse-up after a move (frm:11607). */
    if (m_state == ViewportState::Dragging && m_didDrag &&
        m_currentFunction != TOOL_VCOLOR && m_currentFunction != TOOL_PCOLOR) {
        m_document.snapSelected(m_snapRadius);
    }

    if (m_state == ViewportState::Dragging && !m_didDrag &&
        m_currentFunction != TOOL_VCOLOR && m_currentFunction != TOOL_PCOLOR) {
        /* Click without drag: pop the snapshot we took (nothing moved) */
        m_undoStack.pop();
        bool isPoly = (m_currentFunction == TOOL_PSELECT ||
                       m_currentFunction == TOOL_PSELADD ||
                       m_currentFunction == TOOL_PSELSUB);
        if (isPoly)
            m_document.selectPolyAt(world, rubberMode());
        else
            m_document.selectVertexAt(world, WorldTolerance(), rubberMode());
    }

    m_state   = ViewportState::Idle;
    m_didDrag = false;

    /* Restore effective function after drag ends */
    m_currentFunction = ComputeCurrentFunction(
        event.ShiftDown(), event.ControlDown(), event.AltDown());

    if (m_mainFrame != nullptr) {
        m_mainFrame->UpdateStatusBar();
        m_mainFrame->UpdateTitle();
    }
    Refresh(false);
}

void GlViewport::AddCreationVertex(Vec2 worldPos) {
    if (m_state != ViewportState::CreatingPoly)
        m_state = ViewportState::CreatingPoly;

    /* VB6 CreatePoly (frm:7955) snaps the new vertex either to the grid or to
       a nearby existing vertex before storing it. */
    m_document.snapPoint(worldPos, m_snapRadius);

    m_creationVerts[m_creationVertCount++] = worldPos;

    if (m_creationVertCount == kMaxCreationVerts) {
        m_undoStack.push(m_document);
        auto [texW, texH] = getTextureSize(m_document);
        const float tw = texW > 0 ? static_cast<float>(texW) : 1.0f;
        const float th = texH > 0 ? static_cast<float>(texH) : 1.0f;

        EditorPoly poly;
        for (int i = 0; i < 3; ++i) {
            poly.v[i].world = m_creationVerts[i];
            /* VB6 colours new vertices with the current palette colour and
               opacity (frm:7993-7997). */
            poly.v[i].r = m_paintR;
            poly.v[i].g = m_paintG;
            poly.v[i].b = m_paintB;
            poly.v[i].alpha =
                static_cast<uint8_t>(255.0f * m_paintOpacity + 0.5f);
            poly.v[i].tu = m_creationVerts[i].x / tw;
            poly.v[i].tv = m_creationVerts[i].y / th;
            poly.v[i].selected = true;
        }

        /* VB6 frm:7999-8020: in Textured Quad mode with User Defined X/Y
           enabled the UVs come from the rectangle selected in the Texture
           window instead of from world coordinates, so the quad shows exactly
           that patch of the texture. */
        if (m_currentFunction == TOOL_QUAD && m_mainFrame != nullptr) {
            float u1 = 0, v1 = 0, u2 = 1, v2 = 1;
            if (m_mainFrame->GetTextureSelection(u1, v1, u2, v2)) {
                if (m_mainFrame->GetCustomTexX()) {
                    if (m_creatingQuad) {
                        poly.v[2].tu = u1;
                    } else {
                        poly.v[0].tu = u1;
                        poly.v[1].tu = u2;
                        poly.v[2].tu = u2;
                    }
                }
                if (m_mainFrame->GetCustomTexY()) {
                    if (m_creatingQuad) {
                        poly.v[2].tv = v2;
                    } else {
                        poly.v[0].tv = v1;
                        poly.v[1].tv = v1;
                        poly.v[2].tv = v2;
                    }
                }
            }
        }
        if (m_creatingQuad) {
            /* The second triangle reuses vertices 1 and 3 of the first, so
               their UVs must be carried over verbatim (frm:8058-8066). */
            poly.v[0].tu = m_quadCarryUV[0].x; poly.v[0].tv = m_quadCarryUV[0].y;
            poly.v[1].tu = m_quadCarryUV[1].x; poly.v[1].tv = m_quadCarryUV[1].y;
        }

        poly.polyType = (m_mainFrame != nullptr)
                            ? m_mainFrame->GetCreationPolyType()
                            : POLY_NORMAL;
        /* VB6 forces clockwise winding after the third click (frm:8032). */
        const Vec2& a = poly.v[0].world;
        const Vec2& b = poly.v[1].world;
        const Vec2& c = poly.v[2].world;
        const float cross = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
        if (cross < 0.0f) std::swap(poly.v[1], poly.v[2]);

        m_document.addPoly(poly);
        m_document.markModified();
        if (m_mainFrame != nullptr) {
            m_mainFrame->UpdateStatusBar();
            m_mainFrame->UpdateTitle();
        }

        if (m_currentFunction == TOOL_QUAD && !m_creatingQuad) {
            /* VB6 frm:8054: quad mode immediately starts a second triangle
               seeded with vertices 1 and 3 of the one just finished, so the
               fourth click closes the quad. */
            m_creationVerts[0] = poly.v[0].world;
            m_creationVerts[1] = poly.v[2].world;
            m_quadCarryUV[0] = {poly.v[0].tu, poly.v[0].tv};
            m_quadCarryUV[1] = {poly.v[2].tu, poly.v[2].tv};
            m_creationVertCount = 2;
            m_creatingQuad = true;
        } else {
            m_creatingQuad = false;
            m_creationVertCount = 0;
            m_state = ViewportState::Idle;
        }
    }
    Refresh(false);
}

void GlViewport::CancelCreation() {
    m_creationVertCount = 0;
    m_creatingQuad = false;
    m_document.currentWaypoint = -1;
    m_state = ViewportState::Idle;
}

/* ---- Panning ------------------------------------------------------------ */

bool GlViewport::IsSpacePanGesture(const wxMouseEvent& event) const {
    return event.LeftIsDown() && wxGetKeyState(WXK_SPACE);
}

void GlViewport::BeginPan(const wxPoint& point) {
    m_panning      = true;
    m_lastPanPoint = point;
    /* VB6 frm:11101 sets the hand cursor for the duration of a middle-drag. */
    SetCursor(wxCursor(wxCURSOR_HAND));
    if (!HasCapture()) CaptureMouse();
}

void GlViewport::UpdatePan(const wxPoint& point) {
    const wxPoint delta = point - m_lastPanPoint;
    m_lastPanPoint = point;
    m_document.scrollX -= static_cast<float>(delta.x) / m_document.zoom;
    m_document.scrollY -= static_cast<float>(delta.y) / m_document.zoom;
    m_document.rebuildScreenCache();
    if (m_mainFrame != nullptr) {
        const Vec2 world = m_document.screenToWorld(
            {static_cast<float>(point.x), static_cast<float>(point.y)});
        m_mainFrame->UpdateMouseWorldPosition(world);
        m_mainFrame->UpdateStatusBar();
    }
    Refresh(false);
}

void GlViewport::EndPan() {
    m_panning = false;
    if (HasCapture()) ReleaseMouse();
    if (!m_spaceDown) applyToolCursor();
}

/* ---- Fallback ----------------------------------------------------------- */

void GlViewport::DrawFallback(wxDC& dc) {
    const wxSize size = GetClientSize();
    dc.SetBackground(wxBrush(wxColour(0x2B, 0x1A, 0x0E)));
    dc.Clear();

    dc.SetPen(wxPen(wxColour(90, 90, 90)));
    const int step = 100;
    const int startX = static_cast<int>(std::floor(m_document.scrollX / step) * step);
    const int startY = static_cast<int>(std::floor(m_document.scrollY / step) * step);
    const float right  = m_document.scrollX + static_cast<float>(size.x) / m_document.zoom;
    const float bottom = m_document.scrollY + static_cast<float>(size.y) / m_document.zoom;

    for (int worldX = startX; worldX <= static_cast<int>(right); worldX += step) {
        const int screenX = static_cast<int>((static_cast<float>(worldX) - m_document.scrollX) * m_document.zoom);
        dc.DrawLine(screenX, 0, screenX, size.y);
    }
    for (int worldY = startY; worldY <= static_cast<int>(bottom); worldY += step) {
        const int screenY = static_cast<int>((static_cast<float>(worldY) - m_document.scrollY) * m_document.zoom);
        dc.DrawLine(0, screenY, size.x, screenY);
    }

    dc.SetTextForeground(*wxWHITE);
    dc.DrawText("No OpenGL context - fallback rendering", wxPoint(12, 12));
}
