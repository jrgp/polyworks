#pragma once
/*
 * map_document.h — Editor-side document model.
 *
 * This is what the editor works with at runtime.  It is richer than the
 * on-disk PMS format and must never depend on wxWidgets or OpenGL.
 *
 * Coordinate system (matches VB6 original):
 *   World coords: float X/Y, Y increases downward.
 *   Screen coords: screen = (world - scroll) * zoom
 *                  world  = screen / zoom + scroll
 */

#include "pms_types.h"

#include <string>
#include <vector>
#include <array>
#include <functional>
#include <memory>
#include <cstdint>

/* ---- 2-D vector -------------------------------------------------------- */
struct Vec2 {
    float x = 0, y = 0;
    Vec2() = default;
    Vec2(float x_, float y_) : x(x_), y(y_) {}
    Vec2 operator+(Vec2 o) const { return {x+o.x, y+o.y}; }
    Vec2 operator-(Vec2 o) const { return {x-o.x, y-o.y}; }
    Vec2 operator*(float s) const { return {x*s, y*s}; }
    Vec2& operator+=(Vec2 o) { x+=o.x; y+=o.y; return *this; }
    Vec2& operator-=(Vec2 o) { x-=o.x; y-=o.y; return *this; }
};

/* ---- Editor vertex ----------------------------------------------------- */
struct EditorVertex {
    Vec2     world;   /* source of truth */
    Vec2     screen;  /* cache: (world - scroll) * zoom  — rebuilt by RebuildScreenCache() */
    uint8_t  r = 255, g = 255, b = 255;  /* unlit base colour */
    uint8_t  alpha = 255;
    float    tu = 0, tv = 0;
    bool     selected = false;
};

/* ---- Editor polygon ---------------------------------------------------- */
struct EditorPoly {
    EditorVertex v[3];
    uint8_t      polyType = POLY_NORMAL;

    /* Per-edge bounciness (VB6 Polys().Perp.vertex(j).Z).  Only meaningful for
       POLY_BOUNCY, where 1.0 == 0% extra bounce.  VB6 recovers this from the
       stored normal's magnitude on load and bakes it back in on save. */
    float        bounciness[3] = {1.0f, 1.0f, 1.0f};

    /* True if any vertex is selected. */
    bool anySelected() const {
        return v[0].selected || v[1].selected || v[2].selected;
    }
    bool allSelected() const {
        return v[0].selected && v[1].selected && v[2].selected;
    }
};

/* ---- Scenery instance -------------------------------------------------- */
struct EditorScenery {
    int     style = 0;     /* 1-based index into sceneryNames */
    float   x = 0, y = 0;
    float   rotation = 0;
    float   scaleX = 1, scaleY = 1;
    int     width = 0, height = 0;  /* from loaded texture */
    uint8_t alpha = 255;
    int32_t color = -1;    /* ARGB tint; -1 = white (no tint) */
    int     level = SCENERY_MIDDLE;
    bool    selected = false;

    /* Screen position cache */
    float   screenX = 0, screenY = 0;
};

/* ---- Spawn point ------------------------------------------------------- */
struct EditorSpawn {
    float   x = 0, y = 0;
    int     team = SPAWN_GENERAL;
    bool    active = true;
    bool    selected = false;
};

/* ---- Circle collider --------------------------------------------------- */
struct EditorCollider {
    float x = 0, y = 0, radius = 15;
    bool  active = true;
    bool  selected = false;
};

/* ---- Waypoint ---------------------------------------------------------- */
struct EditorWaypoint {
    int     id = 0;
    float   x = 0, y = 0;
    bool    left = false, right = false, up = false, down = false, m2 = false;
    int     pathNum = 0;
    int     special = 0;
    std::vector<int> connections; /* outgoing connection IDs */
    bool    active = true;
    bool    selected = false;
};

/* ---- Light source (PolyWorks extension) -------------------------------- */
struct EditorLight {
    float    x = 0, y = 0, z = 0;
    float    intensity = 1.0f;
    int      range = 100;
    uint8_t  r = 255, g = 255, b = 255;
    bool     selected = false;
};

/* ---- Sketch line (PolyWorks extension) --------------------------------- */
struct EditorSketchLine {
    Vec2     a, b;
    bool     selected = false;
};

/* ---- Map options ------------------------------------------------------- */
struct MapOptions {
    std::string mapName;
    std::string textureName;
    uint32_t    bgColor1 = 0xFF000000;  /* ARGB black */
    uint32_t    bgColor2 = 0xFF000000;
    int32_t     startJet = 0;
    uint8_t     grenadePacks = 0;
    uint8_t     medikits = 0;
    uint8_t     weather = 0;
    uint8_t     steps = 0;
    int32_t     mapRandomID = -1;  /* -1 = PolyWorks native */
};

