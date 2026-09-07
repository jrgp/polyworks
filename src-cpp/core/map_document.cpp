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
