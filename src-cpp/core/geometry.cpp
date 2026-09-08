#include "geometry.h"
#include <cmath>
#include <algorithm>

/* ---- Grid snapping ---------------------------------------------------- */

float snapToGrid(float coord, float gridSize) {
    if (gridSize <= 0) return coord;
    return std::floor(coord / gridSize + 0.5f) * gridSize;
}

/* ---- Point in polygon (CW winding) ------------------------------------ */

/*
 * VB6 PointInPoly uses a ray-cast from the point upward and counts edge
 * crossings.  Adapted to match the exact integer-arithmetic semantics:
 *
 *   For each edge (v[i], v[j]):
 *     if the vertical ray from (px, py) going in -Y direction crosses the edge,
 *     and the crossing is to the left of px, count it.
 *   Odd count → inside.
 *
 * This only returns reliable results for CW triangles as used by PolyWorks.
 */
bool pointInPoly(float px, float py, const PmsVertex v[3]) {
    bool inside = false;
    int j = 2;
    for (int i = 0; i < 3; ++i) {
        float xi = v[i].x, yi = v[i].y;
        float xj = v[j].x, yj = v[j].y;
        if (((yi > py) != (yj > py)) &&
            (px < (xj - xi) * (py - yi) / (yj - yi) + xi))
            inside = !inside;
        j = i;
    }
    return inside;
}

/* ---- Winding ---------------------------------------------------------- */

bool isPolyClockwise(const PmsVertex v[3]) {
    /* Cross product of (v1-v0) × (v2-v0).
     * In Y-down screen space, CW winding produces a positive Z component. */
    float ax = v[1].x - v[0].x, ay = v[1].y - v[0].y;
    float bx = v[2].x - v[0].x, by = v[2].y - v[0].y;
    return (ax * by - ay * bx) > 0.0f;
}

void swapVertices12(PmsVertex v[3], uint32_t col[3],
                    float tu[3], float tv[3]) {
    std::swap(v[1],   v[2]);
    std::swap(col[1], col[2]);
    std::swap(tu[1],  tu[2]);
    std::swap(tv[1],  tv[2]);
}

/* ---- Normal recomputation --------------------------------------------- */

void computePolyNormals(PmsPolygon& poly, uint8_t polyType) {
    for (int i = 0; i < 3; ++i) {
        int j = (i + 1) % 3;
        float dx = poly.v[j].x - poly.v[i].x;
        float dy = poly.v[j].y - poly.v[i].y;
        float len = std::sqrt(dx*dx + dy*dy);
        if (len < 1e-6f) len = 1e-6f;

        float bounciness = 1.0f;
        if (polyType == POLY_BOUNCY)
            bounciness = poly.perp.n[i].z;

        poly.perp.n[i].x =  dy / len * bounciness;
        poly.perp.n[i].y = -dx / len * bounciness;
        poly.perp.n[i].z = bounciness;
    }
}

/* ---- Segment intersection --------------------------------------------- */

/*
 * Parametric line-segment intersection (Liang-Barsky style):
 *   P = A + t(B-A)
 *   Q = C + u(D-C)
 *   Solve for t, u ∈ [0,1].
 */
bool segmentsIntersect(float x1, float y1, float x2, float y2,
                       float x3, float y3, float x4, float y4) {
    float dx12 = x2 - x1, dy12 = y2 - y1;
    float dx34 = x4 - x3, dy34 = y4 - y3;
    float denom = dy34 * dx12 - dx34 * dy12;
    if (std::fabs(denom) < 1e-10f) return false; /* parallel */

    float dx13 = x1 - x3, dy13 = y1 - y3;
    float t = (dx34 * dy13 - dy34 * dx13) / denom;
    float u = (dx12 * dy13 - dy12 * dx13) / denom;
    return t >= 0.0f && t <= 1.0f && u >= 0.0f && u <= 1.0f;
}

/* ---- Zoom snapping ---------------------------------------------------- */

/*
 * Canonical zoom levels matching VB6 GetZoomDir:
 * 0.03125, 0.0625, 0.125, 0.25, 0.5, 1, 2, 4, 8, 16, 32, 64, 128, 256, 512
 */
