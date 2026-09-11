/*
 * map_document.cpp — MapDocument implementation.
 */
#include "map_document.h"
#include "geometry.h"

#include <algorithm>
#include <cstring>
#include <sstream>
#include <cmath>
#include <limits>

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

bool MapDocument::zoomScroll(float zoomDir, float cx, float cy,
                             float viewW, float viewH,
                             float minZoom, float maxZoom) {
    /* VB6 frm:4113-4119.  A step that would overshoot a limit is shortened so
       that it lands exactly on the limit, but only when the limit has not
       already been reached; otherwise the whole gesture is discarded. */
    if (zoom * zoomDir < minZoom && zoom > minZoom)
        zoomDir = minZoom / zoom;
    else if (zoom * zoomDir > maxZoom && zoom < maxZoom)
        zoomDir = maxZoom / zoom;

    if (zoom * zoomDir < minZoom || zoom * zoomDir > maxZoom)
        return false;

    zoom = zoom * zoomDir;

    /* VB6 frm:4129-4135.  Algebraically these are exact anchored zooms:
       zoom-in keeps the world point under the cursor fixed, zoom-out keeps the
       world point at the viewport centre fixed. */
    if (zoomDir > 1.0f) {
        scrollX += cx / zoom * (zoomDir - 1.0f);
        scrollY += cy / zoom * (zoomDir - 1.0f);
    } else if (zoomDir < 1.0f) {
        scrollX -= viewW / zoom * (1.0f - zoomDir) * 0.5f;
        scrollY -= viewH / zoom * (1.0f - zoomDir) * 0.5f;
    }

    rebuildScreenCache();
    return true;
}

/* ---- Selection ---------------------------------------------------------- */

bool MapDocument::cycleSelection(bool backwards) {
    int selPoly = -1, polyCount = 0;
    for (size_t i = 0; i < polys.size(); ++i) {
        if (polys[i].anySelected()) { selPoly = static_cast<int>(i); ++polyCount; }
    }
    int selScen = -1, scenCount = 0;
    for (size_t i = 0; i < scenery.size(); ++i) {
        if (scenery[i].selected) { selScen = static_cast<int>(i); ++scenCount; }
    }

    if (polyCount == 1 && scenCount == 0) {
        EditorPoly& p = polys[static_cast<size_t>(selPoly)];
        if (p.allSelected()) {
            p.v[0].selected = p.v[1].selected = p.v[2].selected = false;
            const int n = static_cast<int>(polys.size());
            int next = backwards ? (selPoly == 0 ? n - 1 : selPoly - 1)
                                 : (selPoly == n - 1 ? 0 : selPoly + 1);
            EditorPoly& q = polys[static_cast<size_t>(next)];
            q.v[0].selected = q.v[1].selected = q.v[2].selected = true;
        } else {
            /* Rotate the per-vertex selection flags 1 -> 2 -> 3 -> 1. */
            const bool tmp = p.v[0].selected;
            p.v[0].selected = p.v[1].selected;
            p.v[1].selected = p.v[2].selected;
            p.v[2].selected = tmp;
        }
        return true;
    }

    if (scenCount == 1 && polyCount == 0) {
        scenery[static_cast<size_t>(selScen)].selected = false;
        const int n = static_cast<int>(scenery.size());
        int next = backwards ? (selScen == 0 ? n - 1 : selScen - 1)
                             : (selScen == n - 1 ? 0 : selScen + 1);
        scenery[static_cast<size_t>(next)].selected = true;
        return true;
    }

    return false;
}

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

/* Point-in-triangle using sign of cross products (winding-agnostic, Y-down). */
static bool PointInTri(Vec2 p, Vec2 a, Vec2 b, Vec2 c) {
    auto cross = [](Vec2 e0, Vec2 e1, Vec2 pt) {
        return (e1.x - e0.x) * (pt.y - e0.y) - (e1.y - e0.y) * (pt.x - e0.x);
    };
    const float d1 = cross(a, b, p);
    const float d2 = cross(b, c, p);
    const float d3 = cross(c, a, p);
    const bool hasNeg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    const bool hasPos = (d1 > 0) || (d2 > 0) || (d3 > 0);
    return !(hasNeg && hasPos);
}

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
    for (int pi = 0; pi < static_cast<int>(polys.size()); ++pi) {
        const EditorPoly& poly = polys[pi];
        if (PointInTri(worldPos, poly.v[0].world, poly.v[1].world, poly.v[2].world))
            return pi;
    }
    return -1;
}

bool MapDocument::selectVertexAt(Vec2 worldPos, float tolerance, SelectMode mode) {
    if (mode == SelectMode::Replace) clearSelection();

    /* VB6 RegionSelPolys: prefer the poly that contains the click, then pick
       its nearest vertex.  Fall back to the globally nearest vertex within
       tolerance when no poly contains the click. */
    int pi = findPolyAt(worldPos);
    int bestFlat = -1;
    if (pi >= 0) {
        /* frm:8539 caps the region pick at 64 world units, so clicking the
           middle of a large polygon selects nothing rather than yanking a
           distant vertex. */
        float best = 64.0f * 64.0f;
        for (int vi = 0; vi < 3; ++vi) {
            const Vec2& w = polys[pi].v[vi].world;
            float dx = w.x - worldPos.x;
            float dy = w.y - worldPos.y;
            float d2 = dx*dx + dy*dy;
            if (d2 < best) { best = d2; bestFlat = pi * 3 + vi; }
        }
    } else {
        bestFlat = findNearestVertexIdx(worldPos, tolerance);
    }

    if (bestFlat < 0) return false;

    bool& sel = polys[bestFlat / 3].v[bestFlat % 3].selected;
    if (mode == SelectMode::Subtract)
        sel = false;
    else
        sel = true;
    return true;
}

bool MapDocument::pointInScenery(const EditorScenery& s, Vec2 worldPos) const {
    const float w = static_cast<float>(s.width)  * s.scaleX;
    const float h = static_cast<float>(s.height) * s.scaleY;
    if (w == 0.0f || h == 0.0f) return false;

    const float dx = worldPos.x - s.x;
    const float dy = worldPos.y - s.y;
    const float c  = std::cos(s.rotation);
    const float sn = std::sin(s.rotation);
    /* frm:9538: local = Rot(+rotation) * (point - anchor) */
    const float lx = dx * c - dy * sn;
    const float ly = dx * sn + dy * c;

    const float x0 = std::min(0.0f, w), x1 = std::max(0.0f, w);
    const float y0 = std::min(0.0f, h), y1 = std::max(0.0f, h);
    return lx >= x0 && lx <= x1 && ly >= y0 && ly <= y1;
}

