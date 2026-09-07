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
    : GlViewportBase(parent, BuildGlAttributes(), wxID_ANY, wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE),
#else
    : GlViewportBase(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE),
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

/* Maps tool index to cursor filename (no extension). */
static const char* kToolCursorNames[14] = {
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
};

void GlViewport::loadCursors(const std::string& skinsPath) {
    if (skinsPath.empty()) return;
    const wxString cursorDir = wxString::FromUTF8(skinsPath) + wxFILE_SEP_PATH + "cursors";
    for (int i = 0; i < kNumTools; ++i) {
        wxString curPath = cursorDir + wxFILE_SEP_PATH
                           + wxString::FromUTF8(kToolCursorNames[i]) + ".cur";
        if (wxFileExists(curPath)) {
            wxImage img(curPath, wxBITMAP_TYPE_CUR);
            if (img.IsOk()) {
                m_toolCursors[i] = wxCursor(img);
            }
        }
    }
    applyToolCursor();
}

void GlViewport::applyToolCursor() {
    if (m_activeTool >= 0 && m_activeTool < kNumTools) {
        const wxCursor& c = m_toolCursors[m_activeTool];
        if (c.IsOk()) {
            SetCursor(c);
            return;
        }
    }
    SetCursor(wxNullCursor);
}

void GlViewport::addTexturePath(const std::string& path) {
#if PW_HAS_WX_GLCANVAS && PW_HAS_OPENGL_HEADERS
    m_texMgr.addSearchPath(path);
#else
    (void)path;
#endif
}

