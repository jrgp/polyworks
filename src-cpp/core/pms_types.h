#pragma once
/*
 * pms_types.h — Binary-compatible PMS file format structs.
 *
 * All structs match the VB6 original (modOpenSoldatMap.bas) byte-for-byte.
 * See the file-format comment in pms_io.cpp for the full layout.
 *
 * VB6 type mapping:
 *   Long     = int32_t   (4 bytes, signed)
 *   Integer  = int16_t   (2 bytes, signed)
 *   Boolean  = int16_t   (-1 = True, 0 = False in VB6 UDTs)
 *   Single   = float     (4 bytes, IEEE 754)
 *   Byte     = uint8_t   (1 byte)
 *   Long (color/ARGB) = uint32_t
 */

#include <cstdint>
#include <cmath>

/* ---- Constants ---------------------------------------------------------- */
static constexpr int PMS_VERSION              = 11;
static constexpr int SECTOR_NUM               = 25;
static constexpr int SECTOR_CELLS             = 51;  /* 2*SECTOR_NUM + 1 */
static constexpr int MAX_POLYS                = 4000;
static constexpr int MAX_SCENERY              = 500;
static constexpr int MAX_SPAWNS               = 256;
static constexpr int MAX_COLLIDERS            = 128;
static constexpr int MAX_WAYPOINTS            = 500;
static constexpr int MAX_CONNECTIONS_PER_WP   = 20;
static constexpr float PMS_ZOOM_MIN             = 0.03125f;
static constexpr float PMS_ZOOM_DEFAULT         = 1.0f;
static constexpr float PMS_ZOOM_MAX             = 512.0f;

/* Polygon type codes */
static constexpr uint8_t POLY_NORMAL          = 0;
static constexpr uint8_t POLY_ONLY_BULLETS    = 1;
static constexpr uint8_t POLY_ONLY_PLAYERS    = 2;
static constexpr uint8_t POLY_NO_COLLIDE      = 3;
static constexpr uint8_t POLY_ICE             = 4;
static constexpr uint8_t POLY_DEADLY          = 5;
static constexpr uint8_t POLY_BLOODY_DEADLY   = 6;
static constexpr uint8_t POLY_HURTS           = 7;
static constexpr uint8_t POLY_REGENERATES     = 8;
static constexpr uint8_t POLY_LAVA            = 9;
static constexpr uint8_t POLY_ALPHA_BULLETS   = 10;
static constexpr uint8_t POLY_ALPHA_PLAYERS   = 11;
static constexpr uint8_t POLY_BRAVO_BULLETS   = 12;
static constexpr uint8_t POLY_BRAVO_PLAYERS   = 13;
static constexpr uint8_t POLY_CHARLIE_BULLETS = 14;
static constexpr uint8_t POLY_CHARLIE_PLAYERS = 15;
static constexpr uint8_t POLY_DELTA_BULLETS   = 16;
static constexpr uint8_t POLY_DELTA_PLAYERS   = 17;
static constexpr uint8_t POLY_BOUNCY          = 18;
static constexpr uint8_t POLY_EXPLOSIVE       = 19;
static constexpr uint8_t POLY_HIT_MULTIPLY    = 20;
static constexpr uint8_t POLY_COLLIDER        = 21;
static constexpr uint8_t POLY_NO_PASS         = 22;
static constexpr uint8_t POLY_SHIFT           = 23;
static constexpr uint8_t POLY_WEATHER         = 24;
static constexpr uint8_t POLY_NO_FOOTSTEPS    = 25;
static constexpr uint8_t POLY_TYPE_MAX        = 25;

/* Spawn team codes */
static constexpr int SPAWN_GENERAL  = 0;
static constexpr int SPAWN_ALPHA    = 1;
static constexpr int SPAWN_BRAVO    = 2;
static constexpr int SPAWN_CHARLIE  = 3;
static constexpr int SPAWN_DELTA    = 4;
static constexpr int SPAWN_FROGGER  = 5;
static constexpr int SPAWN_YELLOW   = 6;
static constexpr int SPAWN_RED      = 7;