/* ---- Viewport state ---------------------------------------------------- */
struct ViewSettings {
    bool showPolys      = true;
    bool showWireframe  = false;
    bool showPoints     = true;
    bool showGrid       = false;
    bool showObjects    = true;
    bool showWaypoints  = true;
    bool showLights     = true;
    bool showSketch     = true;
    bool showTexture    = true;
    bool showBackground = true;
    bool showSceneryBack   = true;
    bool showSceneryMiddle = true;
    bool showSceneryFront  = true;
    float gridSize = 25.0f;

    /* Editing aids (match VB6 globals) */
    bool snapToGrid     = false;  /* snap dragged vertices to grid */
    bool snapToVertices = false;  /* snap to nearby vertices (ohSnap) */
    bool fixedTexture   = false;  /* don't update tu/tv when snapping/moving */
    bool blendWireframe = false;  /* alpha-blend wireframe overlay */
    bool blendPolys     = false;  /* alpha-blend polygon fill */
};

/* ---- MapDocument ------------------------------------------------------- */
class MapDocument {
public:
    MapDocument();

    /* Viewport/camera state */
    float scrollX = 0, scrollY = 0;  /* world coords of viewport origin */
    /* User-configurable selection highlight colour, 0xRRGGBB.
       modConfig.bas:79 "SelectionColor", default CE4D4A. */
    uint32_t selectionColor = 0xCE4D4Au;

    float zoom    = 1.0f;
    ViewSettings viewSettings;

    /* Map data */
    MapOptions              options;
    std::vector<EditorPoly>    polys;
    std::vector<EditorScenery> scenery;
    std::vector<std::string>   sceneryNames;  /* 1-based; [0] unused */
    std::vector<EditorSpawn>   spawns;
    std::vector<EditorCollider> colliders;
    std::vector<EditorWaypoint> waypoints;
    std::vector<EditorLight>   lights;
    std::vector<EditorSketchLine> sketch;

    bool modified = false;

    /* ---- Screen cache -------------------------------------------------- */
    /* Rebuild all screen-position caches after zoom/scroll change. */
    void rebuildScreenCache();

    /* Convert between world and screen coordinates. */
    Vec2 worldToScreen(Vec2 world) const;
    Vec2 screenToWorld(Vec2 screen) const;

    /* Apply zoom centred on screen point (cx,cy). */
    void setZoom(float newZoom, float cx, float cy);

    /* ---- Selection ----------------------------------------------------- */

    /* Controls how a selection operation combines with existing selection. */
    enum class SelectMode {
        Replace,   /* clear existing selection, then add */
        Add,       /* add to existing selection (Shift) */
        Subtract,  /* remove from existing selection (Alt) */
    };

    void clearSelection();
    void selectAll();
    void invertSelection();
    void selectByColor(uint8_t r, uint8_t g, uint8_t b);

    bool anySelected() const;

    /* ---- Hit testing --------------------------------------------------- */
    /* Returns flat vertex index (polyIdx * 3 + vertIdx) or -1.
       tolerance is in world-space units. */
    int  findNearestVertexIdx(Vec2 worldPos, float tolerance) const;

    /* Returns poly index or -1 (uses point-in-triangle test). */
    int  findPolyAt(Vec2 worldPos) const;

    /* Selects the vertex nearest to worldPos within tolerance.
       Matches VB6 RegionSelPolys: first tries the poly under the cursor and
       picks its nearest vertex; falls back to global nearest within tolerance.
       Returns true if a vertex was affected. */
    bool selectVertexAt(Vec2 worldPos, float tolerance, SelectMode mode = SelectMode::Replace);

    /* Legacy overload kept for tests that pass a bool. */
    bool selectVertexAt(Vec2 worldPos, float tolerance, bool additive) {
        return selectVertexAt(worldPos, tolerance,
                              additive ? SelectMode::Add : SelectMode::Replace);
    }

    /* Selects the polygon at worldPos; returns true if found. */
    bool selectPolyAt(Vec2 worldPos, SelectMode mode = SelectMode::Replace);

    /* Legacy overload. */
    bool selectPolyAt(Vec2 worldPos, bool additive) {
        return selectPolyAt(worldPos,
                            additive ? SelectMode::Add : SelectMode::Replace);
    }

