/*
 * map_document.cpp — MapDocument implementation.
 */
#include "map_document.h"

#include <algorithm>
#include <cstring>
#include <sstream>
#include <cmath>

MapDocument::MapDocument() {
    /* Reserve element [0] in sceneryNames so indices are 1-based */
    sceneryNames.push_back("");
}

/* ---- Screen cache ------------------------------------------------------- */

Vec2 MapDocument::worldToScreen(Vec2 world) const {
    return Vec2((world.x - scrollX) * zoom, (world.y - scrollY) * zoom);
}

Vec2 MapDocument::screenToWorld(Vec2 screen) const {
    return Vec2(screen.x / zoom + scrollX, screen.y / zoom + scrollY);
}

void MapDocument::rebuildPolyScreenCache(EditorPoly& p) const {
    for (int i = 0; i < 3; ++i)
        p.v[i].screen = worldToScreen(p.v[i].world);
}

void MapDocument::rebuildSceneryScreenCache(EditorScenery& s) const {
    Vec2 sc = worldToScreen({s.x, s.y});
    s.screenX = sc.x;
    s.screenY = sc.y;
}

void MapDocument::rebuildScreenCache() {
    for (auto& p : polys)   rebuildPolyScreenCache(p);
    for (auto& s : scenery) rebuildSceneryScreenCache(s);
}

void MapDocument::setZoom(float newZoom, float cx, float cy) {
    /* Keep the world point under screen position (cx,cy) fixed. */
    Vec2 worldUnderCursor = screenToWorld({cx, cy});
    zoom = newZoom;
    if (zoom < 0.03125f) zoom = 0.03125f;
    if (zoom > 512.0f)   zoom = 512.0f;
    scrollX = worldUnderCursor.x - cx / zoom;
    scrollY = worldUnderCursor.y - cy / zoom;
    rebuildScreenCache();
}

/* ---- Selection ---------------------------------------------------------- */

void MapDocument::clearSelection() {
    for (auto& p : polys)
        for (int i = 0; i < 3; ++i)
            p.v[i].selected = false;
    for (auto& s : scenery)     s.selected = false;
    for (auto& sp : spawns)     sp.selected = false;
    for (auto& c : colliders)   c.selected = false;
    for (auto& wp : waypoints)  wp.selected = false;
    for (auto& l : lights)      l.selected = false;
    for (auto& sk : sketch)     sk.selected = false;
}

void MapDocument::selectAll() {
    for (auto& p : polys)
        for (int i = 0; i < 3; ++i)
            p.v[i].selected = true;
    for (auto& s : scenery)     s.selected = true;
    for (auto& sp : spawns)     sp.selected = true;
    for (auto& c : colliders)   c.selected = true;
    for (auto& wp : waypoints)  wp.selected = true;
    for (auto& l : lights)      l.selected = true;
}

void MapDocument::invertSelection() {
    for (auto& p : polys)
        for (int i = 0; i < 3; ++i)
            p.v[i].selected = !p.v[i].selected;
    for (auto& s : scenery)     s.selected = !s.selected;
    for (auto& sp : spawns)     sp.selected = !sp.selected;
    for (auto& c : colliders)   c.selected = !c.selected;
    for (auto& wp : waypoints)  wp.selected = !wp.selected;
    for (auto& l : lights)      l.selected = !l.selected;
}

void MapDocument::selectByColor(uint8_t r, uint8_t g, uint8_t b) {
    for (auto& p : polys)
        for (int i = 0; i < 3; ++i)
            if (p.v[i].r == r && p.v[i].g == g && p.v[i].b == b)
                p.v[i].selected = true;
}

bool MapDocument::anySelected() const {
    for (const auto& p : polys)
        if (p.anySelected()) return true;
    for (const auto& s : scenery)   if (s.selected)  return true;
    for (const auto& sp : spawns)   if (sp.selected) return true;
    for (const auto& c : colliders) if (c.selected)  return true;
    for (const auto& wp : waypoints) if (wp.selected) return true;
    for (const auto& l : lights)    if (l.selected)  return true;
    return false;
}

/* ---- Hit testing -------------------------------------------------------- */

int MapDocument::findNearestVertexIdx(Vec2 worldPos, float tolerance) const {
    float best = tolerance * tolerance;
    int   bestIdx = -1;
    for (int pi = 0; pi < static_cast<int>(polys.size()); ++pi) {
        for (int vi = 0; vi < 3; ++vi) {
            const Vec2& w = polys[pi].v[vi].world;
            float dx = w.x - worldPos.x;
            float dy = w.y - worldPos.y;
            float d2 = dx*dx + dy*dy;
            if (d2 < best) {
                best = d2;
                bestIdx = pi * 3 + vi;
            }
        }
    }
    return bestIdx;
}