/* Scenery level codes */
static constexpr int SCENERY_BACK   = 0;
static constexpr int SCENERY_MIDDLE = 1;
static constexpr int SCENERY_FRONT  = 2;

/* ---- Packed structs (must match VB6 UDT binary layout exactly) --------- */
#pragma pack(push, 1)

/* TCustomVertex in VB6.  28 bytes. */
struct PmsVertex {
    float    x;      /* world X */
    float    y;      /* world Y */
    float    z;      /* always 1.0 on disk */
    float    rhw;    /* always 1.0 on disk */
    uint32_t color;  /* ARGB: 0xAARRGGBB */
    float    tu;     /* texture U */
    float    tv;     /* texture V */
};
static_assert(sizeof(PmsVertex) == 28, "PmsVertex size mismatch");

/* TVertexHit in VB6.  12 bytes. */
struct PmsNormal {
    float x;   /* sin(edge_angle) * bounciness */
    float y;   /* cos(edge_angle) * bounciness */
    float z;   /* 1.0 normally, bounciness value for POLY_BOUNCY */
};
static_assert(sizeof(PmsNormal) == 12, "PmsNormal size mismatch");

/* TPolyHit in VB6.  36 bytes. */
struct PmsPolyNormals {
    PmsNormal n[3];  /* n[0], n[1], n[2] — index 0-based here, 1-based in VB6 */
};
static_assert(sizeof(PmsPolyNormals) == 36, "PmsPolyNormals size mismatch");

/* TPolygon in VB6.  120 bytes. */
struct PmsPolygon {
    PmsVertex     v[3];   /* v[0..2] */
    PmsPolyNormals perp;
};
static_assert(sizeof(PmsPolygon) == 120, "PmsPolygon size mismatch");

/* TMapFile_Polygon in VB6.  121 bytes. */
struct PmsPolyEntry {
    PmsPolygon poly;
    uint8_t    polyType;
};
static_assert(sizeof(PmsPolyEntry) == 121, "PmsPolyEntry size mismatch");

/* TProp in VB6.  44 bytes. */
struct PmsProp {
    int16_t  active;    /* VB6 Boolean in UDT = 2 bytes; -1=true, 0=false */
    int16_t  style;     /* 1-based index into scenery texture list */
    int32_t  width;     /* original texture width in pixels */
    int32_t  height;    /* original texture height in pixels */
    float    x;
    float    y;
    float    rotation;  /* radians */
    float    scaleX;
    float    scaleY;
    int32_t  alpha;     /* 0..255 stored as Long */
    int32_t  color;     /* ARGB tint */
    int32_t  level;     /* 0=back, 1=middle, 2=front */
};
static_assert(sizeof(PmsProp) == 44, "PmsProp size mismatch");

/* TMapFile_Scenery in VB6.  55 bytes.
   name[0] = string length; name[1..length] = ASCII chars. */
struct PmsSceneryName {
    uint8_t  name[51]; /* byte 0 = length, max 50 chars */
    int32_t  date;     /* file date stamp */
};
static_assert(sizeof(PmsSceneryName) == 55, "PmsSceneryName size mismatch");

/* TCollider in VB6.  16 bytes. */
struct PmsCollider {
    int32_t active;   /* non-zero = active */
    float   x;
    float   y;
    float   radius;
};
static_assert(sizeof(PmsCollider) == 16, "PmsCollider size mismatch");

/* TSaveSpawnPoint in VB6.  16 bytes.  Note X/Y are int32 (not float). */
struct PmsSpawnPoint {
    int32_t active;
    int32_t x;      /* integer world X */
    int32_t y;      /* integer world Y */
    int32_t team;
};
static_assert(sizeof(PmsSpawnPoint) == 16, "PmsSpawnPoint size mismatch");