    /* Rubber-band selects all vertices inside the world-space rect. */
    void selectVerticesInRect(Vec2 worldA, Vec2 worldB, SelectMode mode = SelectMode::Replace);

    /* Legacy overload. */
    void selectVerticesInRect(Vec2 worldA, Vec2 worldB, bool additive) {
        selectVerticesInRect(worldA, worldB,
                             additive ? SelectMode::Add : SelectMode::Replace);
    }

    /* Rubber-band selects all polygons whose centroid is inside the rect. */
    void selectPolysInRect(Vec2 worldA, Vec2 worldB, SelectMode mode = SelectMode::Replace);

    /* Legacy overload. */
    void selectPolysInRect(Vec2 worldA, Vec2 worldB, bool additive) {
        selectPolysInRect(worldA, worldB,
                          additive ? SelectMode::Add : SelectMode::Replace);
    }

    /* ---- Color painting ------------------------------------------------ */

    /* Blend formula matching VB6 ApplyBlend: blendMode 0=Normal, 1=Multiply,
       2=Screen, 3=Darken, 4=Lighten, 5=Difference. */
    static void blendColor(uint8_t& dr, uint8_t& dg, uint8_t& db,
                           uint8_t sr, uint8_t sg, uint8_t sb,
                           float opacity, int blendMode);

    /* Apply color to all selected vertices across all polygons.
       If nothing is selected, does nothing and returns false. */
    bool applyColorToSelected(uint8_t r, uint8_t g, uint8_t b,
                              float opacity = 1.0f, int blendMode = 0);

    /* Apply color to all 3 vertices of the polygon under worldPos.
       If no polygon is hit, returns false. */
    bool applyColorToPolyAt(Vec2 worldPos,
                            uint8_t r, uint8_t g, uint8_t b,
                            float opacity = 1.0f, int blendMode = 0);

    /* Paint color onto vertices within worldRadius of worldPos.
       If any vertices are selected, only paints selected vertices in range.
       If nothing is selected, paints all unselected vertices in range.
       Returns true if any vertex was painted. */
    bool applyColorToVerticesNear(Vec2 worldPos, float worldRadius,
                                  uint8_t r, uint8_t g, uint8_t b,
                                  float opacity = 1.0f, int blendMode = 0);

    /* ---- Editing ------------------------------------------------------- */
    void deleteSelected();
    void duplicateSelected(float offsetX, float offsetY);
    void moveSelected(float dx, float dy);

    /* Clipboard operations */
    void copySelected();
    void pasteSelected();
    bool hasClipboard() const;

    /* Ordering */
    void bringSelectedToFront();
    void bringSelectedForward();
    void sendSelectedBackward();
    void sendSelectedToBack();

    /* Transform (operates on centroid of selection bounding box) */
    void rotateSelected(float angleDeg);
    void flipSelected(bool horizontal, bool vertical);

    /* ---- Interactive transform sessions --------------------------------- */
    /*
     * VB6 performs Ctrl-drag scaling (Scaling()) and Alt-drag rotation
     * (Rotating()) about rCenter — the centre of the selection's bounding
     * rectangle — recomputing absolute positions from the drag origin on every
     * mouse-move.  A session captures the pre-drag geometry so repeated moves
     * do not accumulate rounding drift.
     */
    struct TransformSession {
        Vec2 center;
        std::vector<Vec2> polyVerts;   /* selected poly vertices, in order */
        std::vector<Vec2> scenery;
        std::vector<Vec2> spawns;
        std::vector<Vec2> colliders;
        std::vector<Vec2> waypoints;
        std::vector<Vec2> lights;
        bool empty() const {
            return polyVerts.empty() && scenery.empty() && spawns.empty() &&
                   colliders.empty() && waypoints.empty() && lights.empty();
        }
    };

    /* Centre of the bounding rectangle of everything currently selected. */
    Vec2 selectionCenter() const;

    /* Capture the current positions of all selected items. */
    void beginTransform(TransformSession& s) const;

    /* Re-apply the session's geometry scaled by (sx,sy) then rotated by
       angleRad, both about s.center. */
    void applyTransform(const TransformSession& s, float sx, float sy,
                        float angleRad);

    /* Polygon operations */
    int  addPoly(const EditorPoly& p);
    bool removePoly(int index);

    /* Polygon editing operations (Ctrl+L, Ctrl+J, Ctrl+E) */
    /* Split each selected poly at its selected vertex: produces a new poly sharing
       the selected vertex and the left-hand neighbour; the original poly's left vertex
       moves to the midpoint of the original left-right edge. */
    void splitAtVertex();

