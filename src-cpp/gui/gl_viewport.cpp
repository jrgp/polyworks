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
    /* Replicates VB6 frmOpenSoldatMapEditor lines 10715–10793: modifier keys
       dynamically change the effective tool (currentFunction).  Space-pan is
       handled separately as it never reaches this path. */
    if (!shiftDown && !ctrlDown && !altDown)
        return m_activeTool;

    switch (m_activeTool) {
    case TOOL_VSELECT:
        if (shiftDown) return TOOL_VSELADD;
        if (altDown)   return TOOL_VSELSUB;
        break;
    case TOOL_PSELECT:
        if (shiftDown) return TOOL_PSELADD;
        if (altDown)   return TOOL_PSELSUB;
        break;
    case TOOL_MOVE:
        if (ctrlDown) return TOOL_SCALE;
        if (altDown)  return TOOL_ROTATE;
        break;
    case TOOL_WAYPOINT:
        if (shiftDown) return TOOL_CONNECT;
        break;
    case TOOL_SKETCH:
        /* Ctrl+sketch and Alt+sketch have VB6 equivalents but are not
           implemented yet; fall through to default behaviour. */
        break;
    case TOOL_VCOLOR:
    case TOOL_PCOLOR:
    case TOOL_DEPTHMAP:
        if (altDown) return TOOL_COLORPICK;
        break;
    case TOOL_COLORPICK:
        if (altDown) return TOOL_COLORPICK; /* litpicker; keep as colorpick for now */
        break;
    default:
        /* Ctrl on any tool > MOVE → temporary MOVE */
        if (ctrlDown && m_activeTool > TOOL_MOVE) return TOOL_MOVE;
        /* Alt on most other tools → temporary VSELECT */
        if (altDown) return TOOL_VSELECT;
        break;
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
        if (m_state == ViewportState::CreatingPoly) {
            CancelCreation();
        } else {
            m_document.clearSelection();
        }
        Refresh(false);
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
        if (team < 0) {
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
        m_state = ViewportState::Sketching;
        m_rubberA = world;
        m_rubberB = world;
        return;

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
        m_state == ViewportState::Sketching)
        m_rubberB = world;

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

    if (m_state == ViewportState::Sketching && m_didDrag) {
        m_undoStack.push(m_document);
        m_document.addSketchLine(m_rubberA, world);
        m_document.markModified();
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