bool MapDocument::selectNearestObject(Vec2 worldPos, SelectMode mode) {
    if (mode == SelectMode::Replace) clearSelection();
    const bool  value = (mode != SelectMode::Subtract);
    const float z     = (zoom > 0.0f) ? zoom : 1.0f;
    const float tolVertex = 8.0f  / z;
    const float tolRegion = 64.0f / z;

    bool hit = false;

    /* --- Polygon vertices (frm:6763) ---------------------------------- */
    if (viewSettings.showPolys) {
        for (auto& p : polys) {
            for (int j = 0; j < 3; ++j) {
                const Vec2& w = p.v[j].world;
                if (nearCoord(worldPos.x, w.x, tolVertex) &&
                    nearCoord(worldPos.y, w.y, tolVertex)) {
                    p.v[j].selected = value;
                    hit = true;
                }
            }
        }

        /* frm:6777: only when no vertex was grabbed directly, fall back to the
           nearest vertex (within 64px) of a polygon containing the click. */
        if (!hit) {
            float best = tolRegion * tolRegion + 1.0f;
            int bestPoly = -1, bestVert = -1;
            for (size_t i = 0; i < polys.size(); ++i) {
                if (!PointInTri(worldPos, polys[i].v[0].world, polys[i].v[1].world,
                                polys[i].v[2].world))
                    continue;
                for (int j = 0; j < 3; ++j) {
                    const Vec2& w = polys[i].v[j].world;
                    if (!nearCoord(worldPos.x, w.x, tolRegion) ||
                        !nearCoord(worldPos.y, w.y, tolRegion))
                        continue;
                    const float dx = w.x - worldPos.x, dy = w.y - worldPos.y;
                    const float d2 = dx * dx + dy * dy;
                    if (d2 < best) { best = d2; bestPoly = static_cast<int>(i); bestVert = j; }
                }
            }
            if (bestPoly >= 0) {
                polys[static_cast<size_t>(bestPoly)].v[bestVert].selected = value;
                hit = true;
            }
        }
    }

    /* --- Scenery (frm:6800) ------------------------------------------- */
    if (!hit && viewSettings.showScenery) {
        for (auto& s : scenery) {
            if (pointInScenery(s, worldPos)) { s.selected = value; hit = true; break; }
        }
    }

    /* --- Spawns (frm:6810) -------------------------------------------- */
    if (!hit && viewSettings.showObjects) {
        float best = tolVertex * tolVertex + 1.0f;
        int   pick = -1;
        for (size_t i = 0; i < spawns.size(); ++i) {
            if (!nearCoord(worldPos.x, spawns[i].x, tolVertex) ||
                !nearCoord(worldPos.y, spawns[i].y, tolVertex))
                continue;
            const float dx = spawns[i].x - worldPos.x, dy = spawns[i].y - worldPos.y;
            const float d2 = dx * dx + dy * dy;
            if (d2 < best) { best = d2; pick = static_cast<int>(i); }
        }
        if (pick >= 0) { spawns[static_cast<size_t>(pick)].selected = value; hit = true; }
    }

    /* --- Colliders (frm:6830): tolerance is the collider's own radius/2 - */
    if (!hit && viewSettings.showObjects) {
        float best = tolRegion * tolRegion + 1.0f;
        int   pick = -1;
        for (size_t i = 0; i < colliders.size(); ++i) {
            const float r = colliders[i].radius / 2.0f;
            if (!nearCoord(worldPos.x, colliders[i].x, r) ||
                !nearCoord(worldPos.y, colliders[i].y, r))
                continue;
            const float dx = colliders[i].x - worldPos.x, dy = colliders[i].y - worldPos.y;
            const float d2 = dx * dx + dy * dy;
            if (d2 < best) { best = d2; pick = static_cast<int>(i); }
        }
        if (pick >= 0) { colliders[static_cast<size_t>(pick)].selected = value; hit = true; }
    }

    /* --- Waypoints (frm:6849) ----------------------------------------- */
    if (!hit && viewSettings.showWaypoints) {
        float best = tolVertex * tolVertex + 1.0f;
        int   pick = -1;
        for (size_t i = 0; i < waypoints.size(); ++i) {
            /* A path hidden by frmWaypoints' "Show:" group cannot be picked
               (frm:8717), so clicking where an invisible waypoint happens to
               be does not silently select it. */
            if (!viewSettings.waypointPathVisible(waypoints[i].pathNum))
                continue;
            if (!nearCoord(worldPos.x, waypoints[i].x, tolVertex) ||
                !nearCoord(worldPos.y, waypoints[i].y, tolVertex))
                continue;
            const float dx = waypoints[i].x - worldPos.x, dy = waypoints[i].y - worldPos.y;
            const float d2 = dx * dx + dy * dy;
            if (d2 < best) { best = d2; pick = static_cast<int>(i); }
        }
        if (pick >= 0) { waypoints[static_cast<size_t>(pick)].selected = value; hit = true; }
    }

    return hit;
}

bool MapDocument::selectPolyAt(Vec2 worldPos, SelectMode mode) {
    if (mode == SelectMode::Replace) clearSelection();
    int idx = findPolyAt(worldPos);
    if (idx < 0) return false;
    for (int i = 0; i < 3; ++i)
        polys[idx].v[i].selected = (mode != SelectMode::Subtract);
    return true;
}

void MapDocument::selectVerticesInRect(Vec2 worldA, Vec2 worldB, SelectMode mode) {
    if (mode == SelectMode::Replace) clearSelection();
    float x0 = std::min(worldA.x, worldB.x);
    float x1 = std::max(worldA.x, worldB.x);
    float y0 = std::min(worldA.y, worldB.y);
    float y1 = std::max(worldA.y, worldB.y);
    const bool value = (mode != SelectMode::Subtract);
    auto inRect = [&](float x, float y) {
        return x >= x0 && x <= x1 && y >= y0 && y <= y1;
    };

    /* VB6 VertexSel (frm:8840) runs one pass per object class, each gated on
       the matching display flag; hidden classes are never picked. */
    if (viewSettings.showPolys || viewSettings.showWireframe || viewSettings.showPoints) {
        for (auto& p : polys)
            for (int i = 0; i < 3; ++i)
                if (inRect(p.v[i].world.x, p.v[i].world.y)) p.v[i].selected = value;
    }

    /* VertexSelScenery (frm:8990) tests the sprite's anchor corner; the other
       three corners only count when the SceneryVerts preference is on. */
    if (viewSettings.showScenery) {
        for (auto& s : scenery) {
            bool in = inRect(s.x, s.y);
            if (!in && viewSettings.sceneryVerts) {
                const float w = static_cast<float>(s.width)  * s.scaleX;
                const float h = static_cast<float>(s.height) * s.scaleY;
                const float c = std::cos(s.rotation), sn = std::sin(s.rotation);
                const Vec2 corner[3] = {
                    { s.x + c * w,          s.y - sn * w },
                    { s.x + sn * h,         s.y + c * h  },
                    { s.x + c * w + sn * h, s.y - sn * w + c * h },
                };
                for (const auto& cv : corner) if (inRect(cv.x, cv.y)) { in = true; break; }
            }
            if (in) s.selected = value;
        }
    }

    /* VertexSelObjects (frm:9046) */
    if (viewSettings.showObjects) {
        for (auto& s : spawns)    if (inRect(s.x, s.y)) s.selected = value;
        for (auto& c : colliders) if (inRect(c.x, c.y)) c.selected = value;
    }

    /* VertexSelWaypoints (frm:9118), which honours the path filter too
       (frm:9126). */
    if (viewSettings.showWaypoints)
        for (auto& w : waypoints)
            if (viewSettings.waypointPathVisible(w.pathNum) && inRect(w.x, w.y))
                w.selected = value;

    /* VertexSelLights (frm:9091) */
    if (viewSettings.showLights)
        for (auto& l : lights) if (inRect(l.x, l.y)) l.selected = value;
}

void MapDocument::selectPolysInRect(Vec2 worldA, Vec2 worldB, SelectMode mode) {
    if (mode == SelectMode::Replace) clearSelection();
    float x0 = std::min(worldA.x, worldB.x);
    float x1 = std::max(worldA.x, worldB.x);
    float y0 = std::min(worldA.y, worldB.y);
    float y1 = std::max(worldA.y, worldB.y);
    for (auto& p : polys) {
        /* Use centroid to decide whether poly is "in" the rect */
        float cx = (p.v[0].world.x + p.v[1].world.x + p.v[2].world.x) / 3.0f;
        float cy = (p.v[0].world.y + p.v[1].world.y + p.v[2].world.y) / 3.0f;
        if (cx >= x0 && cx <= x1 && cy >= y0 && cy <= y1) {
            bool val = (mode != SelectMode::Subtract);
            for (int i = 0; i < 3; ++i)
                p.v[i].selected = val;
        }
    }
}

/* ---- Color painting ----------------------------------------------------- */

