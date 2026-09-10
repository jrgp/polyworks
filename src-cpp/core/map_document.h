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
#include <set>
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

    /*
     * Depth used by the Depthmap tool and by the lighting solver
     * (VB6 `lightDir.Z = Lights(k).Z - Polys(i).vertex(j).Z`, frm:6319).
     * The compiler always writes 1.0 to disk, but the PolyWorks-native save
     * path stores the real value, so it must survive a document round-trip.
     * A negative z additionally means "hidden" (VB6 mnuVisible_Click,
     * frm:13728, which pairs z = -1 with rhw = -10).
     */
    float    z   = 1.0f;
    float    rhw = 1.0f;

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
    float gridSize = 32.0f;  /* modConfig.bas:68 GridSpacing default */

    /* Editing aids (match VB6 globals) */
    bool snapToGrid     = false;  /* snap dragged vertices to grid */
    bool snapToVertices = false;  /* snap to nearby vertices (ohSnap) */
    bool fixedTexture   = false;  /* don't update tu/tv when snapping/moving */
    bool blendWireframe = false;  /* use the configured wireframe blend factors */
    bool blendPolys     = false;  /* use the configured polygon blend factors */

    /* Blend factors chosen in Preferences (frmPreferences cboPolySrc etc.).
       Indices into the original's ZERO/ONE/SRCCOLOR/INVSRCCOLOR/DESTCOLOR/
       INVDESTCOLOR/SRCALPHA/INVSRCALPHA list. */
    /* Grid appearance (frmPreferences Grid tab). Major lines use colour 1 at
       opacity 1; the gridDivisions-1 minor lines between them use colour 2. */
    int          gridDivisions = 4;
    unsigned int gridColor1 = 0xFF000000;
    unsigned int gridColor2 = 0xFF000000;
    float        gridAlpha1 = 1.0f;
    float        gridAlpha2 = 0.2f;

    int polyBlendSrc  = 6;  /* SRCALPHA */
    int polyBlendDest = 7;  /* INVSRCALPHA */
    int wireBlendSrc  = 6;
    int wireBlendDest = 7;
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

    /* ---- Transform reference point (VB6 rCenter, frm:1521) -------------- */
    enum class RCenterMode {
        Fixed,   /* mnuFixedRCenter — bbox centre, marker hidden */
        Set,     /* mnuSetRCenter   — user-placed pivot */
        Center,  /* mnuCenterRCenter — bbox centre, marker shown */
    };
    RCenterMode rCenterMode = RCenterMode::Fixed;
    Vec2        rCenter;

    /* ---- Gostek reference figure (VB6 mnuGostek, frm:14461) ------------- */
    bool showGostek = false;
    Vec2 gostek;

    /* Waypoint currently acting as the source for the Connect tool
       (VB6 currentWaypoint).  -1 when nothing is anchored. */
    int currentWaypoint = -1;

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

    /* Multiplicative wheel zoom, faithful to ZoomScroll
       (frmOpenSoldatMapEditor.frm:4108).  `zoomDir` is a ratio (the original
       uses 1.25 for a wheel notch forward and 0.8 for backward).  Zooming in
       anchors the world point under (cx,cy); zooming out anchors the centre of
       the viewport — that asymmetry is in the original and is preserved.
       Returns false when the request was rejected because the limit was
       already reached. */
    bool zoomScroll(float zoomDir, float cx, float cy,
                    float viewW, float viewH,
                    float minZoom, float maxZoom);

    /* ---- Selection ----------------------------------------------------- */

    /* Controls how a selection operation combines with existing selection. */
    enum class SelectMode {
        Replace,   /* clear existing selection, then add */
        Add,       /* add to existing selection (Shift) */
        Subtract,  /* remove from existing selection (Alt) */
    };

    void clearSelection();

    /* Tab / Shift+Tab cycling (frm:6070 TabPressed).
       With exactly one polygon selected and no scenery: if all three vertices
       are selected the selection moves to the next (or previous) polygon;
       otherwise the vertex selection rotates within the polygon.
       With exactly one scenery item selected and no polygons: the selection
       moves to the next (or previous) scenery item.
       Returns true when something changed. */
    bool cycleSelection(bool backwards);
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

       VertexColoring (frm:7570) also tints scenery in range, and in the
       default colour mode paints each vertex at most once per stroke.  Pass
       a `stroke` set to get that: keys already in it are skipped, and every
       key painted is added.  Passing null repaints on every call, which is
       what the original's "dynamic" mode does.
       Returns true if any vertex was painted. */
    bool applyColorToVerticesNear(Vec2 worldPos, float worldRadius,
                                  uint8_t r, uint8_t g, uint8_t b,
                                  float opacity = 1.0f, int blendMode = 0,
                                  std::set<uint32_t>* stroke = nullptr);

    /* PrecisionColoring (frm:7496): colour only the single closest vertex
       within worldRadius, honouring the selection the same way. */
    bool applyColorToNearestVertex(Vec2 worldPos, float worldRadius,
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
        std::vector<float> sceneryRot; /* parallel to `scenery` */
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

    /* VB6 ClearUnused (frm:4581): drops scenery names that no placed instance
     * references (and duplicate names), renumbering the surviving styles.
     * Returns the number of entries removed. */
    int clearUnusedScenery();

    /* Snaps a single point the way VB6 CreatePoly does before storing a new
     * vertex (frm:7963-7989): grid snapping first, else nearest existing
     * polygon vertex within snapRadius.  Returns true if the point moved. */
    bool snapPoint(Vec2& p, float snapRadius) const;

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

    /*
     * Eraser tool (VB6 EraseSketch, frm:8743).  Deletes the single sketch line
     * whose nearest endpoint lies within `radius` world units of `worldPos`.
     * VB6 removes it by swapping in the last element, so ordering is not
     * preserved — reproduced here because sketch order is user-visible in the
     * saved file.  Returns true if a line was removed.
     */
    bool eraseSketchAt(Vec2 worldPos, float radius);

    /*
     * Smudge tool (VB6 MoveLines, frm:8782).  Drags sketch endpoints within
     * `radius` by (dx,dy), attenuated by cos((d²/r²)·π/2) so the effect falls
     * off to zero at the brush edge.  Returns true if anything moved.
     */
    bool smudgeSketchAt(Vec2 worldPos, float dx, float dy, float radius);

    /*
     * Freehand sketching (VB6 StartSketch / LinkSketch / EndSketch,
     * frm:8082-8175).  beginSketchStroke starts a zero-length line;
     * extendSketchStroke moves its far endpoint and, once the stroke has
     * travelled more than 16 world units from the current line's origin,
     * commits it and starts a new one.
     */
    void beginSketchStroke(Vec2 worldPos);
    bool extendSketchStroke(Vec2 worldPos, bool finish = false);

    /* ---- Depthmap (VB6 EditDepthMap, frm:7662) -------------------------- */
    /*
     * Blends `value` into the z of every vertex within `radius` world units,
     * as `z = z*(1-opacity) + value*opacity`.  Restricted to selected
     * polygons when anything is selected, matching the original.  Returns
     * true if any vertex changed.
     */
    bool applyDepthNear(Vec2 worldPos, float radius, float value, float opacity);

    /* ---- Pickers (VB6 ColorPicker / DepthPicker / LightPicker) ---------- */
    /*
     * Finds the vertex nearest worldPos among polygons that actually contain
     * worldPos, within `radius` world units.  This "must be inside the poly"
     * rule is what makes the pickers feel precise in the original.
     * Returns true and fills polyIdx/vertIdx on success.
     */
    bool pickVertexInPoly(Vec2 worldPos, float radius,
                          int& polyIdx, int& vertIdx) const;

    /* ---- Texture tool (VB6 StretchingTexture, frm:7860) ----------------- */
    /* Offsets tu/tv of every selected vertex.  du/dv are already divided by
       the texture dimensions by the caller. */
    void offsetTextureOnSelected(float du, float dv);

    /* ---- Visibility (VB6 mnuVisible_Click, frm:13728) ------------------- */
    /* Toggles the hidden flag (negative z / rhw) on all selected polygons. */
    void toggleSelectedVisibility();

    /* ---- Waypoint connections (VB6 CreateConnection, frm:7887) ---------- */
    /*
     * Connect tool.  Finds a waypoint within `radius` of worldPos:
     *   - if one is found and a source waypoint is already anchored, adds a
     *     connection from the anchor to it and re-anchors on it;
     *   - if one is found with no anchor, just anchors on it;
     *   - if none is found, clears the anchor and deselects all waypoints.
     * Returns true if a connection was actually created.
     */
    bool connectWaypointAt(Vec2 worldPos, float radius);

    /* ---- Selection bounds ----------------------------------------------- */
    /* Bounding rectangle of the current selection (VB6 selRect).  Returns
       false when nothing is selected. */
    bool selectionBounds(float& minX, float& minY,
                         float& maxX, float& maxY) const;

    /* Recomputes rCenter from the current selection unless the user pinned it
       with "Set Reference Point". */
    void updateRCenter();

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
