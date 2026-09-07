#pragma once

#include "map_document.h"

#include <wx/panel.h>
#include <wx/cursor.h>
#include <string>

#if defined(__has_include)
#if __has_include(<wx/glcanvas.h>)
#include <wx/glcanvas.h>
#define PW_HAS_WX_GLCANVAS 1
#else
#define PW_HAS_WX_GLCANVAS 0
#endif

#if __has_include(<GL/gl.h>)
#include <GL/gl.h>
#define PW_HAS_OPENGL_HEADERS 1
#elif __has_include(<OpenGL/gl.h>)
#include <OpenGL/gl.h>
#define PW_HAS_OPENGL_HEADERS 1
#else
#define PW_HAS_OPENGL_HEADERS 0
#endif
#else
#define PW_HAS_WX_GLCANVAS 0
#define PW_HAS_OPENGL_HEADERS 0
#endif

#if PW_HAS_WX_GLCANVAS && PW_HAS_OPENGL_HEADERS
using GlViewportBase = wxGLCanvas;
#include "renderer.h"
#include "texture_manager.h"
#else
using GlViewportBase = wxPanel;
class wxGLContext;
#endif

class MainFrame;
class UndoStack;
class wxMouseEvent;
class wxPaintEvent;
class wxKeyEvent;

/* Mouse interaction states */
enum class ViewportState {
    Idle,
    Panning,
    RubberBanding,   /* drag-select rectangle in progress */
    Dragging,        /* moving selected objects */
    CreatingPoly,    /* placing polygon vertices one by one */
    Sketching,       /* drawing a sketch line by drag */
};

class GlViewport final : public GlViewportBase {
public:
    GlViewport(MainFrame* parent, MapDocument& document, UndoStack& undoStack);
    ~GlViewport() override;

    void setSkinsPath(const std::string& path);
    void addTexturePath(const std::string& path);
    void setActiveTool(int tool);

private:
    /* Event handlers */
    void OnPaint(wxPaintEvent& event);
    void OnMouseMove(wxMouseEvent& event);
    void OnMouseWheel(wxMouseEvent& event);
    void OnLeftDown(wxMouseEvent& event);
    void OnLeftUp(wxMouseEvent& event);
    void OnMiddleDown(wxMouseEvent& event);
    void OnMiddleUp(wxMouseEvent& event);
    void OnRightDown(wxMouseEvent& event);
    void OnKeyDown(wxKeyEvent& event);

    /* Panning helpers */
    bool IsSpacePanGesture(const wxMouseEvent& event) const;
    void BeginPan(const wxPoint& point);
    void UpdatePan(const wxPoint& point);
    void EndPan();

    /* Editing helpers */
    void HandleLeftDownEdit(const wxMouseEvent& event);
    void HandleMouseMoveEdit(const wxMouseEvent& event);
    void HandleLeftUpEdit(const wxMouseEvent& event);
    void CommitDrag();
    void AddCreationVertex(Vec2 worldPos);
    void CancelCreation();
    float WorldTolerance() const;  /* ~5 screen pixels in world space */

    void DrawFallback(wxDC& dc);

    MainFrame*    m_mainFrame  = nullptr;
    MapDocument&  m_document;
    UndoStack&    m_undoStack;
    int           m_activeTool = 0;

    /* Panning state */
    bool          m_panning      = false;
    wxPoint       m_lastPanPoint;

    /* General editing state */
    ViewportState m_state = ViewportState::Idle;
    Vec2          m_dragWorldStart;   /* world pos at drag start */
    Vec2          m_dragWorldLast;    /* world pos at last move event */
    Vec2          m_rubberA, m_rubberB; /* rubber-band rect corners (world) */
    bool          m_didDrag = false;  /* true if mouse moved > threshold */

    /* CREATE tool state */
    static constexpr int kMaxCreationVerts = 3;
    Vec2 m_creationVerts[kMaxCreationVerts];
    int  m_creationVertCount = 0;

    /* Cursor management */
    void loadCursors(const std::string& skinsPath);
    void applyToolCursor();
    /* Index matches tool constants TOOL_MOVE..TOOL_DEPTHMAP (14 tools) */
    static constexpr int kNumTools = 14;
    wxCursor m_toolCursors[kNumTools];

#if PW_HAS_WX_GLCANVAS && PW_HAS_OPENGL_HEADERS
    wxGLContext*  m_glContext   = nullptr;
    Renderer      m_renderer;
    TextureManager m_texMgr;
    bool          m_initialized = false;
#endif
};
