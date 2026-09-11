#pragma once
/*
 * interaction.h — the editor's mouse/keyboard state machine.
 *
 * Every gesture the original supports is a transition in one explicit machine,
 * rather than state smeared across a dozen event handlers.  The states are the
 * ones the VB6 code actually has: it tracks `noneSelected`, `creating`,
 * `sketching`, `scaling`/`rotating` and the scroll drag as separate global
 * flags, and the handlers below are the same distinctions given names.
 *
 * The class knows nothing about the windowing system.  It is fed positions
 * already converted to the viewport's coordinate space, plus the modifier
 * state, so the same machine can be driven by GLFW, by a test, or by a replay.
 */

#include "map_document.h"

#include <set>

namespace pw {

class Editor;

/* The tools, in the order of the VB6 TOOL_* constants (frm:1313-1341).  The
   first fourteen are choosable in frmTools; the rest are "virtual" tools that
   a modifier key selects for as long as it is held. */
enum {
    TOOL_MOVE = 0, TOOL_CREATE, TOOL_VSELECT, TOOL_PSELECT, TOOL_VCOLOR,
    TOOL_PCOLOR, TOOL_TEXTURE, TOOL_SCENERY, TOOL_WAYPOINT, TOOL_OBJECTS,
    TOOL_COLORPICK, TOOL_SKETCH, TOOL_LIGHTS, TOOL_DEPTHMAP,
    TOOL_HAND, TOOL_VSELADD, TOOL_VSELSUB, TOOL_PSELADD, TOOL_PSELSUB,
    TOOL_SCALE, TOOL_ROTATE, TOOL_CONNECT, TOOL_QUAD, TOOL_PIXPICKER,
    TOOL_LITPICKER, TOOL_ERASER, TOOL_SMUDGE,
    TOOL_COUNT
};
constexpr int kSelectableTools = 14;

enum class InteractionState {
    Idle,
    Panning,
    RubberBanding,   /* drag-select rectangle in progress */
    Dragging,        /* moving selected objects, or a continuous paint stroke */
    CreatingPoly,    /* placing polygon vertices one click at a time */
    Sketching,
    Transforming,    /* Ctrl-drag scale / Alt-drag rotate about the pivot */
};

struct Modifiers {
    bool shift = false;
    bool ctrl  = false;
    bool alt   = false;
    bool space = false;
};

class Interaction {
public:
    explicit Interaction(Editor& editor) : m_editor(editor) {}

    /* Modifier state is pushed in once per frame rather than read from events:
       the effective tool has to follow a key being pressed or released even
       when the mouse is not moving, which is how the original's DirectInput
       poller behaves (frm:10715). */
    void setModifiers(Modifiers mods);
    const Modifiers& modifiers() const { return m_mods; }

    void setActiveTool(int tool);

    /* All positions are viewport-local logical coordinates. */
    void onLeftDown(Vec2 pos);
    void onLeftUp(Vec2 pos);
    void onRightDown(Vec2 pos);
    void onMiddleDown(Vec2 pos);
    void onMiddleUp(Vec2 pos);
    void onMouseMove(Vec2 pos);
    /* `notches` is already normalised to whole wheel clicks. */
    void onWheel(int notches, Vec2 pos);

    /* Returns true when the key was consumed. */
    bool onEscape();
    bool onTab(bool backwards);

    InteractionState state() const { return m_state; }
    /* The modifier-adjusted tool actually in effect (VB6 currentFunction). */
    int currentFunction() const { return m_currentFunction; }

    /* Overlay geometry the viewport draws: the rubber-band rectangle and the
       vertices of a polygon being created. */
    bool rubberBand(Vec2& a, Vec2& b) const;
    int  pendingVertices(Vec2 out[3]) const;
    bool sketchPreview(Vec2& a, Vec2& b) const;

    void cancelCreation();

private:
    int  computeCurrentFunction() const;
    void refreshCurrentFunction();
    void beginPan(Vec2 pos);
    void updatePan(Vec2 pos);
    void endPan();
    void addCreationVertex(Vec2 world);
    void beginTransformDrag(Vec2 world);
    void updateTransformDrag(Vec2 world);
    float worldTolerance() const;
    MapDocument::SelectMode selectMode() const;

    Editor& m_editor;
    Modifiers m_mods;

    InteractionState m_state = InteractionState::Idle;
    int m_activeTool = 0;
    int m_currentFunction = 0;

    Vec2 m_dragWorldStart{};
    Vec2 m_dragWorldLast{};
    Vec2 m_rubberA{}, m_rubberB{};
    Vec2 m_lastPanPos{};
    bool m_didDrag = false;
    /* VB6 `noneSelected` (frm:1533): the Move tool's own SelNearest pick,
       released again on mouse-up (frm:11734). */
    bool m_moveTransientSel = false;
    bool m_sketchStraight = false;

    MapDocument::TransformSession m_transform;

    static constexpr int kMaxCreationVerts = 3;
    Vec2 m_creationVerts[kMaxCreationVerts];
    int  m_creationVertCount = 0;
    bool m_creatingQuad = false;
    Vec2 m_quadCarryUV[2]{};

    /* Vertices already painted in the current stroke, so that the normal
       colour mode paints each one once (frm:7595). */
    std::set<uint32_t> m_colorStroke;
};

}  // namespace pw