/*static*/
void MapDocument::blendColor(uint8_t& dr, uint8_t& dg, uint8_t& db,
                              uint8_t sr, uint8_t sg, uint8_t sb,
                              float opacity, int blendMode) {
    auto blend = [&](uint8_t d, uint8_t s) -> uint8_t {
        float fd = d / 255.0f;
        float fs = s / 255.0f;
        float result;
        switch (blendMode) {
        case 0: result = fs * opacity + fd * (1.0f - opacity); break;           /* normal */
        case 1: result = (fd * fs) * opacity + fd * (1.0f - opacity); break;   /* multiply */
        case 2: result = (fd - fd*fs + fs) * opacity + fd * (1.0f - opacity); break; /* screen */
        case 3: result = std::min(fd, fs) * opacity + fd * (1.0f - opacity); break;  /* darken */
        case 4: result = std::max(fd, fs) * opacity + fd * (1.0f - opacity); break;  /* lighten */
        case 5: result = std::abs(fd - fs) * opacity + fd * (1.0f - opacity); break; /* difference */
        default: result = 0; break;
        }
        float clamped = result < 0.0f ? 0.0f : (result > 1.0f ? 1.0f : result);
        return static_cast<uint8_t>(clamped * 255.0f + 0.5f);
    };
    dr = blend(dr, sr);
    dg = blend(dg, sg);
    db = blend(db, sb);
}

bool MapDocument::applyColorToSelected(uint8_t r, uint8_t g, uint8_t b,
                                        float opacity, int blendMode) {
    bool applied = false;
    for (auto& p : polys)
        for (int i = 0; i < 3; ++i)
            if (p.v[i].selected) {
                blendColor(p.v[i].r, p.v[i].g, p.v[i].b, r, g, b, opacity, blendMode);
                applied = true;
            }
    if (applied) markModified();
    return applied;
}

bool MapDocument::applyColorToPolyAt(Vec2 worldPos,
                                      uint8_t r, uint8_t g, uint8_t b,
                                      float opacity, int blendMode) {
    int pi = findPolyAt(worldPos);
    if (pi < 0) return false;
    for (int i = 0; i < 3; ++i)
        blendColor(polys[pi].v[i].r, polys[pi].v[i].g, polys[pi].v[i].b,
                   r, g, b, opacity, blendMode);
    markModified();
    return true;
}

bool MapDocument::applyColorToVerticesNear(Vec2 worldPos, float worldRadius,
                                            uint8_t r, uint8_t g, uint8_t b,
                                            float opacity, int blendMode,
                                            std::set<uint32_t>* stroke) {
    float r2 = worldRadius * worldRadius;
    bool hasSelection = anySelected();
    bool applied = false;

    /* Stroke keys: polygon vertices are (polyIndex << 2) | vertex, scenery is
       tagged with the high bit so the two cannot collide. */
    auto firstTouch = [stroke](uint32_t key) {
        if (stroke == nullptr) return true;
        return stroke->insert(key).second;
    };

    for (std::size_t pi = 0; pi < polys.size(); ++pi) {
        auto& p = polys[pi];
        for (int i = 0; i < 3; ++i) {
            /* Respect selection: if anything is selected, only paint selected
               vertices; otherwise paint unselected vertices (VB6 behavior). */
            if (hasSelection && !p.v[i].selected) continue;
            if (!hasSelection && p.v[i].selected) continue;
            const Vec2& w = p.v[i].world;
            float dx = w.x - worldPos.x;
            float dy = w.y - worldPos.y;
            if (dx*dx + dy*dy <= r2) {
                if (!firstTouch(static_cast<uint32_t>(pi) << 2 |
                                static_cast<uint32_t>(i))) continue;
                blendColor(p.v[i].r, p.v[i].g, p.v[i].b, r, g, b, opacity, blendMode);
                applied = true;
            }
        }
    }

    /* VertexColoring tints scenery in range too (frm:7624-7652), gated on
       showScenery, with the same selected/unselected split as the vertices
       above. */
    const bool hasSceneryeSel = std::any_of(scenery.begin(), scenery.end(),
        [](const EditorScenery& s) { return s.selected; });
    for (std::size_t si = 0; viewSettings.showScenery && si < scenery.size(); ++si) {
        auto& s = scenery[si];
        if (hasSceneryeSel && !s.selected) continue;
        if (!hasSceneryeSel && s.selected) continue;
        float dx = s.x - worldPos.x;
        float dy = s.y - worldPos.y;
        if (dx*dx + dy*dy > r2) continue;
        if (!firstTouch(0x80000000u | static_cast<uint32_t>(si))) continue;

        const uint32_t argb = static_cast<uint32_t>(s.color);
        uint8_t dr = static_cast<uint8_t>((argb >> 16) & 0xFF);
        uint8_t dg = static_cast<uint8_t>((argb >> 8) & 0xFF);
        uint8_t db = static_cast<uint8_t>(argb & 0xFF);
        blendColor(dr, dg, db, r, g, b, opacity, blendMode);
        s.color = static_cast<int32_t>((argb & 0xFF000000u) |
                                       (static_cast<uint32_t>(dr) << 16) |
                                       (static_cast<uint32_t>(dg) << 8) | db);
        applied = true;
    }

    if (applied) markModified();
    return applied;
}

