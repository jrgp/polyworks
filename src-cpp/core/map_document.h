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
};

/* ---- MapDocument ------------------------------------------------------- */
class MapDocument {
public:
    MapDocument();

    /* Viewport/camera state */
    float scrollX = 0, scrollY = 0;  /* world coords of viewport origin */
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
    void clearSelection();
    void selectAll();
    void invertSelection();
    void selectByColor(uint8_t r, uint8_t g, uint8_t b);

    bool anySelected() const;

    /* ---- Editing ------------------------------------------------------- */
    void deleteSelected();
    void duplicateSelected(float offsetX, float offsetY);
    void moveSelected(float dx, float dy);

    /* Polygon operations */
    int  addPoly(const EditorPoly& p);
    bool removePoly(int index);

    /* Vertex operations (operate on selected vertices across all polys) */
    void nudgeSelectedVertices(float dx, float dy);

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
};