/* TNewWaypoint in VB6.  112 bytes. */
struct PmsWaypoint {
    int32_t  active;
    int32_t  id;
    int32_t  x;
    int32_t  y;
    uint8_t  left;
    uint8_t  right;
    uint8_t  up;
    uint8_t  down;
    uint8_t  m2;
    uint8_t  pathNum;
    uint8_t  special;
    uint8_t  crap[5];
    int32_t  connectionsNum;
    int32_t  connections[20];  /* outgoing connection waypoint IDs */
};
static_assert(sizeof(PmsWaypoint) == 112, "PmsWaypoint size mismatch");

/* Three-channel colour.  TColor in VB6.  3 bytes. */
struct Color3 {
    uint8_t r, g, b;
};
static_assert(sizeof(Color3) == 3, "Color3 size mismatch");

/* TLightSource in VB6.  22 bytes. */
struct PmsLight {
    uint8_t  selected;
    Color3   color;
    float    intensity;
    int16_t  range;   /* VB6 Integer = 2 bytes */
    float    x;
    float    y;
    float    z;
};
static_assert(sizeof(PmsLight) == 22, "PmsLight size mismatch");

/* TSketchVertex in VB6.  12 bytes. */
struct PmsSketchVertex {
    float x, y, z;
};
static_assert(sizeof(PmsSketchVertex) == 12, "PmsSketchVertex size mismatch");

/* TSketchLine in VB6.  24 bytes. */
struct PmsSketchLine {
    PmsSketchVertex v[2];
};
static_assert(sizeof(PmsSketchLine) == 24, "PmsSketchLine size mismatch");

/* TOptions in VB6.  84 bytes. */
struct PmsOptions {
    uint8_t  mapName[39];     /* byte 0 = length, max 38 ASCII chars */
    uint8_t  textureName[25]; /* byte 0 = length, max 24 ASCII chars */
    uint32_t bgColor1;        /* ARGB background top */
    uint32_t bgColor2;        /* ARGB background bottom */
    int32_t  startJet;
    uint8_t  grenadePacks;
    uint8_t  medikits;
    uint8_t  weather;
    uint8_t  steps;
    int32_t  mapRandomID;     /* -1=PolyWorks native; >0=compiled game map */
};
static_assert(sizeof(PmsOptions) == 84, "PmsOptions size mismatch");

#pragma pack(pop)

/* ---- Helpers for Pascal-style length-prefixed byte strings ------------- */
#include <string>

inline std::string pms_read_string(const uint8_t* buf, int maxLen) {
    int len = buf[0];
    if (len > maxLen) len = maxLen;
    return std::string(reinterpret_cast<const char*>(buf + 1),
                       static_cast<size_t>(len));
}

inline void pms_write_string(uint8_t* buf, int bufSize, const std::string& s) {
    int len = static_cast<int>(s.size());
    if (len > bufSize - 1) len = bufSize - 1;
    buf[0] = static_cast<uint8_t>(len);
    for (int i = 0; i < len; ++i)
        buf[i + 1] = static_cast<uint8_t>(s[i]);
    for (int i = len + 1; i < bufSize; ++i)
        buf[i] = 0;
}

/* ARGB helpers (VB6 ARGB = 0xAARRGGBB) */
inline uint32_t argb(uint8_t a, uint8_t r, uint8_t g, uint8_t b) {
    return (uint32_t(a) << 24) | (uint32_t(r) << 16) |
           (uint32_t(g) << 8)  |  uint32_t(b);
}
inline uint8_t argb_a(uint32_t c) { return (c >> 24) & 0xFF; }
inline uint8_t argb_r(uint32_t c) { return (c >> 16) & 0xFF; }
inline uint8_t argb_g(uint32_t c) { return (c >>  8) & 0xFF; }
inline uint8_t argb_b(uint32_t c) { return  c        & 0xFF; }