    /* Join Vertices: move all selected vertices to the position of the first selected
       vertex (matches VB6 mnuJoinVertices_Click). */
    void joinSelectedVertices();

    /* Create With Selected: collect up to 3 selected vertices from the current
       selection (across all polys, in poly/vertex order) and create a new polygon. */
    void createPolyFromSelected();

    /* Texture coordinate operations (Ctrl+F, Ctrl+U) */
    /* Fix Texture: set tu/tv of each selected vertex to world_x/texW, world_y/texH
       where texW/texH are the polygon's current texture dimensions (use 1.0 if unknown). */
    void fixTextureOnSelected(float texW, float texH);

    /* Untexture: set tu/tv of each selected vertex to 1.0f, 1.0f. */
    void untextureSelected();

    /* Flip texture UVs on selected vertices around their centroid.
       horizontal=true: reflect tu; horizontal=false: reflect tv. */
    void flipTextureOnSelected(bool horizontal);

    /* Rotate texture UVs on selected vertices by angle (radians) around their centroid.
       texAspect = textureWidth / textureHeight (use 1.0 if unknown). */
    void rotateTextureOnSelected(float angle, float texAspect = 1.0f);

    /* Average Vertex Colors (Ctrl+G): for each group of coincident vertices (within 2
       world-units of each other), set all their colors to the average of the group.
       Matches VB6 AverageVertices(). */
    void averageVertexColors();

    /* Vertex operations (operate on selected vertices across all polys) */
    void nudgeSelectedVertices(float dx, float dy);

    /* ---- Snapping (VB6 SnapSelected, frm:8246) -------------------------- */
    /*
     * Applied on mouse-up after a move.  The anchor is the first selected
     * vertex (or the first selected scenery item when no vertex is selected);
     * the whole selection is then shifted by the anchor's snap delta.
     *
     * Grid snapping wins over vertex snapping, matching the original's
     * `If snapToGrid And showGrid ... ElseIf ohSnap ...` structure.
     * Returns true if the selection moved.
     */
    bool snapSelectedToGrid(float gridSize);
    bool snapSelectedToVertices(float snapRadius);

    /* Runs the appropriate snap for the current viewSettings. */
    bool snapSelected(float snapRadius);

    /* Waypoint operations */
    /* Severs connections between selected waypoints.
       If 2+ waypoints are selected: removes connections where BOTH endpoints are selected.
       If 1 waypoint is selected: removes ALL connections to/from that waypoint.
       Matches VB6 mnuSever_Click. */
    void severWaypointConnections();

    /* Sketch operations */
    void clearSketch();

    /* Light operations */
    /* Bakes the light contribution (using VB6-compatible dot-product formula) into the
       base vertex r/g/b values of all (or selected) polygons, then clears the lights array.
       Matches VB6 mnuApplyLight_Click. */
    void applyLightsToBaseColors();

    /* Map bounds — computes bounding box of all polygon vertices.
       Returns false (with all zeros) if there are no polygons. */
    bool mapBounds(float& minX, float& minY, float& maxX, float& maxY) const;

    /* Fit-to-viewport: adjusts zoom and scroll so the entire map is centred and visible
       within a viewport of the given pixel size. Matches VB6 mnuFitOnScreen_Click. */
    void fitToViewport(float viewW, float viewH);

    /* Entity placement */
    void addSpawn(float wx, float wy, int team = SPAWN_GENERAL);
    void addCollider(float wx, float wy, float radius = 15.0f);
    void addWaypoint(float wx, float wy);
    void addLight(float wx, float wy, uint8_t r = 255, uint8_t g = 255,
                  uint8_t b = 255, float intensity = 1.0f, int range = 100);
    void addSketchLine(Vec2 a, Vec2 b);
    /* Add a scenery instance by scenery-name index (1-based) */
    void addSceneryInstance(int nameIdx, float wx, float wy, int level = 1);

    /* Undo snapshot — returns opaque blob describing current state */
    std::vector<uint8_t> snapshotState() const;
    void restoreState(const std::vector<uint8_t>& snap);

    /* ---- Reset --------------------------------------------------------- */
    void clear();

    /* ---- Dirty-flag helpers ------------------------------------------- */
    void markModified() { modified = true; }
    void clearModified() { modified = false; }

private:
    void rebuildPolyScreenCache(EditorPoly& p) const;
    void rebuildSceneryScreenCache(EditorScenery& s) const;

    /* Clipboard for Copy/Paste */
    std::vector<EditorPoly>    m_clipPolys;
    bool                        m_hasClipboard = false;
};
