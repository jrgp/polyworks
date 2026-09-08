#pragma once

#include "map_document.h"

#include <wx/panel.h>
#include <wx/cursor.h>
#include <string>
#include <utility>

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
    Transforming,    /* Ctrl-drag scale / Alt-drag rotate about the selection */
};

class GlViewport final : public GlViewportBase {
public:
    GlViewport(MainFrame* parent, MapDocument& document, UndoStack& undoStack);
    ~GlViewport() override;

    void setSkinsPath(const std::string& path);
    void addTexturePath(const std::string& path);
    void setActiveTool(int tool);

    /* Paint color used by VCOLOR / PCOLOR tools */
    void setPaintColor(uint8_t r, uint8_t g, uint8_t b,
                       float opacity = 1.0f, int blendMode = 0,
                       float radius = 8.0f);

    /* Radius (world units) used by vertex snapping on drag release. */
    void setSnapRadius(float r) { m_snapRadius = r; }

    /* Wheel-zoom limits (gMinZoom / gMaxZoom in modConfig.bas). */
    void setZoomLimits(float minZoom, float maxZoom) {
        m_minZoom = minZoom; m_maxZoom = maxZoom;
    }

    TextureManager& GetTextureManager() { return m_texMgr; }

    /* The modifier-adjusted tool actually in effect (VB6 currentFunction). */
    int GetCurrentFunction() const { return m_currentFunction; }

    /* Returns the (width, height) in pixels of the texture used by the first
       selected polygon in doc. Returns {0, 0} if not found. */
    std::pair<int,int> getTextureSize(const MapDocument& doc);

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
    void OnKeyUp(wxKeyEvent& event);

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

    /* Compute the effective tool from m_activeTool + held modifier keys.
       Matches the VB6 currentFunction mapping exactly. */
    int ComputeCurrentFunction(bool shiftDown, bool ctrlDown, bool altDown) const;

    void DrawFallback(wxDC& dc);

    MainFrame*    m_mainFrame  = nullptr;
    MapDocument&  m_document;
    UndoStack&    m_undoStack;
    int           m_activeTool     = 0;
    int           m_currentFunction = 0;   /* effective tool (modifier-adjusted) */
    bool          m_spaceDown      = false; /* track space for pan */

    /* Panning state */
    bool          m_panning      = false;
    wxPoint       m_lastPanPoint;

    /* General editing state */
    ViewportState m_state = ViewportState::Idle;
    Vec2          m_dragWorldStart;   /* world pos at drag start */
    Vec2          m_dragWorldLast;    /* world pos at last move event */
    Vec2          m_rubberA, m_rubberB; /* rubber-band rect corners (world) */
    bool          m_didDrag = false;  /* true if mouse moved > threshold */

    /* Ctrl-drag scale / Alt-drag rotate session (VB6 Scaling / Rotating) */
    MapDocument::TransformSession m_transform;
    float         m_snapRadius = 8.0f;
    float         m_minZoom = PMS_ZOOM_MIN;
    float         m_maxZoom = PMS_ZOOM_MAX;
    /* Accumulated raw wheel rotation below one full notch (HiDPI trackpads). */
    int           m_wheelAccum = 0;

    void BeginTransformDrag(Vec2 world);
    void UpdateTransformDrag(Vec2 world, bool shiftDown);

    /* CREATE tool state */
    static constexpr int kMaxCreationVerts = 3;
    Vec2 m_creationVerts[kMaxCreationVerts];
    int  m_creationVertCount = 0;
    /* True while a Shift-anchored straight sketch line is being dragged. */
    bool m_sketchStraight = false;
    /* True between the two halves of a Textured Quad creation. */
    bool m_creatingQuad = false;
    Vec2 m_quadCarryUV[2]{};

    /* Paint color state (used by VCOLOR / PCOLOR) */
    uint8_t m_paintR = 255, m_paintG = 255, m_paintB = 255;
    float   m_paintOpacity  = 1.0f;
    int     m_paintBlendMode = 0;
    float   m_paintRadius   = 8.0f;

    /* Cursor management */
    void loadCursors(const std::string& skinsPath);
    void applyToolCursor();
    /* Index matches the VB6 TOOL_* constants, TOOL_MOVE..TOOL_SMUDGE. */
    static constexpr int kNumTools = 27;
    wxCursor m_toolCursors[kNumTools];

#if PW_HAS_WX_GLCANVAS && PW_HAS_OPENGL_HEADERS
    wxGLContext*  m_glContext   = nullptr;
    Renderer      m_renderer;
    TextureManager m_texMgr;
    bool          m_initialized = false;
#endif
};
