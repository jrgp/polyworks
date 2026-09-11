#include "interaction.h"

#include "editor.h"

#include <algorithm>
#include <cmath>

namespace pw {
namespace {

/* Movement below this many screen pixels is a click, not a drag. */
constexpr float kDragThreshold = 4.0f;

}  // namespace

void Interaction::setModifiers(Modifiers mods) {
    m_mods = mods;
    refreshCurrentFunction();
}

void Interaction::setActiveTool(int tool) {
    if (m_activeTool == tool) {
        return;
    }
    if (m_state == InteractionState::CreatingPoly) {
        cancelCreation();
    }
    m_state = InteractionState::Idle;
    m_activeTool = tool;
    m_currentFunction = tool;
    refreshCurrentFunction();
}

void Interaction::refreshCurrentFunction() {
    m_currentFunction = computeCurrentFunction();
}

/*
 * Replicates the VB6 DirectInput key poller (frm:10715-10793).  The original
 * evaluates Shift, then Ctrl, then Alt and returns on the first one that is
 * down, so that precedence is reproduced here.  Space-pan never reaches this
 * path; it is a separate gesture.
 */
int Interaction::computeCurrentFunction() const {
    if (m_mods.shift) {
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

    if (m_mods.ctrl) {
        switch (m_activeTool) {
        case TOOL_MOVE:   return TOOL_SCALE;
        case TOOL_SKETCH: return TOOL_SMUDGE;
        default:
            if (m_activeTool > TOOL_MOVE) return TOOL_MOVE;
            return m_activeTool;
        }
    }

    if (m_mods.alt) {
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

float Interaction::worldTolerance() const {
    return 6.0f / m_editor.doc.zoom;
}

MapDocument::SelectMode Interaction::selectMode() const {
    if (m_currentFunction == TOOL_VSELADD || m_currentFunction == TOOL_PSELADD) {
        return MapDocument::SelectMode::Add;
    }
    if (m_currentFunction == TOOL_VSELSUB || m_currentFunction == TOOL_PSELSUB) {
        return MapDocument::SelectMode::Subtract;
    }
    return MapDocument::SelectMode::Replace;
}

/* ---- panning ----------------------------------------------------------- */

void Interaction::beginPan(Vec2 pos) {
    m_state = InteractionState::Panning;
    m_lastPanPos = pos;
}

void Interaction::updatePan(Vec2 pos) {
    MapDocument& doc = m_editor.doc;
    const float dx = pos.x - m_lastPanPos.x;
    const float dy = pos.y - m_lastPanPos.y;
    m_lastPanPos = pos;
    doc.scrollX -= dx / doc.zoom;
    doc.scrollY -= dy / doc.zoom;
    doc.rebuildScreenCache();
}

void Interaction::endPan() {
    m_state = InteractionState::Idle;
}

void Interaction::onMiddleDown(Vec2 pos) {
    beginPan(pos);
}

void Interaction::onMiddleUp(Vec2 /*pos*/) {
    if (m_state == InteractionState::Panning) {
        endPan();
    }
}

/* ---- wheel ------------------------------------------------------------- */

void Interaction::onWheel(int notches, Vec2 pos) {
    if (notches == 0) {
        return;
    }
    /* VB6 frm:12733 MouseHelper_MouseWheel — 1.25 forward, 0.8 backward. */
    const float step  = notches > 0 ? 1.25f : 0.8f;
    const int   count = notches > 0 ? notches : -notches;

    for (int i = 0; i < count; ++i) {
        if (!m_editor.doc.zoomScroll(step, pos.x, pos.y,
                                     m_editor.viewport.width,
                                     m_editor.viewport.height,
                                     m_editor.prefs.minZoom,
                                     m_editor.prefs.maxZoom)) {
            break;
        }
    }
}

/* ---- keys -------------------------------------------------------------- */

bool Interaction::onEscape() {
    /* VB6 frm:10954: Escape cancels a pending creation, quad or waypoint
       anchor if one exists; only otherwise does it clear the selection. */
    if (m_state == InteractionState::CreatingPoly ||
        m_editor.doc.currentWaypoint >= 0) {
        cancelCreation();
    } else {
        m_editor.doc.clearSelection();
    }
    return true;
}

bool Interaction::onTab(bool backwards) {
    /* VB6 frm:10951 -> TabPressed: cycle the single selected polygon, vertex
       or scenery item.  Shift reverses direction. */
    return m_editor.doc.cycleSelection(backwards);
}

void Interaction::cancelCreation() {
    m_creationVertCount = 0;
    m_creatingQuad = false;
    m_editor.doc.currentWaypoint = -1;
    m_state = InteractionState::Idle;
}

/* ---- overlays ---------------------------------------------------------- */

bool Interaction::rubberBand(Vec2& a, Vec2& b) const {
    if (m_state != InteractionState::RubberBanding || !m_didDrag) {
        return false;
    }
    a = m_rubberA;
    b = m_rubberB;
    return true;
}

int Interaction::pendingVertices(Vec2 out[3]) const {
    for (int i = 0; i < m_creationVertCount; ++i) {
        out[i] = m_creationVerts[i];
    }
    return m_creationVertCount;
}

bool Interaction::sketchPreview(Vec2& a, Vec2& b) const {
    if (m_state != InteractionState::Sketching || !m_sketchStraight) {
        return false;
    }
    a = m_rubberA;
    b = m_rubberB;
    return true;
}

/* ---- left button ------------------------------------------------------- */

void Interaction::onLeftDown(Vec2 pos) {
    Editor& ed = m_editor;
    MapDocument& doc = ed.doc;

    if (m_mods.space) {
        beginPan(pos);
        return;
    }

    const Vec2 world = doc.screenToWorld(pos);
    refreshCurrentFunction();

    m_dragWorldStart = world;
    m_dragWorldLast  = world;
    m_didDrag        = false;

    switch (m_currentFunction) {

    case TOOL_CREATE:
    case TOOL_QUAD:
        addCreationVertex(world);
        return;

    case TOOL_VSELECT:
    case TOOL_VSELADD:
    case TOOL_VSELSUB: {
        const MapDocument::SelectMode mode = selectMode();
        if (doc.selectVertexAt(world, worldTolerance(), mode)) {
            m_state = InteractionState::Dragging;
            ed.undo.push(doc);
        } else {
            if (mode == MapDocument::SelectMode::Replace) {
                doc.clearSelection();
            }
            m_state   = InteractionState::RubberBanding;
            m_rubberA = world;
            m_rubberB = world;
        }
        return;
    }

    case TOOL_MOVE: {
        /* VB6 MouseDownMove (frm:6730): an existing selection is dragged
           as-is; only when nothing is selected does SelNearest pick the object
           under the cursor -- and that transient pick is deselected again on
           mouse-up (frm:11734). */
        const MapDocument::SelectMode mode = selectMode();
        bool hit = doc.anySelected();
        if (!hit) {
            hit = doc.selectNearestObject(world, mode);
            m_moveTransientSel = hit;
        }
        if (hit) {
            m_state = InteractionState::Dragging;
            ed.undo.push(doc);
        } else {
            if (mode == MapDocument::SelectMode::Replace) {
                doc.clearSelection();
            }
            m_state   = InteractionState::RubberBanding;
            m_rubberA = world;
            m_rubberB = world;
        }
        return;
    }

    case TOOL_PSELECT:
    case TOOL_PSELADD:
    case TOOL_PSELSUB: {
        const MapDocument::SelectMode mode = selectMode();
        if (doc.selectPolyAt(world, mode)) {
            m_state = InteractionState::Dragging;
            ed.undo.push(doc);
        } else {
            if (mode == MapDocument::SelectMode::Replace) {
                doc.clearSelection();
            }
            m_state   = InteractionState::RubberBanding;
            m_rubberA = world;
            m_rubberB = world;
        }
        return;
    }

    case TOOL_PCOLOR: {
        /* ColorFill (frm:9555): colours every selected vertex, or, with
           nothing selected, every vertex of the polygon under the cursor. */
        ed.undo.push(doc);
        const PaletteState& p = ed.palette;
        if (!doc.applyColorToSelected(p.r, p.g, p.b, p.opacity, p.blendMode)) {
            doc.applyColorToPolyAt(world, p.r, p.g, p.b, p.opacity, p.blendMode);
        }
        return;
    }

    case TOOL_VCOLOR: {
        /* frm:11210 splits on the colour mode: precision colours the single
           closest vertex on the click and does not paint on the drag. */
        ed.undo.push(doc);
        const PaletteState& p = ed.palette;
        const float worldRadius = static_cast<float>(p.radius) / doc.zoom;
        if (p.colorMode == 0) {
            doc.applyColorToNearestVertex(world, worldRadius, p.r, p.g, p.b,
                                          p.opacity, p.blendMode);
        } else {
            m_state = InteractionState::Dragging;
            m_colorStroke.clear();
            doc.applyColorToVerticesNear(world, worldRadius, p.r, p.g, p.b,
                                         p.opacity, p.blendMode,
                                         p.colorMode == 1 ? &m_colorStroke : nullptr);
        }
        return;
    }

    case TOOL_OBJECTS: {
        if (doc.showGostek) {
            /* VB6 frm:11292: while the gostek reference is active, clicking
               just repositions the silhouette; nothing is added to the map. */
            doc.gostek = world;
        } else if (ed.currentSpawnTeam < 0) {
            ed.undo.push(doc);
            doc.addCollider(world.x, world.y);
            doc.markModified();
        } else {
            ed.undo.push(doc);
            doc.addSpawn(world.x, world.y, ed.currentSpawnTeam);
            doc.markModified();
        }
        return;
    }

    case TOOL_LIGHTS:
        ed.undo.push(doc);
        doc.addLight(world.x, world.y);
        doc.markModified();
        return;

    case TOOL_WAYPOINT: {
        /* VB6 frm:11322-11345 stamps the mnuWayType flags onto the new
           waypoint and, when a waypoint is already anchored, chains a
           connection from it. */
        ed.undo.push(doc);
        doc.addWaypoint(world.x, world.y);
        EditorWaypoint& wp = doc.waypoints.back();
        wp.left  = ed.waypointState.type[0];
        wp.right = ed.waypointState.type[1];
        wp.up    = ed.waypointState.type[2];
        wp.down  = ed.waypointState.type[3];
        wp.m2    = ed.waypointState.type[4];
        const int newIdx = static_cast<int>(doc.waypoints.size()) - 1;
        const int prev   = doc.currentWaypoint;
        if (prev >= 0 && prev < newIdx &&
            doc.waypoints[static_cast<size_t>(prev)].connections.size() < 20) {
            doc.waypoints[static_cast<size_t>(prev)].connections.push_back(wp.id);
        }
        doc.currentWaypoint = newIdx;
        doc.markModified();
        return;
    }

    case TOOL_CONNECT:
        /* VB6 CreateConnection (frm:7887): anchor on a waypoint, then link
           subsequent clicks to it. */
        ed.undo.push(doc);
        if (!doc.connectWaypointAt(world, 8.0f / doc.zoom)) {
            ed.undo.pop();
        }
        return;

    case TOOL_COLORPICK:
    case TOOL_LITPICKER:
    case TOOL_PIXPICKER: {
        /* VB6 ColorPicker / LightPicker / DepthPicker (frm:7711-7860).  The
           Depthmap tool's Alt-pick reads the vertex depth instead of a colour;
           every picker samples the vertex nearest the cursor among the
           polygons that contain it. */
        int pi = -1, vi = -1;
        if (doc.pickVertexInPoly(world, 32.0f, pi, vi)) {
            const EditorVertex& v = doc.polys[static_cast<size_t>(pi)].v[vi];
            if (m_activeTool == TOOL_DEPTHMAP) {
                int d = static_cast<int>(v.z);
                d = std::clamp(d, 0, 255);
                const uint8_t grey = static_cast<uint8_t>(d);
                ed.setPaintColorFromPicker(grey, grey, grey);
            } else {
                ed.setPaintColorFromPicker(v.r, v.g, v.b);
            }
        }
        return;
    }

    case TOOL_DEPTHMAP:
        /* VB6 EditDepthMap (frm:7662): paints the red channel of the current
           colour into the vertex depth, with the palette brush radius. */
        ed.undo.push(doc);
        m_state = InteractionState::Dragging;
        doc.applyDepthNear(world,
                           static_cast<float>(ed.palette.radius) / doc.zoom,
                           static_cast<float>(ed.palette.r), ed.palette.opacity);
        return;

    case TOOL_TEXTURE:
        /* VB6 StretchingTexture (frm:7860) slides the UVs of the selected
           vertices as the mouse drags. */
        if (!doc.anySelected()) {
            return;
        }
        ed.undo.push(doc);
        m_state = InteractionState::Dragging;
        return;

    case TOOL_ERASER:
        ed.undo.push(doc);
        m_state = InteractionState::Dragging;
        /* The snapshot is kept even when this first click erased nothing: the
           drag may still erase on a later move. */
        doc.eraseSketchAt(world, static_cast<float>(ed.palette.radius) / doc.zoom);
        return;

    case TOOL_SMUDGE:
        ed.undo.push(doc);
        m_state = InteractionState::Dragging;
        return;

    case TOOL_SCALE:
    case TOOL_ROTATE:
        /* Ctrl-drag scales and Alt-drag rotates the current selection about
           the pivot (VB6 Scaling / Rotating, frm:7170 / frm:7355). */
        if (!doc.anySelected()) {
            return;
        }
        ed.undo.push(doc);
        beginTransformDrag(world);
        return;

    case TOOL_SKETCH:
        /* VB6 frm:11370: unmodified sketching is freehand (StartSketch +
           LinkSketch); Shift anchors a single straight line. */
        ed.undo.push(doc);
        m_state   = InteractionState::Sketching;
        m_rubberA = world;
        m_rubberB = world;
        m_sketchStraight = m_mods.shift;
        if (!m_sketchStraight) {
            doc.beginSketchStroke(world);
        }
        return;

    case TOOL_SCENERY: {
        const int idx = ed.orAddSelectedSceneryIndex();
        if (idx == 0) {
            return;
        }
        ed.undo.push(doc);
        doc.addSceneryInstance(idx, world.x, world.y, ed.sceneryState.level);
        /* frm:2737 writes the source texture's pixel size into the saved prop,
           so a freshly placed sprite needs its metrics filled in for both
           saving and hit-testing. */
        if (!doc.scenery.empty()) {
            EditorScenery& placed = doc.scenery.back();
            if (placed.width == 0 || placed.height == 0) {
                const unsigned int texId =
                    ed.texMgr.loadTexture(doc.sceneryNames[static_cast<size_t>(idx)]);
                int texW = 0, texH = 0;
                ed.texMgr.getSize(texId, texW, texH);
                placed.width  = texW;
                placed.height = texH;
            }
        }
        doc.markModified();
        return;
    }

    default:
        break;
    }
}

/* ---- transform drag ---------------------------------------------------- */

void Interaction::beginTransformDrag(Vec2 world) {
    m_editor.doc.beginTransform(m_transform);
    if (m_transform.empty()) {
        m_state = InteractionState::Idle;
        return;
    }
    m_state          = InteractionState::Transforming;
    m_dragWorldStart = world;
    m_dragWorldLast  = world;
    m_didDrag        = false;
}

void Interaction::updateTransformDrag(Vec2 world) {
    const Vec2 c     = m_transform.center;
    const Vec2 start = m_dragWorldStart;

    if (m_currentFunction == TOOL_ROTATE) {
        /* VB6 Rotating (frm:7374-7404): angle delta between the drag origin
           and the cursor, both measured from the rotation centre.  Shift
           quantises the total rotation to 15-degree steps. */
        const float a0 = std::atan2(start.y - c.y, start.x - c.x);
        const float a1 = std::atan2(world.y - c.y, world.x - c.x);
        float delta = a1 - a0;
        if (m_mods.shift) {
            constexpr float kPi = 3.14159265358979f;
            const float deg = delta * 180.0f / kPi;
            delta = std::floor((deg + 7.5f) / 15.0f) * 15.0f / 180.0f * kPi;
        }
        m_editor.doc.applyTransform(m_transform, 1.0f, 1.0f, delta);
        return;
    }

    /* TOOL_SCALE — VB6 Scaling (frm:7189-7209). */
    float sx = 1.0f, sy = 1.0f;
    if (m_mods.shift) {
        /* Ctrl+Shift: proportional scale on both axes. */
        const float dnx = start.x - c.x;
        const float dny = start.y - c.y;
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
    m_editor.doc.applyTransform(m_transform, sx, sy, 0.0f);
}

/* ---- motion ------------------------------------------------------------ */

void Interaction::onMouseMove(Vec2 pos) {
    Editor& ed = m_editor;
    MapDocument& doc = ed.doc;

    const Vec2 world = doc.screenToWorld(pos);
    ed.lastMouseWorld = world;

    if (m_state == InteractionState::Panning) {
        updatePan(pos);
        ed.lastMouseWorld = doc.screenToWorld(pos);
        return;
    }

    refreshCurrentFunction();

    const float dx = world.x - m_dragWorldStart.x;
    const float dy = world.y - m_dragWorldStart.y;
    if (std::sqrt(dx * dx + dy * dy) * doc.zoom > kDragThreshold) {
        m_didDrag = true;
    }

    if (m_state == InteractionState::Transforming) {
        if (m_didDrag) {
            updateTransformDrag(world);
        }
        m_dragWorldLast = world;
        return;
    }

    if (m_state == InteractionState::Dragging && m_didDrag) {
        const PaletteState& p = ed.palette;
        const float brush = static_cast<float>(p.radius) / doc.zoom;

        if (m_currentFunction == TOOL_VCOLOR) {
            /* In the normal colour mode each vertex is painted once per
               stroke (frm:7595), so repeated passes at a low opacity do not
               build up. */
            doc.applyColorToVerticesNear(world, brush, p.r, p.g, p.b,
                                         p.opacity, p.blendMode,
                                         p.colorMode == 1 ? &m_colorStroke : nullptr);
            m_dragWorldLast = world;
            return;
        }
        if (m_currentFunction == TOOL_DEPTHMAP) {
            doc.applyDepthNear(world, brush, static_cast<float>(p.r), p.opacity);
            m_dragWorldLast = world;
            return;
        }
        if (m_currentFunction == TOOL_ERASER) {
            doc.eraseSketchAt(world, brush);
            m_dragWorldLast = world;
            return;
        }
        if (m_currentFunction == TOOL_SMUDGE) {
            doc.smudgeSketchAt(world, world.x - m_dragWorldLast.x,
                               world.y - m_dragWorldLast.y, brush);
            m_dragWorldLast = world;
            return;
        }
        if (m_currentFunction == TOOL_TEXTURE) {
            /* VB6 StretchingTexture (frm:7869-7871) divides the screen-space
               delta by the zoom *and* the texture dimensions, so the texture
               tracks the cursor 1:1 regardless of zoom. */
            Vec2 target = world;
            if (m_mods.shift) {
                if (std::fabs(world.x - m_dragWorldStart.x) >=
                    std::fabs(world.y - m_dragWorldStart.y)) {
                    target.y = m_dragWorldStart.y;
                } else {
                    target.x = m_dragWorldStart.x;
                }
            }
            int texW = 0, texH = 0;
            ed.selectedTextureSize(texW, texH);
            const float tw = texW > 0 ? static_cast<float>(texW) : 1.0f;
            const float th = texH > 0 ? static_cast<float>(texH) : 1.0f;
            doc.offsetTextureOnSelected((target.x - m_dragWorldLast.x) / tw,
                                        (target.y - m_dragWorldLast.y) / th);
            m_dragWorldLast = target;
            return;
        }

        /* VB6 constrains movement to one axis while Shift is held
           (frm:11469-11474): whichever axis has moved further wins. */
        Vec2 target = world;
        if (m_mods.shift) {
            if (std::fabs(world.x - m_dragWorldStart.x) >=
                std::fabs(world.y - m_dragWorldStart.y)) {
                target.y = m_dragWorldStart.y;
            } else {
                target.x = m_dragWorldStart.x;
            }
        }
        doc.moveSelected(target.x - m_dragWorldLast.x,
                         target.y - m_dragWorldLast.y);
        m_dragWorldLast = target;
        return;
    }

    if (m_state == InteractionState::RubberBanding ||
        m_state == InteractionState::Sketching) {
        m_rubberB = world;
        /* VB6 LinkSketch (frm:8133) commits a segment and starts a fresh one
           every time the cursor gets more than 16 world units from the current
           segment's origin, giving freehand strokes. */
        if (m_state == InteractionState::Sketching && !m_sketchStraight) {
            if (doc.extendSketchStroke(world)) {
                m_rubberA = world;
            }
        }
    }

    m_dragWorldLast = world;
}

/* ---- left up ----------------------------------------------------------- */

void Interaction::onLeftUp(Vec2 pos) {
    Editor& ed = m_editor;
    MapDocument& doc = ed.doc;

    if (m_state == InteractionState::Panning) {
        endPan();
        return;
    }

    const Vec2 world = doc.screenToWorld(pos);
    const MapDocument::SelectMode mode = selectMode();

    const bool isPoly = (m_currentFunction == TOOL_PSELECT ||
                         m_currentFunction == TOOL_PSELADD ||
                         m_currentFunction == TOOL_PSELSUB);

    if (m_state == InteractionState::RubberBanding && m_didDrag) {
        if (isPoly) {
            doc.selectPolysInRect(m_rubberA, m_rubberB, mode);
        } else {
            doc.selectVerticesInRect(m_rubberA, m_rubberB, mode);
        }
    }

    if (m_state == InteractionState::Sketching) {
        /* Straight-line mode commits the whole drag as one segment; freehand
           mode has already committed its segments during the move, so only the
           trailing stub needs flushing. */
        if (m_sketchStraight) {
            if (m_didDrag) {
                doc.addSketchLine(m_rubberA, world);
                doc.markModified();
            } else {
                ed.undo.pop();
            }
        } else {
            doc.extendSketchStroke(world, true);
            doc.markModified();
        }
        m_sketchStraight = false;
    }

    if (m_state == InteractionState::Transforming && !m_didDrag) {
        ed.undo.pop();  /* click without drag: nothing changed */
    }

    const bool paintTool = (m_currentFunction == TOOL_VCOLOR ||
                            m_currentFunction == TOOL_PCOLOR);

    /* VB6 calls SnapSelected on mouse-up after a move (frm:11607). */
    if (m_state == InteractionState::Dragging && m_didDrag && !paintTool) {
        doc.snapSelected(ed.prefs.snapRadius);
    }

    if (m_state == InteractionState::Dragging && !m_didDrag && !paintTool) {
        ed.undo.pop();  /* click without drag: nothing moved */
        if (m_currentFunction == TOOL_MOVE) {
            /* The Move tool already picked in onLeftDown via SelNearest;
               re-picking here would override it with a polygon-only hit. */
        } else if (isPoly) {
            doc.selectPolyAt(world, mode);
        } else {
            doc.selectVertexAt(world, worldTolerance(), mode);
        }
    }

    /* frm:11734: a selection the Move tool made for itself via SelNearest is
       released again as soon as the button comes up. */
    if (m_moveTransientSel) {
        doc.clearSelection();
        m_moveTransientSel = false;
    }

    m_state   = InteractionState::Idle;
    m_didDrag = false;
    refreshCurrentFunction();
}

void Interaction::onRightDown(Vec2 /*pos*/) {
    /* The context menus themselves are ImGui popups raised by the viewport;
       nothing about the interaction state changes here.  Kept so that the
       state machine remains the single place that knows a button was pressed. */
}

/* ---- polygon creation -------------------------------------------------- */

void Interaction::addCreationVertex(Vec2 worldPos) {
    Editor& ed = m_editor;
    MapDocument& doc = ed.doc;

    if (m_state != InteractionState::CreatingPoly) {
        m_state = InteractionState::CreatingPoly;
    }

    /* VB6 CreatePoly (frm:7955) snaps the new vertex either to the grid or to
       a nearby existing vertex before storing it. */
    doc.snapPoint(worldPos, ed.prefs.snapRadius);

    m_creationVerts[m_creationVertCount++] = worldPos;
    if (m_creationVertCount < kMaxCreationVerts) {
        return;
    }

    ed.undo.push(doc);
    int texW = 0, texH = 0;
    ed.selectedTextureSize(texW, texH);
    const float tw = texW > 0 ? static_cast<float>(texW) : 1.0f;
    const float th = texH > 0 ? static_cast<float>(texH) : 1.0f;

    const PaletteState& p = ed.palette;
    EditorPoly poly;
    for (int i = 0; i < 3; ++i) {
        poly.v[i].world = m_creationVerts[i];
        /* VB6 colours new vertices with the current palette colour and
           opacity (frm:7993-7997). */
        poly.v[i].r = p.r;
        poly.v[i].g = p.g;
        poly.v[i].b = p.b;
        poly.v[i].alpha = static_cast<uint8_t>(255.0f * p.opacity + 0.5f);
        poly.v[i].tu = m_creationVerts[i].x / tw;
        poly.v[i].tv = m_creationVerts[i].y / th;
        poly.v[i].selected = true;
    }

    /* VB6 frm:7999-8020: in Textured Quad mode with User Defined X/Y enabled
       the UVs come from the rectangle selected in the Texture window instead
       of from world coordinates, so the quad shows exactly that patch. */
    if (m_currentFunction == TOOL_QUAD && ed.textureWindow.hasSelection) {
        const TextureWindowState& t = ed.textureWindow;
        if (ed.customTexX) {
            if (m_creatingQuad) {
                poly.v[2].tu = t.u1;
            } else {
                poly.v[0].tu = t.u1;
                poly.v[1].tu = t.u2;
                poly.v[2].tu = t.u2;
            }
        }
        if (ed.customTexY) {
            if (m_creatingQuad) {
                poly.v[2].tv = t.v2;
            } else {
                poly.v[0].tv = t.v1;
                poly.v[1].tv = t.v1;
                poly.v[2].tv = t.v2;
            }
        }
    }
    if (m_creatingQuad) {
        /* The second triangle reuses vertices 1 and 3 of the first, so their
           UVs must be carried over verbatim (frm:8058-8066). */
        poly.v[0].tu = m_quadCarryUV[0].x; poly.v[0].tv = m_quadCarryUV[0].y;
        poly.v[1].tu = m_quadCarryUV[1].x; poly.v[1].tv = m_quadCarryUV[1].y;
    }

    poly.polyType = ed.creationPolyType;

    /* VB6 forces clockwise winding after the third click (frm:8032). */
    const Vec2& a = poly.v[0].world;
    const Vec2& b = poly.v[1].world;
    const Vec2& c = poly.v[2].world;
    if ((b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x) < 0.0f) {
        std::swap(poly.v[1], poly.v[2]);
    }

    doc.addPoly(poly);
    doc.markModified();

    if (m_currentFunction == TOOL_QUAD && !m_creatingQuad) {
        /* VB6 frm:8054: quad mode immediately starts a second triangle seeded
           with vertices 1 and 3 of the one just finished, so the fourth click
           closes the quad. */
        m_creationVerts[0] = poly.v[0].world;
        m_creationVerts[1] = poly.v[2].world;
        m_quadCarryUV[0] = {poly.v[0].tu, poly.v[0].tv};
        m_quadCarryUV[1] = {poly.v[2].tu, poly.v[2].tv};
        m_creationVertCount = 2;
        m_creatingQuad = true;
    } else {
        m_creatingQuad = false;
        m_creationVertCount = 0;
        m_state = InteractionState::Idle;
    }
}

}  // namespace pw