static const float kZoomLevels[] = {
    0.03125f, 0.0625f, 0.125f, 0.25f, 0.5f,
    1.0f, 2.0f, 4.0f, 8.0f, 16.0f,
    32.0f, 64.0f, 128.0f, 256.0f, 512.0f
};
static const int kNumZoomLevels = sizeof(kZoomLevels) / sizeof(kZoomLevels[0]);

float snapZoom(float current, int dir,
               float minZoom, float maxZoom) {
    /* Find the index of the level closest to current. */
    int best = 0;
    float bestDiff = std::fabs(current - kZoomLevels[0]);
    for (int i = 1; i < kNumZoomLevels; ++i) {
        float d = std::fabs(current - kZoomLevels[i]);
        if (d < bestDiff) { bestDiff = d; best = i; }
    }
    int next = best + (dir > 0 ? 1 : -1);
    if (next < 0) next = 0;
    if (next >= kNumZoomLevels) next = kNumZoomLevels - 1;
    float z = kZoomLevels[next];
    if (z < minZoom) z = minZoom;
    if (z > maxZoom) z = maxZoom;
    return z;
}

/* ---- Polygon type colours --------------------------------------------- */

uint32_t polyTypeColor(int polyType, uint32_t selectionColor) {
    /* Defaults verbatim from modConfig.bas:211-235.  Indices 12..17 alias
       10/11 in the original (TeamBullets / TeamPlayers reused for Bravo,
       Charlie and Delta). */
    static const uint32_t kColors[26] = {
        0x000000,  /*  0 Normal          -> selectionColor          */
        0x7ACC29,  /*  1 OnlyBullets     */
        0xCCCC29,  /*  2 OnlyPlayer      */
        0x29CC29,  /*  3 DoesntCollide   */
        0x29CCCC,  /*  4 Ice             */
        0xCC297A,  /*  5 Deadly          */
        0xCC29CC,  /*  6 BloodyDeadly    */
        0xCC2929,  /*  7 Hurts           */
        0x2929CC,  /*  8 Regenerates     */
        0xCC7A29,  /*  9 Lava            */
        0x7A7A29,  /* 10 TeamBullets     */
        0x7A2929,  /* 11 TeamPlayers     */
        0x7A7A29,  /* 12 = 10            */
        0x7A2929,  /* 13 = 11            */
        0x7A7A29,  /* 14 = 10            */
        0x7A2929,  /* 15 = 11            */
        0x7A7A29,  /* 16 = 10            */
        0x7A2929,  /* 17 = 11            */
        0x297ACC,  /* 18 Bouncy          */
        0xCCCCCC,  /* 19 Explosive       */
        0xCCCC7A,  /* 20 HurtFlaggers    */
        0x7A7ACC,  /* 21 OnlyFlagger     */
        0x7A29CC,  /* 22 NonFlagger      */
        0x29297A,  /* 23 FlagCollides    */
        0x292929,  /* 24 Back            */
        0x7A7A7A,  /* 25 BackTransition  */
    };
    if (polyType <= 0) return selectionColor;
    if (polyType > 25) return selectionColor;
    return kColors[polyType];
}

/* ---- Polygon type names ------------------------------------------------ */

const char* polyTypeName(int polyType) {
    static const char* const kNames[POLY_TYPE_COUNT] = {
        "Normal",
        "Only Bullets Collide",
        "Only Player Collides",
        "Doesn't Collide",
        "Ice",
        "Deadly",
        "Bloody Deadly",
        "Hurts",
        "Regenerates",
        "Lava",
        "Red Bullets Collides",
        "Red Players Collide",
        "Blue Bullets Collide",
        "Blue Players Collide",
        "Yellow Bullets Collide",
        "Yellow Players Collide",
        "Green Bullets Collide",
        "Green Players Collide",
        "Bouncy",
        "Explosive",
        "Hurts Flaggers",
        "Flagger Collides",
        "Non-Flagger Collides",
        "Flag Collides",
        "Background",
        "Background Transition",
    };
    if (polyType < 0 || polyType >= POLY_TYPE_COUNT) return "";
    return kNames[polyType];
}