int MapDocument::findPolyAt(Vec2 worldPos) const {
    /* Point-in-triangle using sign of cross products (CW winding, Y-down). */
    for (int pi = 0; pi < static_cast<int>(polys.size()); ++pi) {
        const EditorPoly& poly = polys[pi];
        const Vec2 a = poly.v[0].world;
        const Vec2 b = poly.v[1].world;
        const Vec2 c = poly.v[2].world;
        const Vec2 p = worldPos;

        auto cross = [](Vec2 e0, Vec2 e1, Vec2 pt) {
            return (e1.x - e0.x) * (pt.y - e0.y) - (e1.y - e0.y) * (pt.x - e0.x);
        };

        float d1 = cross(a, b, p);
        float d2 = cross(b, c, p);
        float d3 = cross(c, a, p);

        bool hasNeg = (d1 < 0) || (d2 < 0) || (d3 < 0);
        bool hasPos = (d1 > 0) || (d2 > 0) || (d3 > 0);
        if (!(hasNeg && hasPos))
            return pi;
    }
    return -1;
}

bool MapDocument::selectVertexAt(Vec2 worldPos, float tolerance, bool additive) {
    if (!additive) clearSelection();
    int idx = findNearestVertexIdx(worldPos, tolerance);
    if (idx < 0) return false;
    polys[idx / 3].v[idx % 3].selected = true;
    return true;
}

bool MapDocument::selectPolyAt(Vec2 worldPos, bool additive) {
    if (!additive) clearSelection();
    int idx = findPolyAt(worldPos);
    if (idx < 0) return false;
    for (int i = 0; i < 3; ++i)
        polys[idx].v[i].selected = true;
    return true;
}

void MapDocument::selectVerticesInRect(Vec2 worldA, Vec2 worldB, bool additive) {
    if (!additive) clearSelection();
    float x0 = std::min(worldA.x, worldB.x);
    float x1 = std::max(worldA.x, worldB.x);
    float y0 = std::min(worldA.y, worldB.y);
    float y1 = std::max(worldA.y, worldB.y);
    for (auto& p : polys)
        for (int i = 0; i < 3; ++i) {
            const Vec2& w = p.v[i].world;
            if (w.x >= x0 && w.x <= x1 && w.y >= y0 && w.y <= y1)
                p.v[i].selected = true;
        }
}

void MapDocument::selectPolysInRect(Vec2 worldA, Vec2 worldB, bool additive) {
    if (!additive) clearSelection();
    float x0 = std::min(worldA.x, worldB.x);
    float x1 = std::max(worldA.x, worldB.x);
    float y0 = std::min(worldA.y, worldB.y);
    float y1 = std::max(worldA.y, worldB.y);
    for (auto& p : polys) {
        /* Use centroid to decide whether poly is "in" the rect */
        float cx = (p.v[0].world.x + p.v[1].world.x + p.v[2].world.x) / 3.0f;
        float cy = (p.v[0].world.y + p.v[1].world.y + p.v[2].world.y) / 3.0f;
        if (cx >= x0 && cx <= x1 && cy >= y0 && cy <= y1)
            for (int i = 0; i < 3; ++i)
                p.v[i].selected = true;
    }
}

/* ---- Editing ------------------------------------------------------------ */

void MapDocument::deleteSelected() {
    polys.erase(std::remove_if(polys.begin(), polys.end(),
        [](const EditorPoly& p) { return p.allSelected(); }), polys.end());

    /* Partial vertex selection: don't delete the poly, just deselect the
       vertices (matching VB6 "Clear" which deletes whole polys only). */

    scenery.erase(std::remove_if(scenery.begin(), scenery.end(),
        [](const EditorScenery& s) { return s.selected; }), scenery.end());

    spawns.erase(std::remove_if(spawns.begin(), spawns.end(),
        [](const EditorSpawn& s) { return s.selected; }), spawns.end());

    colliders.erase(std::remove_if(colliders.begin(), colliders.end(),
        [](const EditorCollider& c) { return c.selected; }), colliders.end());

    waypoints.erase(std::remove_if(waypoints.begin(), waypoints.end(),
        [](const EditorWaypoint& wp) { return wp.selected; }), waypoints.end());

    lights.erase(std::remove_if(lights.begin(), lights.end(),
        [](const EditorLight& l) { return l.selected; }), lights.end());

    markModified();
}

void MapDocument::duplicateSelected(float offsetX, float offsetY) {
    /* Copy selected polys and scenery with an offset; deselect originals. */
    std::vector<EditorPoly> newPolys;
    for (auto& p : polys) {
        if (!p.anySelected()) continue;
        EditorPoly np = p;
        for (int i = 0; i < 3; ++i) {
            np.v[i].world.x += offsetX;
            np.v[i].world.y += offsetY;
        }
        /* Deselect the original */
        for (int i = 0; i < 3; ++i) p.v[i].selected = false;
        newPolys.push_back(np);
    }

    std::vector<EditorScenery> newScen;
    for (auto& s : scenery) {
        if (!s.selected) continue;
        EditorScenery ns = s;
        ns.x += offsetX;
        ns.y += offsetY;
        s.selected = false;
        newScen.push_back(ns);
    }

    for (auto& p : newPolys) polys.push_back(p);
    for (auto& s : newScen)  scenery.push_back(s);

    rebuildScreenCache();
    markModified();
}