void GlViewport::setActiveTool(int tool) {
    if (m_activeTool == tool) return;
    if (m_state == ViewportState::CreatingPoly)
        CancelCreation();
    m_state = ViewportState::Idle;
    m_activeTool = tool;
    applyToolCursor();
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
    if (m_state == ViewportState::CreatingPoly) {
        CancelCreation();
        Refresh(false);
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
            }, item->GetId());
        }
        menu.AppendSeparator();
        wxMenuItem* colliderItem = menu.AppendRadioItem(wxID_ANY, "Collider");
        if (currentTeam == -1) colliderItem->Check(true);  /* -1 = collider mode */
        menu.Bind(wxEVT_MENU, [this](wxCommandEvent&) {
            if (m_mainFrame != nullptr)
                m_mainFrame->SetCurrentSpawnTeam(-1);  /* -1 signals collider placement */
        }, colliderItem->GetId());
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
            m_document.duplicateSelected(10.0f, 10.0f);
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
    if (event.GetKeyCode() == WXK_ESCAPE) {
        if (m_state == ViewportState::CreatingPoly) {
            CancelCreation();
        } else {
            /* ESC when idle → deselect all */
            m_document.clearSelection();
        }
        Refresh(false);
        return;
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
    const bool additive = event.ShiftDown();

    m_dragWorldStart = world;
    m_dragWorldLast  = world;
    m_didDrag        = false;

    if (!HasCapture()) CaptureMouse();

    switch (m_activeTool) {
    case TOOL_CREATE:
        AddCreationVertex(world);
        return;

    case TOOL_VSELECT:
    case TOOL_MOVE: {
        bool hit = m_document.selectVertexAt(world, WorldTolerance(), additive);
        if (hit) {
            m_state = ViewportState::Dragging;
            m_undoStack.push(m_document);
        } else {
            if (!additive) m_document.clearSelection();
            m_state   = ViewportState::RubberBanding;
            m_rubberA = world;
            m_rubberB = world;
        }
        Refresh(false);
        return;
    }

    case TOOL_PSELECT: {
        bool hit = m_document.selectPolyAt(world, additive);
        if (hit) {
            m_state = ViewportState::Dragging;
            m_undoStack.push(m_document);
        } else {
            if (!additive) m_document.clearSelection();
            m_state   = ViewportState::RubberBanding;
            m_rubberA = world;
            m_rubberB = world;
        }
        Refresh(false);
        return;
    }

    case TOOL_OBJECTS: {
        const int team = (m_mainFrame != nullptr) ? m_mainFrame->GetCurrentSpawnTeam() : 0;
        if (team < 0) {
            /* Negative team means place collider */
            m_undoStack.push(m_document);
            m_document.addCollider(world.x, world.y);
        } else {
            m_undoStack.push(m_document);
            m_document.addSpawn(world.x, world.y, team);
        }
        m_document.markModified();
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
        m_document.addWaypoint(world.x, world.y);
        m_document.markModified();
        if (m_mainFrame != nullptr) { m_mainFrame->UpdateStatusBar(); m_mainFrame->UpdateTitle(); }
        Refresh(false);
        return;

    case TOOL_SCENERY: {
        if (m_mainFrame == nullptr) return;
        int idx = m_mainFrame->GetOrAddSelectedSceneryIndex();
        if (idx == 0) return; /* nothing selected in SceneryPanel */
        m_undoStack.push(m_document);
        const int level = m_mainFrame->GetSceneryLevel();
        m_document.addSceneryInstance(idx, world.x, world.y, level);
        m_document.markModified();
        m_mainFrame->UpdateStatusBar();
        m_mainFrame->UpdateTitle();
        Refresh(false);
        return;
    }

    case TOOL_SKETCH:
        /* Sketch: left-drag to draw a line — handled via drag state */
        m_state = ViewportState::Sketching;
        m_rubberA = world;
        m_rubberB = world;
        return;

    default:
        break;
    }
}

void GlViewport::HandleMouseMoveEdit(const wxMouseEvent& event) {
    const Vec2 world = m_document.screenToWorld(
        {static_cast<float>(event.GetX()), static_cast<float>(event.GetY())});

    float dx = world.x - m_dragWorldStart.x;
    float dy = world.y - m_dragWorldStart.y;
    float screenDist = std::sqrt(dx*dx + dy*dy) * m_document.zoom;
    if (screenDist > kDragThreshold)
        m_didDrag = true;

    if (m_state == ViewportState::Dragging && m_didDrag) {
        float moveDx = world.x - m_dragWorldLast.x;
        float moveDy = world.y - m_dragWorldLast.y;
        m_document.moveSelected(moveDx, moveDy);
        if (m_mainFrame != nullptr) {
            m_mainFrame->UpdateStatusBar();
            m_mainFrame->UpdateTitle();
        }
    }

    if (m_state == ViewportState::RubberBanding ||
        m_state == ViewportState::Sketching)
        m_rubberB = world;

    m_dragWorldLast = world;
    if (m_state != ViewportState::Idle)
        Refresh(false);
}

void GlViewport::HandleLeftUpEdit(const wxMouseEvent& event) {
    const Vec2 world = m_document.screenToWorld(
        {static_cast<float>(event.GetX()), static_cast<float>(event.GetY())});
    const bool additive = event.ShiftDown();

    if (HasCapture()) ReleaseMouse();

    if (m_state == ViewportState::RubberBanding && m_didDrag) {
        if (m_activeTool == TOOL_PSELECT)
            m_document.selectPolysInRect(m_rubberA, m_rubberB, additive);
        else
            m_document.selectVerticesInRect(m_rubberA, m_rubberB, additive);
    }

    if (m_state == ViewportState::Sketching && m_didDrag) {
        m_undoStack.push(m_document);
        m_document.addSketchLine(m_rubberA, world);
        m_document.markModified();
        if (m_mainFrame != nullptr) m_mainFrame->UpdateTitle();
    }

    if (m_state == ViewportState::Dragging && !m_didDrag) {
        /* Click without drag: pop the snapshot we took (nothing moved) */
        m_undoStack.pop();
        if (m_activeTool == TOOL_PSELECT)
            m_document.selectPolyAt(world, additive);
        else
            m_document.selectVertexAt(world, WorldTolerance(), additive);
    }

    m_state   = ViewportState::Idle;
    m_didDrag = false;

    if (m_mainFrame != nullptr) {
        m_mainFrame->UpdateStatusBar();
        m_mainFrame->UpdateTitle();
    }
    Refresh(false);
}

void GlViewport::AddCreationVertex(Vec2 worldPos) {
    if (m_state != ViewportState::CreatingPoly)
        m_state = ViewportState::CreatingPoly;

    m_creationVerts[m_creationVertCount++] = worldPos;

    if (m_creationVertCount == kMaxCreationVerts) {
        m_undoStack.push(m_document);
        EditorPoly poly;
        for (int i = 0; i < 3; ++i) {
            poly.v[i].world = m_creationVerts[i];
            poly.v[i].r = poly.v[i].g = poly.v[i].b = 200;
            poly.v[i].alpha = 255;
            poly.v[i].selected = true;
        }
        poly.polyType = POLY_NORMAL;
        m_document.addPoly(poly);
        if (m_mainFrame != nullptr)
            m_mainFrame->UpdateTitle();
        m_creationVertCount = 0;
        m_state = ViewportState::Idle;
    }
    Refresh(false);
}

void GlViewport::CancelCreation() {
    m_creationVertCount = 0;
    m_state = ViewportState::Idle;
}

/* ---- Panning ------------------------------------------------------------ */

bool GlViewport::IsSpacePanGesture(const wxMouseEvent& event) const {
    return event.LeftIsDown() && wxGetKeyState(WXK_SPACE);
}

void GlViewport::BeginPan(const wxPoint& point) {
    m_panning      = true;
    m_lastPanPoint = point;
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