bool MapDocument::applyColorToNearestVertex(Vec2 worldPos, float worldRadius,
                                             uint8_t r, uint8_t g, uint8_t b,
                                             float opacity, int blendMode) {
    const float r2 = worldRadius * worldRadius;
    const bool hasSelection = anySelected();
    float best = r2;
    EditorVertex* target = nullptr;

    for (auto& p : polys)
        for (int i = 0; i < 3; ++i) {
            if (hasSelection && !p.v[i].selected) continue;
            if (!hasSelection && p.v[i].selected) continue;
            const Vec2& w = p.v[i].world;
            const float dx = w.x - worldPos.x;
            const float dy = w.y - worldPos.y;
            const float d2 = dx*dx + dy*dy;
            if (d2 <= best) {
                best = d2;
                target = &p.v[i];
            }
        }

    if (target == nullptr) return false;
    blendColor(target->r, target->g, target->b, r, g, b, opacity, blendMode);
    markModified();
    return true;
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
    /* mnuFixedTexture (frm:7106, frm:8325): the texture stays put in world
       space while the polygon slides over it.  VB6 recomputes the UV from the
       vertex's new world position plus the offset it had before the move,
       which reduces exactly to advancing the UV by the world delta measured
       in texture pixels. */
    const bool fixTex = viewSettings.fixedTexture && textureW > 0 && textureH > 0;
    const float du = fixTex ? dx / static_cast<float>(textureW) : 0.0f;
    const float dv = fixTex ? dy / static_cast<float>(textureH) : 0.0f;

    for (auto& p : polys)
        for (int i = 0; i < 3; ++i)
            if (p.v[i].selected) {
                p.v[i].world.x += dx;
                p.v[i].world.y += dy;
                if (fixTex) {
                    p.v[i].tu += du;
                    p.v[i].tv += dv;
                }
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

/* ---- Snapping ----------------------------------------------------------- */

namespace {
/* Returns the anchor position for a snap operation, or false if nothing is
   selected.  VB6 uses the first selected vertex of the first selected poly,
   falling back to the first selected scenery item (frm:8283-8308). */
bool snapAnchor(const MapDocument& doc, Vec2& out, int& polyHits) {
    polyHits = 0;
    bool found = false;
    for (const auto& p : doc.polys)
        for (int i = 0; i < 3; ++i)
            if (p.v[i].selected) {
                if (!found) { out = p.v[i].world; found = true; }
                ++polyHits;
            }
    if (found) return true;
    for (const auto& s : doc.scenery)
        if (s.selected) { out = {s.x, s.y}; return true; }
    return false;
}
} /* namespace */

int MapDocument::clearUnusedScenery() {
    const int count = static_cast<int>(sceneryNames.size()) - 1;
    if (count <= 0) return 0;

    /* A style survives if some placed instance uses it and its name has not
       already been kept under an earlier index (VB6 also collapses duplicate
       names, frm:4600). */
    std::vector<bool> keep(static_cast<size_t>(count) + 1, false);
    for (int i = 1; i <= count; ++i) {
        bool used = false;
        for (const auto& s : scenery) {
            if (s.style == i) { used = true; break; }
        }
        if (!used) continue;
        bool duplicate = false;
        for (int j = 1; j < i; ++j) {
            if (keep[static_cast<size_t>(j)] &&
                sceneryNames[static_cast<size_t>(j)] ==
                    sceneryNames[static_cast<size_t>(i)]) {
                duplicate = true;
                break;
            }
        }
        keep[static_cast<size_t>(i)] = !duplicate;
    }

    std::vector<int> remap(static_cast<size_t>(count) + 1, 0);
    std::vector<std::string> kept;
    kept.push_back(sceneryNames[0]);
    for (int i = 1; i <= count; ++i) {
        if (keep[static_cast<size_t>(i)]) {
            kept.push_back(sceneryNames[static_cast<size_t>(i)]);
            remap[static_cast<size_t>(i)] = static_cast<int>(kept.size()) - 1;
        } else {
            /* Duplicates fold onto the surviving entry with the same name. */
            for (int j = 1; j < i; ++j) {
                if (keep[static_cast<size_t>(j)] &&
                    sceneryNames[static_cast<size_t>(j)] ==
                        sceneryNames[static_cast<size_t>(i)]) {
                    remap[static_cast<size_t>(i)] = remap[static_cast<size_t>(j)];
                    break;
                }
            }
        }
    }

    const int removed = count - (static_cast<int>(kept.size()) - 1);
    if (removed <= 0) return 0;

    for (auto& s : scenery) {
        if (s.style >= 1 && s.style <= count)
            s.style = remap[static_cast<size_t>(s.style)];
    }
    sceneryNames = std::move(kept);
    markModified();
    return removed;
}

bool MapDocument::snapPoint(Vec2& p, float snapRadius) const {
    if (viewSettings.snapToGrid && viewSettings.showGrid &&
        viewSettings.gridSize > 0.0f) {
        p.x = snapToGrid(p.x, viewSettings.gridSize);
        p.y = snapToGrid(p.y, viewSettings.gridSize);
        return true;
    }
    if (!viewSettings.snapToVertices || snapRadius <= 0.0f) return false;
    float best  = snapRadius * snapRadius;
    bool  found = false;
    Vec2  hit{};
    for (const auto& poly : polys) {
        for (int j = 0; j < 3; ++j) {
            const Vec2& w = poly.v[j].world;
            const float dx = w.x - p.x, dy = w.y - p.y;
            const float d2 = dx * dx + dy * dy;
            if (d2 < best) { best = d2; hit = w; found = true; }
        }
    }
    if (found) p = hit;
    return found;
}

bool MapDocument::snapSelectedToGrid(float gridSize) {
    if (gridSize <= 0) return false;
    Vec2 anchor;
    int polyHits = 0;
    if (!snapAnchor(*this, anchor, polyHits)) return false;

    float tx = snapToGrid(anchor.x, gridSize);
    float ty = snapToGrid(anchor.y, gridSize);
    float dx = tx - anchor.x;
    float dy = ty - anchor.y;
    if (dx == 0.0f && dy == 0.0f) return false;

    moveSelected(dx, dy);
    return true;
}

bool MapDocument::snapSelectedToVertices(float snapRadius) {
    if (snapRadius <= 0) return false;
    Vec2 anchor;
    int polyHits = 0;
    if (!snapAnchor(*this, anchor, polyHits)) return false;

    /* VB6 refuses to vertex-snap when the selected vertices do not all share
       the anchor's coordinates (frm:8378-8391) — otherwise the snap would
       collapse unrelated vertices onto one point. */
    for (const auto& p : polys)
        for (int i = 0; i < 3; ++i)
            if (p.v[i].selected &&
                (p.v[i].world.x != anchor.x || p.v[i].world.y != anchor.y))
                return false;

    const float r2 = snapRadius * snapRadius;
    float best = r2 + 1.0f;
    Vec2 target = anchor;
    for (const auto& p : polys)
        for (int i = 0; i < 3; ++i) {
            if (p.v[i].selected) continue;  /* never snap to the selection */
            float dx = p.v[i].world.x - anchor.x;
            float dy = p.v[i].world.y - anchor.y;
            float d2 = dx * dx + dy * dy;
            if (d2 <= r2 && d2 <= best) { best = d2; target = p.v[i].world; }
        }

    if (target.x == anchor.x && target.y == anchor.y) return false;
    moveSelected(target.x - anchor.x, target.y - anchor.y);
    return true;
}

bool MapDocument::snapSelected(float snapRadius) {
    if (viewSettings.snapToGrid && viewSettings.showGrid)
        return snapSelectedToGrid(viewSettings.gridSize);
    if (viewSettings.snapToVertices)
        return snapSelectedToVertices(snapRadius);
    return false;
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

void MapDocument::addSceneryInstance(int nameIdx, float wx, float wy, int level) {
    if (nameIdx < 1 || nameIdx > static_cast<int>(sceneryNames.size())) return;
    EditorScenery s;
    s.style = nameIdx;
    s.x = wx; s.y = wy;
    s.rotation = 0.0f; s.scaleX = 1.0f; s.scaleY = 1.0f;
    s.alpha = 255; s.level = level; s.selected = true;
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

/* ---- Polygon editing operations --------------------------------------- */

void MapDocument::splitAtVertex() {
    /* Matches VB6 mnuSplit_Click:
       For each selected poly with exactly one selected vertex at index j,
       create a new poly from (j, left, midpoint(left,right));
       the original poly's left vertex moves to that midpoint.
       "left" and "right" are the two other vertices in vertex-index order. */
    const int n = static_cast<int>(polys.size());
    std::vector<EditorPoly> newPolys;

    for (int i = 0; i < n; ++i) {
        EditorPoly& orig = polys[i];
        if (!orig.anySelected()) continue;

        for (int j = 0; j < 3; ++j) {
            if (!orig.v[j].selected) continue;

            const int left  = (j + 1) % 3;
            const int right = (j + 2) % 3;

            /* Midpoint of the left–right edge */
            Vec2 mid;
            mid.x = (orig.v[left].world.x + orig.v[right].world.x) * 0.5f;
            mid.y = (orig.v[left].world.y + orig.v[right].world.y) * 0.5f;

            /* New polygon: vertices j, left, mid */
            EditorPoly np{};
            np.polyType = orig.polyType;

            np.v[0] = orig.v[j];
            np.v[1] = orig.v[left];

            /* Build the midpoint vertex */
            EditorVertex mv{};
            mv.world = mid;
            mv.screen = worldToScreen(mid);
            mv.r     = static_cast<uint8_t>((orig.v[left].r     + orig.v[right].r)     / 2);
            mv.g     = static_cast<uint8_t>((orig.v[left].g     + orig.v[right].g)     / 2);
            mv.b     = static_cast<uint8_t>((orig.v[left].b     + orig.v[right].b)     / 2);
            mv.alpha = static_cast<uint8_t>((orig.v[left].alpha + orig.v[right].alpha) / 2);
            mv.tu    = (orig.v[left].tu + orig.v[right].tu) * 0.5f;
            mv.tv    = (orig.v[left].tv + orig.v[right].tv) * 0.5f;
            np.v[2]  = mv;

            /* Move the original poly's left vertex to the midpoint */
            orig.v[left].world  = mid;
            orig.v[left].screen = worldToScreen(mid);
            orig.v[left].r     = mv.r;     orig.v[left].g     = mv.g;
            orig.v[left].b     = mv.b;     orig.v[left].alpha = mv.alpha;
            orig.v[left].tu    = mv.tu;    orig.v[left].tv    = mv.tv;

            newPolys.push_back(np);
            break;  /* one selected vertex per poly */
        }
    }

    for (auto& np : newPolys) {
        /* Ensure CW winding */
        const Vec2 ab = {np.v[1].world.x - np.v[0].world.x, np.v[1].world.y - np.v[0].world.y};
        const Vec2 ac = {np.v[2].world.x - np.v[0].world.x, np.v[2].world.y - np.v[0].world.y};
        if (ab.x * ac.y - ab.y * ac.x < 0.0f)  /* CCW → swap v1/v2 */
            std::swap(np.v[1], np.v[2]);
        polys.push_back(np);
    }

    rebuildScreenCache();
    markModified();
}

void MapDocument::joinSelectedVertices() {
    /* Matches VB6 mnuJoinVertices_Click:
       Move all selected vertices to the position of the first selected vertex. */
    Vec2 target{};
    bool found = false;

    for (const auto& p : polys) {
        if (!p.anySelected()) continue;
        for (int j = 0; j < 3; ++j) {
            if (p.v[j].selected) {
                target = p.v[j].world;
                found = true;
                break;
            }
        }
        if (found) break;
    }
    if (!found) return;

    for (auto& p : polys) {
        if (!p.anySelected()) continue;
        for (int j = 0; j < 3; ++j) {
            if (p.v[j].selected) {
                p.v[j].world  = target;
                p.v[j].screen = worldToScreen(target);
            }
        }
    }
    markModified();
}

void MapDocument::createPolyFromSelected() {
    /* Matches VB6 mnuCreate_Click:
       Collect up to 3 selected vertices (in poly/vertex iteration order),
       create a new polygon, enforce CW winding. */
    EditorPoly np{};
    int count = 0;

    for (const auto& p : polys) {
        if (!p.anySelected()) continue;
        for (int j = 0; j < 3 && count < 3; ++j) {
            if (p.v[j].selected) {
                np.v[count] = p.v[j];
                np.polyType  = p.polyType;
                ++count;
            }
        }
        if (count == 3) break;
    }

    if (count < 3) return;

    /* Ensure CW winding */
    const Vec2 ab = {np.v[1].world.x - np.v[0].world.x, np.v[1].world.y - np.v[0].world.y};
    const Vec2 ac = {np.v[2].world.x - np.v[0].world.x, np.v[2].world.y - np.v[0].world.y};
    if (ab.x * ac.y - ab.y * ac.x < 0.0f)
        std::swap(np.v[1], np.v[2]);

    addPoly(np);
}

void MapDocument::fixTextureOnSelected(float texW, float texH) {
    /* Matches VB6 mnuFixTexture_Click:
       Set tu = world_x / texW, tv = world_y / texH for each selected vertex. */
    if (texW <= 0.0f) texW = 1.0f;
    if (texH <= 0.0f) texH = 1.0f;

    for (auto& p : polys) {
        if (!p.anySelected()) continue;
        for (int j = 0; j < 3; ++j) {
            if (p.v[j].selected) {
                p.v[j].tu = p.v[j].world.x / texW;
                p.v[j].tv = p.v[j].world.y / texH;
            }
        }
    }
    markModified();
}

void MapDocument::untextureSelected() {
    /* Matches VB6 mnuUntexture_Click: set tu=1, tv=1 for each selected vertex. */
    for (auto& p : polys) {
        if (!p.anySelected()) continue;
        for (int j = 0; j < 3; ++j) {
            if (p.v[j].selected) {
                p.v[j].tu = 1.0f;
                p.v[j].tv = 1.0f;
            }
        }
    }
    markModified();
}

void MapDocument::averageVertexColors() {
    /* Matches VB6 AverageVertices():
       For each group of coincident vertices (within 2 world-units of each other),
       average their R/G/B and write the result back. Alpha is preserved.
       If selection is empty, operates on all vertices; otherwise only operates
       on selected vertices. */
    const bool hasSelection = anySelected();
    const float SNAP = 2.0f;

    /* Build a flat list of (polyIdx, vertIdx) pairs to operate on */
    struct VRef { int pi, vi; };
    std::vector<VRef> verts;
    for (int i = 0; i < static_cast<int>(polys.size()); ++i) {
        for (int j = 0; j < 3; ++j) {
            if (!hasSelection || polys[i].v[j].selected)
                verts.push_back({i, j});
        }
    }

    /* Track which have been averaged to avoid double-counting */
    std::vector<bool> done(verts.size(), false);

    for (size_t k = 0; k < verts.size(); ++k) {
        if (done[k]) continue;

        const Vec2 pos = polys[verts[k].pi].v[verts[k].vi].world;
        /* Find all verts at same position */
        std::vector<size_t> group;
        group.push_back(k);
        for (size_t m = k + 1; m < verts.size(); ++m) {
            if (done[m]) continue;
            const Vec2 p2 = polys[verts[m].pi].v[verts[m].vi].world;
            const float dx = std::abs(p2.x - pos.x);
            const float dy = std::abs(p2.y - pos.y);
            if (dx <= SNAP && dy <= SNAP)
                group.push_back(m);
        }

        if (group.size() <= 1) continue;  /* nothing to average */

        /* Compute average */
        int sumR = 0, sumG = 0, sumB = 0;
        for (size_t idx : group) {
            auto& v = polys[verts[idx].pi].v[verts[idx].vi];
            sumR += v.r; sumG += v.g; sumB += v.b;
        }
        const int n = static_cast<int>(group.size());
        const uint8_t avgR = static_cast<uint8_t>(sumR / n);
        const uint8_t avgG = static_cast<uint8_t>(sumG / n);
        const uint8_t avgB = static_cast<uint8_t>(sumB / n);

        for (size_t idx : group) {
            auto& v = polys[verts[idx].pi].v[verts[idx].vi];
            v.r = avgR; v.g = avgG; v.b = avgB;
            done[idx] = true;
        }
    }
    markModified();
}



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

/* ---- Clipboard --------------------------------------------------------- */

void MapDocument::copySelected() {
    m_clipPolys.clear();
    for (const auto& p : polys) {
        if (p.anySelected())
            m_clipPolys.push_back(p);
    }
    m_hasClipboard = !m_clipPolys.empty();
}

void MapDocument::pasteSelected() {
    if (!m_hasClipboard) return;
    clearSelection();
    for (auto p : m_clipPolys) {
        /* offset pasted polys slightly so they are visible */
        for (int i = 0; i < 3; ++i) {
            p.v[i].world.x += 10.0f;
            p.v[i].world.y += 10.0f;
            p.v[i].selected = true;
        }
        addPoly(p);
    }
    rebuildScreenCache();
    markModified();
}

bool MapDocument::hasClipboard() const {
    return m_hasClipboard;
}

/* ---- Ordering ---------------------------------------------------------- */

void MapDocument::bringSelectedToFront() {
    std::stable_partition(polys.begin(), polys.end(),
        [](const EditorPoly& p){ return !p.anySelected(); });
    rebuildScreenCache();
    markModified();
}

void MapDocument::bringSelectedForward() {
    /* Move each selected poly one position toward end */
    for (int i = static_cast<int>(polys.size()) - 2; i >= 0; --i) {
        if (polys[i].anySelected() && !polys[i + 1].anySelected()) {
            std::swap(polys[i], polys[i + 1]);
        }
    }
    markModified();
}

void MapDocument::sendSelectedBackward() {
    for (int i = 1; i < static_cast<int>(polys.size()); ++i) {
        if (polys[i].anySelected() && !polys[i - 1].anySelected()) {
            std::swap(polys[i], polys[i - 1]);
        }
    }
    markModified();
}

void MapDocument::sendSelectedToBack() {
    std::stable_partition(polys.begin(), polys.end(),
        [](const EditorPoly& p){ return p.anySelected(); });
    rebuildScreenCache();
    markModified();
}

/* ---- Transform --------------------------------------------------------- */

static Vec2 SelectionCenter(const std::vector<EditorPoly>& polys) {
    float minX = 1e30f, minY = 1e30f, maxX = -1e30f, maxY = -1e30f;
    bool any = false;
    for (const auto& p : polys) {
        for (int i = 0; i < 3; ++i) {
            if (p.v[i].selected) {
                minX = std::min(minX, p.v[i].world.x);
                minY = std::min(minY, p.v[i].world.y);
                maxX = std::max(maxX, p.v[i].world.x);
                maxY = std::max(maxY, p.v[i].world.y);
                any = true;
            }
        }
    }
    if (!any) return Vec2{0, 0};
    return Vec2{(minX + maxX) * 0.5f, (minY + maxY) * 0.5f};
}

void MapDocument::rotateSelected(float angleDeg) {
    /* VB6 ApplyRotation (frm:3913-4020) rotates every selected entity kind —
       polygon vertices, scenery, spawns, colliders, waypoints and lights —
       about rCenter, so route this through the same session machinery the
       interactive Alt-drag uses instead of touching polygons only. */
    TransformSession s;
    beginTransform(s);
    if (s.empty()) return;
    applyTransform(s, 1.0f, 1.0f, angleDeg * (3.14159265358979f / 180.0f));
}

/* ---- Interactive transform sessions ------------------------------------ */

Vec2 MapDocument::selectionCenter() const {
    bool any = false;
    float minX = 0, minY = 0, maxX = 0, maxY = 0;
    auto acc = [&](float x, float y) {
        if (!any) { minX = maxX = x; minY = maxY = y; any = true; return; }
        if (x < minX) minX = x;
        if (x > maxX) maxX = x;
        if (y < minY) minY = y;
        if (y > maxY) maxY = y;
    };
    for (const auto& p : polys)
        for (int i = 0; i < 3; ++i)
            if (p.v[i].selected) acc(p.v[i].world.x, p.v[i].world.y);
    for (const auto& s : scenery)   if (s.selected)  acc(s.x, s.y);
    for (const auto& s : spawns)    if (s.selected)  acc(s.x, s.y);
    for (const auto& c : colliders) if (c.selected)  acc(c.x, c.y);
    for (const auto& w : waypoints) if (w.selected)  acc(w.x, w.y);
    for (const auto& l : lights)    if (l.selected)  acc(l.x, l.y);

    if (!any) return {0, 0};
    return {(minX + maxX) * 0.5f, (minY + maxY) * 0.5f};
}

void MapDocument::beginTransform(TransformSession& s) const {
    s = TransformSession{};
    /* VB6 recomputes rCenter from selRect on every transform unless the user
       pinned it with "Set Reference Point" (mnuSetRCenter, frm:12310). */
    s.center = (rCenterMode == RCenterMode::Set) ? rCenter : selectionCenter();
    for (const auto& p : polys)
        for (int i = 0; i < 3; ++i)
            if (p.v[i].selected) s.polyVerts.push_back(p.v[i].world);
    for (const auto& e : scenery)
        if (e.selected) { s.scenery.push_back({e.x, e.y}); s.sceneryRot.push_back(e.rotation); }
    for (const auto& e : spawns)    if (e.selected) s.spawns.push_back({e.x, e.y});
    for (const auto& e : colliders) if (e.selected) s.colliders.push_back({e.x, e.y});
    for (const auto& e : waypoints) if (e.selected) s.waypoints.push_back({e.x, e.y});
    for (const auto& e : lights)    if (e.selected) s.lights.push_back({e.x, e.y});
}

void MapDocument::applyTransform(const TransformSession& s, float sx, float sy,
                                 float angleRad) {
    const float c = std::cos(angleRad);
    const float sn = std::sin(angleRad);

    auto xform = [&](Vec2 orig) -> Vec2 {
        float dx = (orig.x - s.center.x) * sx;
        float dy = (orig.y - s.center.y) * sy;
        return { s.center.x + dx * c - dy * sn,
                 s.center.y + dx * sn + dy * c };
    };

    size_t k = 0;
    for (auto& p : polys)
        for (int i = 0; i < 3; ++i)
            if (p.v[i].selected && k < s.polyVerts.size())
                p.v[i].world = xform(s.polyVerts[k++]);

    k = 0;
    for (auto& e : scenery)
        if (e.selected && k < s.scenery.size()) {
            Vec2 v = xform(s.scenery[k]);
            e.x = v.x; e.y = v.y;
            /* VB6 ApplyRotation frm:3975-3979: a rotated scenery sprite also
               spins about its own origin, and a handedness-flipping scale
               mirrors that spin. */
            float rot = s.sceneryRot[k] - angleRad;
            if (sx * sy < 0.0f) rot = -rot;
            e.rotation = rot;
            ++k;
        }
    k = 0;
    for (auto& e : spawns)
        if (e.selected && k < s.spawns.size()) {
            Vec2 v = xform(s.spawns[k++]); e.x = v.x; e.y = v.y;
        }
    k = 0;
    for (auto& e : colliders)
        if (e.selected && k < s.colliders.size()) {
            Vec2 v = xform(s.colliders[k++]); e.x = v.x; e.y = v.y;
        }
    k = 0;
    for (auto& e : waypoints)
        if (e.selected && k < s.waypoints.size()) {
            Vec2 v = xform(s.waypoints[k++]); e.x = v.x; e.y = v.y;
        }
    k = 0;
    for (auto& e : lights)
        if (e.selected && k < s.lights.size()) {
            Vec2 v = xform(s.lights[k++]); e.x = v.x; e.y = v.y;
        }

    rebuildScreenCache();
    markModified();
}

void MapDocument::flipSelected(bool horizontal, bool vertical) {
    /* VB6 mnuFlip_Click drives the same ApplyScale path used by Ctrl-drag with
       a factor of -1 on the flipped axis, so every selected entity kind moves,
       not just polygon vertices. */
    TransformSession s;
    beginTransform(s);
    if (s.empty()) return;
    applyTransform(s, horizontal ? -1.0f : 1.0f, vertical ? -1.0f : 1.0f, 0.0f);
}

/* ---- Texture UV transforms ---------------------------------------------- */

void MapDocument::flipTextureOnSelected(bool horizontal) {
    /* Matches VB6 mnuFlipTexture_Click:
       Compute centroid of selected UV coords, then reflect tu (if horizontal)
       or tv (if !horizontal) through that centroid. */
    float sumU = 0, sumV = 0;
    float avgMul = 1;
    for (const auto& p : polys) {
        for (int i = 0; i < 3; ++i) {
            if (p.v[i].selected) {
                sumU = sumU * (1.0f - 1.0f / avgMul) + p.v[i].tu / avgMul;
                sumV = sumV * (1.0f - 1.0f / avgMul) + p.v[i].tv / avgMul;
                avgMul += 1;
            }
        }
    }
    const float cu = sumU, cv = sumV;
    for (auto& p : polys) {
        for (int i = 0; i < 3; ++i) {
            if (p.v[i].selected) {
                if (horizontal)
                    p.v[i].tu = cu + (p.v[i].tu - cu) * -1.0f;
                else
                    p.v[i].tv = cv + (p.v[i].tv - cv) * -1.0f;
            }
        }
    }
    markModified();
}

void MapDocument::rotateTextureOnSelected(float angle, float texAspect) {
    /* Matches VB6 mnuRotateTexture_Click:
       Rotate UV coords around their centroid by 'angle' radians.
       texAspect = texWidth / texHeight is applied so the UV space is treated uniformly. */
    float cu = 0, cv = 0;
    float avgMul = 1;
    for (const auto& p : polys) {
        for (int i = 0; i < 3; ++i) {
            if (p.v[i].selected) {
                cu = cu * (1.0f - 1.0f / avgMul) + p.v[i].tu * texAspect / avgMul;
                cv = cv * (1.0f - 1.0f / avgMul) + p.v[i].tv / avgMul;
                avgMul += 1;
            }
        }
    }
    const float cosA = std::cos(angle);
    const float sinA = std::sin(angle);
    for (auto& p : polys) {
        for (int i = 0; i < 3; ++i) {
            if (p.v[i].selected) {
                const float dx = p.v[i].tu * texAspect - cu;
                const float dy = p.v[i].tv - cv;
                const float r = std::sqrt(dx * dx + dy * dy);
                if (r < 1e-9f) continue;
                float theta = std::atan2(dy, dx) + angle;
                (void)cosA; (void)sinA;
                p.v[i].tu = (cu + r * std::cos(theta)) / texAspect;
                p.v[i].tv =  cv + r * std::sin(theta);
            }
        }
    }
    markModified();
}

/* ---- Waypoint sever ----------------------------------------------------- */

void MapDocument::severWaypointConnections() {
    /* Matches VB6 mnuSever_Click:
       2+ selected waypoints → remove connections where BOTH endpoints are selected.
       1  selected waypoint  → remove ALL connections involving that waypoint. */
    int numSel = 0;
    for (const auto& wp : waypoints)
        if (wp.selected) ++numSel;

    if (numSel == 0) return;

    if (numSel > 1) {
        /* Remove connections where both endpoints are selected */
        for (auto& wp : waypoints)
            if (wp.selected)
                wp.connections.erase(
                    std::remove_if(wp.connections.begin(), wp.connections.end(),
                        [&](int connId) {
                            for (const auto& other : waypoints)
                                if (other.id == connId && other.selected)
                                    return true;
                            return false;
                        }),
                    wp.connections.end());
    } else {
        /* Single selected waypoint: remove ALL its connections, and any pointing to it */
        int selId = -1;
        for (const auto& wp : waypoints)
            if (wp.selected) { selId = wp.id; break; }
        if (selId < 0) return;
        for (auto& wp : waypoints) {
            if (wp.selected)
                wp.connections.clear();
            else
                wp.connections.erase(
                    std::remove(wp.connections.begin(), wp.connections.end(), selId),
                    wp.connections.end());
        }
    }
    markModified();
}

/* ---- Sketch ------------------------------------------------------------ */

void MapDocument::clearSketch() {
    sketch.clear();
    markModified();
}

bool MapDocument::eraseSketchAt(Vec2 worldPos, float radius) {
    /* VB6 EraseSketch (frm:8743) picks the single closest endpoint across all
       sketch lines and deletes that line by swapping the last element into its
       slot, which is why the surviving order is not stable. */
    const float r2 = radius * radius;
    float best = r2;
    int   bestIdx = -1;
    for (size_t i = 0; i < sketch.size(); ++i) {
        const Vec2 ends[2] = {sketch[i].a, sketch[i].b};
        for (const Vec2& e : ends) {
            const float dx = worldPos.x - e.x, dy = worldPos.y - e.y;
            const float d2 = dx * dx + dy * dy;
            if (d2 < best) { best = d2; bestIdx = static_cast<int>(i); }
        }
    }
    if (bestIdx < 0) return false;
    sketch[static_cast<size_t>(bestIdx)] = sketch.back();
    sketch.pop_back();
    markModified();
    return true;
}

bool MapDocument::smudgeSketchAt(Vec2 worldPos, float dx, float dy, float radius) {
    /* VB6 MoveLines (frm:8782): endpoints inside the brush are dragged by
       (dx,dy) attenuated by cos((d²/r²)·π/2), giving a soft falloff that
       reaches exactly zero at the brush edge. */
    if (radius <= 0.0f) return false;
    const float r2 = radius * radius;
    const float kHalfPi = 1.57079632679f;
    bool moved = false;
    for (auto& line : sketch) {
        Vec2* ends[2] = {&line.a, &line.b};
        for (Vec2* e : ends) {
            const float ex = worldPos.x - e->x, ey = worldPos.y - e->y;
            const float d2 = ex * ex + ey * ey;
            if (d2 >= r2) continue;
            const float w = std::cos((d2 / r2) * kHalfPi);
            e->x += dx * w;
            e->y += dy * w;
            moved = true;
        }
    }
    if (moved) markModified();
    return moved;
}

void MapDocument::beginSketchStroke(Vec2 worldPos) {
    /* VB6 StartSketch (frm:8082) appends a zero-length line. */
    EditorSketchLine line;
    line.a = worldPos;
    line.b = worldPos;
    sketch.push_back(line);
    markModified();
}

bool MapDocument::extendSketchStroke(Vec2 worldPos, bool finish) {
    /* VB6 LinkSketch (frm:8133): the live line's far end tracks the cursor;
       once it is more than 16 world units from that line's origin the segment
       is committed and a new one starts there.  EndSketch (frm:8160) drops the
       trailing stub if it never grew long enough to be a real segment. */
    if (sketch.empty()) { beginSketchStroke(worldPos); return false; }
    EditorSketchLine& cur = sketch.back();
    const float dx = worldPos.x - cur.a.x, dy = worldPos.y - cur.a.y;
    const bool split = (dx * dx + dy * dy > 16.0f * 16.0f);
    cur.b = worldPos;
    if (finish) {
        if (!split) sketch.pop_back();
        markModified();
        return false;
    }
    if (split) {
        EditorSketchLine next;
        next.a = worldPos;
        next.b = worldPos;
        sketch.push_back(next);
    }
    markModified();
    return split;
}

/* ---- Depthmap ---------------------------------------------------------- */

bool MapDocument::applyDepthNear(Vec2 worldPos, float radius, float value,
                                 float opacity) {
    /* VB6 EditDepthMap (frm:7662): when polygons are selected only their
       selected vertices are affected; otherwise only *unselected* vertices
       are, which is how the original avoids clobbering an active selection. */
    const float r2 = radius * radius;
    const bool haveSelection = [&] {
        for (const auto& p : polys)
            if (p.anySelected()) return true;
        return false;
    }();
    bool edited = false;
    for (auto& p : polys) {
        for (int j = 0; j < 3; ++j) {
            if (haveSelection ? !p.v[j].selected : p.v[j].selected) continue;
            const float dx = p.v[j].world.x - worldPos.x;
            const float dy = p.v[j].world.y - worldPos.y;
            if (dx * dx + dy * dy > r2) continue;
            p.v[j].z = p.v[j].z * (1.0f - opacity) + value * opacity;
            edited = true;
        }
    }
    if (edited) markModified();
    return edited;
}

/* ---- Pickers ----------------------------------------------------------- */

bool MapDocument::pickVertexInPoly(Vec2 worldPos, float radius,
                                   int& polyIdx, int& vertIdx) const {
    /* VB6 ColorPicker / DepthPicker / LightPicker (frm:7711-7860) only
       consider polygons that actually contain the cursor, then take the
       nearest vertex of those within a 32-unit box. */
    float best = radius * radius + 1.0f;
    polyIdx = vertIdx = -1;
    for (size_t i = 0; i < polys.size(); ++i) {
        if (!PointInTri(worldPos, polys[i].v[0].world,
                        polys[i].v[1].world, polys[i].v[2].world))
            continue;
        for (int j = 0; j < 3; ++j) {
            const float dx = polys[i].v[j].world.x - worldPos.x;
            const float dy = polys[i].v[j].world.y - worldPos.y;
            const float d2 = dx * dx + dy * dy;
            if (d2 < best) {
                best = d2;
                polyIdx = static_cast<int>(i);
                vertIdx = j;
            }
        }
    }
    return polyIdx >= 0;
}

/* ---- Texture tool ------------------------------------------------------ */

void MapDocument::offsetTextureOnSelected(float du, float dv) {
    for (auto& p : polys)
        for (int j = 0; j < 3; ++j)
            if (p.v[j].selected) { p.v[j].tu -= du; p.v[j].tv -= dv; }
    markModified();
}

/* ---- Visibility -------------------------------------------------------- */

void MapDocument::toggleSelectedVisibility() {
    /* VB6 mnuVisible_Click (frm:13728) toggles the pair (z, rhw) between
       (1, 1) and (-1, -10) for every vertex of a selected polygon. */
    for (auto& p : polys) {
        if (!p.anySelected()) continue;
        for (int j = 0; j < 3; ++j) {
            if (p.v[j].z < 0.0f) { p.v[j].z = 1.0f;  p.v[j].rhw = 1.0f; }
            else                 { p.v[j].z = -1.0f; p.v[j].rhw = -10.0f; }
        }
    }
    markModified();
}

/* ---- Waypoint connections ---------------------------------------------- */

bool MapDocument::connectWaypointAt(Vec2 worldPos, float radius) {
    /* VB6 CreateConnection (frm:7887). */
    float best = radius * radius + 1.0f;
    int   hit  = -1;
    for (size_t i = 0; i < waypoints.size(); ++i) {
        /* CreateConnection skips waypoints the filter is hiding (frm:7903),
           using the drawing test rather than the picking one. */
        if (!viewSettings.waypointPathDrawn(waypoints[i].pathNum))
            continue;
        const float dx = waypoints[i].x - worldPos.x;
        const float dy = waypoints[i].y - worldPos.y;
        const float d2 = dx * dx + dy * dy;
        if (d2 < best) { best = d2; hit = static_cast<int>(i); }
    }

    if (hit < 0) {
        currentWaypoint = -1;
        for (auto& w : waypoints) w.selected = false;
        return false;
    }

    bool created = false;
    if (currentWaypoint >= 0 && currentWaypoint != hit &&
        currentWaypoint < static_cast<int>(waypoints.size())) {
        EditorWaypoint& src = waypoints[static_cast<size_t>(currentWaypoint)];
        const int destId = waypoints[static_cast<size_t>(hit)].id;
        /* VB6 caps stored connections at 20 per waypoint on save. */
        if (src.connections.size() < 20 &&
            std::find(src.connections.begin(), src.connections.end(), destId) ==
                src.connections.end()) {
            src.connections.push_back(destId);
            created = true;
            markModified();
        }
    }
    currentWaypoint = hit;
    return created;
}

/* ---- Selection bounds / reference point --------------------------------- */

bool MapDocument::selectionBounds(float& minX, float& minY,
                                  float& maxX, float& maxY) const {
    bool any = false;
    auto acc = [&](float x, float y) {
        if (!any) { minX = maxX = x; minY = maxY = y; any = true; return; }
        minX = std::min(minX, x); maxX = std::max(maxX, x);
        minY = std::min(minY, y); maxY = std::max(maxY, y);
    };
    for (const auto& p : polys)
        for (int i = 0; i < 3; ++i)
            if (p.v[i].selected) acc(p.v[i].world.x, p.v[i].world.y);
    for (const auto& s : scenery)   if (s.selected) acc(s.x, s.y);
    for (const auto& s : spawns)    if (s.selected) acc(s.x, s.y);
    for (const auto& c : colliders) if (c.selected) acc(c.x, c.y);
    for (const auto& w : waypoints) if (w.selected) acc(w.x, w.y);
    for (const auto& l : lights)    if (l.selected) acc(l.x, l.y);
    return any;
}

void MapDocument::updateRCenter() {
    if (rCenterMode == RCenterMode::Set) return;
    rCenter = selectionCenter();
}

/* ---- Apply lights to base colors --------------------------------------- */

void MapDocument::applyLightsToBaseColors() {
    /* Matches VB6 mnuApplyLight_Click:
       For each polygon vertex (or selected only if selection is non-empty),
       compute the light contribution using the VB6 dot-product formula and
       bake it into the vertex r/g/b.  Then clear the lights array. */
    if (lights.empty()) return;

    const bool hasSelection = anySelected();

    const float PI = 3.14159265358979f;
    (void)PI;

    for (auto& p : polys) {
        /* For a 2-D polygon all vertices have z=0, so the face normal degenerates
           to (0,0,1).  VB6 uses the full cross-product path but arrives at the
           same result when all z-coords are equal. */
        float nx = 0, ny = 0, nz = 1;
        float mag = std::sqrt(nx*nx + ny*ny + nz*nz);
        if (mag > 0) { nx/=mag; ny/=mag; nz/=mag; }

        for (int j = 0; j < 3; ++j) {
            if (hasSelection && !p.v[j].selected) continue;

            int rVal = 0, gVal = 0, bVal = 0;

            for (const auto& light : lights) {
                float ldx = light.x - p.v[j].world.x;
                float ldy = light.y - p.v[j].world.y;
                float lmag = std::sqrt(ldx*ldx + ldy*ldy);
                float lnx = 0, lny = 0, lnz = 1;
                if (lmag > 0) { lnx = ldx/lmag; lny = ldy/lmag; }

                float diffuse = nx*lnx + ny*lny + nz*lnz;
                if (diffuse < 0) diffuse = 0;

                float atten;
                if (light.range == 0) {
                    atten = 1.0f;
                } else {
                    if (lmag <= light.range)
                        atten = 1.0f - lmag / static_cast<float>(light.range);
                    else
                        atten = 0.0f;
                }

                rVal += static_cast<int>(light.r * diffuse * atten);
                gVal += static_cast<int>(light.g * diffuse * atten);
                bVal += static_cast<int>(light.b * diffuse * atten);
            }

            rVal += p.v[j].r;
            gVal += p.v[j].g;
            bVal += p.v[j].b;

            p.v[j].r = static_cast<uint8_t>(std::min(rVal, 255));
            p.v[j].g = static_cast<uint8_t>(std::min(gVal, 255));
            p.v[j].b = static_cast<uint8_t>(std::min(bVal, 255));
        }
    }

    lights.clear();
    rebuildScreenCache();
    markModified();
}

/* ---- Map bounds / fit to viewport -------------------------------------- */

bool MapDocument::mapBounds(float& minX, float& minY, float& maxX, float& maxY) const {
    if (polys.empty()) {
        minX = minY = maxX = maxY = 0;
        return false;
    }
    minX = minY =  1e30f;
    maxX = maxY = -1e30f;
    for (const auto& p : polys) {
        for (int i = 0; i < 3; ++i) {
            minX = std::min(minX, p.v[i].world.x);
            minY = std::min(minY, p.v[i].world.y);
            maxX = std::max(maxX, p.v[i].world.x);
            maxY = std::max(maxY, p.v[i].world.y);
        }
    }
    return true;
}

void MapDocument::fitToViewport(float viewW, float viewH) {
    /* Matches VB6 mnuFitOnScreen_Click:
       Zoom the view so the entire polygon set fits within the viewport with
       a small margin, then centre the map in the viewport. */
    float minX, minY, maxX, maxY;
    if (!mapBounds(minX, minY, maxX, maxY)) return;

    const float margin = 32.0f;
    const float mapW = maxX - minX;
    const float mapH = maxY - minY;
    if (mapW <= 0 || mapH <= 0) return;

    const float usableW = viewW - margin * 2;
    const float usableH = viewH - margin * 2;
    float newZoom;
    if (mapH / mapW < usableH / usableW)
        newZoom = usableW / mapW;
    else
        newZoom = usableH / mapH;

    /* Clamp to legal range */
    if (newZoom < 0.03125f) newZoom = 0.03125f;
    if (newZoom > 512.0f)   newZoom = 512.0f;

    zoom = newZoom;
    const float centerX = (minX + maxX) * 0.5f;
    const float centerY = (minY + maxY) * 0.5f;
    scrollX = centerX - viewW * 0.5f / zoom;
    scrollY = centerY - viewH * 0.5f / zoom;
    rebuildScreenCache();
}