void MapDocument::moveSelected(float dx, float dy) {
    for (auto& p : polys)
        for (int i = 0; i < 3; ++i)
            if (p.v[i].selected) {
                p.v[i].world.x += dx;
                p.v[i].world.y += dy;
            }
    for (auto& s : scenery)    if (s.selected)  { s.x += dx; s.y += dy; }
    for (auto& sp : spawns)    if (sp.selected) { sp.x += dx; sp.y += dy; }
    for (auto& c : colliders)  if (c.selected)  { c.x += dx; c.y += dy; }
    for (auto& wp : waypoints) if (wp.selected) { wp.x += dx; wp.y += dy; }
    for (auto& l : lights)     if (l.selected)  { l.x += dx; l.y += dy; }

    rebuildScreenCache();
    markModified();
}

void MapDocument::nudgeSelectedVertices(float dx, float dy) {
    moveSelected(dx, dy);
}

/* ---- Entity placement -------------------------------------------------- */

void MapDocument::addSpawn(float wx, float wy, int team) {
    EditorSpawn s;
    s.x = wx; s.y = wy; s.team = team; s.active = true; s.selected = true;
    clearSelection();
    spawns.push_back(s);
    markModified();
}

void MapDocument::addCollider(float wx, float wy, float radius) {
    EditorCollider c;
    c.x = wx; c.y = wy; c.radius = radius; c.active = true; c.selected = true;
    clearSelection();
    colliders.push_back(c);
    markModified();
}

void MapDocument::addWaypoint(float wx, float wy) {
    EditorWaypoint w;
    w.x = wx; w.y = wy; w.active = true; w.selected = true;
    w.id = static_cast<int>(waypoints.size()) + 1;
    clearSelection();
    waypoints.push_back(w);
    markModified();
}

void MapDocument::addLight(float wx, float wy,
                            uint8_t r, uint8_t g, uint8_t b,
                            float intensity, int range) {
    EditorLight l;
    l.x = wx; l.y = wy; l.r = r; l.g = g; l.b = b;
    l.intensity = intensity; l.range = range; l.selected = true;
    clearSelection();
    lights.push_back(l);
    markModified();
}

void MapDocument::addSketchLine(Vec2 a, Vec2 b) {
    EditorSketchLine sl;
    sl.a = a; sl.b = b; sl.selected = true;
    sketch.push_back(sl);
    markModified();
}

void MapDocument::addSceneryInstance(int nameIdx, float wx, float wy) {
    if (nameIdx < 1 || nameIdx > static_cast<int>(sceneryNames.size())) return;
    EditorScenery s;
    s.style = nameIdx;
    s.x = wx; s.y = wy;
    s.rotation = 0.0f; s.scaleX = 1.0f; s.scaleY = 1.0f;
    s.alpha = 255; s.selected = true;
    clearSelection();
    scenery.push_back(s);
    rebuildSceneryScreenCache(scenery.back());
    markModified();
}

int MapDocument::addPoly(const EditorPoly& p) {
    int idx = static_cast<int>(polys.size());
    polys.push_back(p);
    rebuildPolyScreenCache(polys.back());
    markModified();
    return idx;
}

bool MapDocument::removePoly(int index) {
    if (index < 0 || index >= static_cast<int>(polys.size())) return false;
    polys.erase(polys.begin() + index);
    markModified();
    return true;
}

/* ---- Undo snapshot (simple: just re-serialize everything) -------------- */

/* Forward declarations from pms_io.h */
#include "pms_io.h"

std::vector<uint8_t> MapDocument::snapshotState() const {
    /* Use PMS native save as the snapshot format — well-tested, complete. */
    std::vector<uint8_t> buf;
    PmsData data;
    docToPmsData(*this, data);
    std::string err;
    if (!pmsDataToBytes(data, buf, err))
        buf.clear();
    return buf;
}

void MapDocument::restoreState(const std::vector<uint8_t>& snap) {
    if (snap.empty()) return;
    PmsData data;
    std::string err;
    if (!pmsBytesToData(snap, data, err)) return;
    pmsDataToDoc(data, *this);
    rebuildScreenCache();
}

/* ---- Clear ------------------------------------------------------------- */

void MapDocument::clear() {
    polys.clear();
    scenery.clear();
    sceneryNames.clear();
    sceneryNames.push_back("");  /* keep [0] as sentinel */
    spawns.clear();
    colliders.clear();
    waypoints.clear();
    lights.clear();
    sketch.clear();
    options = MapOptions{};
    scrollX = scrollY = 0;
    zoom = 1.0f;
    modified = false;
}
