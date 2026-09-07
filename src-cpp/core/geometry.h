#pragma once
/*
 * geometry.h — Pure geometric operations.
 *
 * All functions are stateless; they take explicit arguments.
 * No VB6 global state is referenced here.
 */

#include "pms_types.h"
#include <cstdint>

/* ---- Coordinate conversion -------------------------------------------- */

inline void worldToScreen(float wx, float wy,
                           float scrollX, float scrollY, float zoom,
                           float& sx, float& sy) {
    sx = (wx - scrollX) * zoom;
    sy = (wy - scrollY) * zoom;
}

inline void screenToWorld(float sx, float sy,
                           float scrollX, float scrollY, float zoom,
                           float& wx, float& wy) {
    wx = sx / zoom + scrollX;
    wy = sy / zoom + scrollY;
}

/* ---- Grid snapping ---------------------------------------------------- */

/* Snap a world coordinate to the nearest grid line.
 * Matches VB6 Floor() (not Trunc) so negatives snap correctly. */
float snapToGrid(float coord, float gridSize);

/* ---- Point-in-polygon (CW winding only) ------------------------------- */

bool pointInPoly(float px, float py, const PmsVertex v[3]);

/* ---- Winding ---------------------------------------------------------- */

/* Returns true when triangle is clockwise (VB6 PointInPoly assumption). */
bool isPolyClockwise(const PmsVertex v[3]);

/* Swap vertex 1 and vertex 2 (0-based) to enforce CW winding. */
void swapVertices12(PmsVertex v[3], uint32_t col[3], float tu[3], float tv[3]);

/* ---- Normal recomputation --------------------------------------------- */

void computePolyNormals(PmsPolygon& poly, uint8_t polyType);

/* ---- Segment intersection --------------------------------------------- */

bool segmentsIntersect(float x1, float y1, float x2, float y2,
                       float x3, float y3, float x4, float y4);

/* ---- Proximity test --------------------------------------------------- */

/* Returns true when |coord - target| <= range. */
inline bool nearCoord(float coord, float target, float range) {
    float d = coord - target;
    return d >= -range && d <= range;
}

/* ---- Selection rectangle (exclusive test, matching VB6) --------------- */

inline bool inSelRect(float x, float y,
                      float rx1, float ry1, float rx2, float ry2) {
    float minX = rx1 < rx2 ? rx1 : rx2;
    float maxX = rx1 < rx2 ? rx2 : rx1;
    float minY = ry1 < ry2 ? ry1 : ry2;
    float maxY = ry1 < ry2 ? ry2 : ry1;
    return x > minX && x < maxX && y > minY && y < maxY;
}

/* ---- Zoom snapping ---------------------------------------------------- */

/* Snap zoom to the nearest "nice" level. dir > 0 = zoom in, < 0 = out. */
float snapZoom(float current, int dir, float minZoom = PMS_ZOOM_MIN,
               float maxZoom = PMS_ZOOM_MAX);
