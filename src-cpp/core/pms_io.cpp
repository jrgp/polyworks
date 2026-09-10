/*
 * pms_io.cpp — PMS file read / write / compile.
 *
 * Binary format (all little-endian, matches VB6 Put/Get):
 *
 *   Offset    Size  Description
 *   ------    ----  -----------
 *   0         4     Version (int32) = 11
 *   4         84    PmsOptions
 *   88        4     PolyCount (int32)
 *   92        121×N PmsPolyEntry  (121 bytes each)
 *   92+121N   4     SectorsDivision (int32)
 *   96+121N   4     SECTOR_NUM (int32, always 25)
 *   100+..    var   Sector table: 51×51 cells; each cell = int16 count +
 *                     count × int16 poly-indices (1-based in file, 0-based here)
 *   after     4     PropCount (int32)
 *   ..        44×P  PmsProp each
 *   ..        4     SceneryNameCount (int32)
 *   ..        55×E  PmsSceneryName each
 *   ..        4     ColliderCount (int32)
 *   ..        16×C  PmsCollider each
 *   ..        4     SpawnCount (int32)
 *   ..        16×S  PmsSpawnPoint each
 *   ..        4     WaypointCount (int32)
 *   ..        112×W PmsWaypoint each
 *
 *   If options.mapRandomID < 0 (PolyWorks native):
 *   ..        2     LightCount (int16)
 *   ..        22×L  PmsLight each
 *   ..        2     SketchCount (int16)
 *   ..        24×K  PmsSketchLine each
 *
 *   Else (compiled game format):
 *   ..        8     four int16 zeros (SaveAndCompile trailing padding)
 */

#include "pms_io.h"

#include <fstream>
#include <sstream>
#include <cstring>
#include <cmath>
#include <stdexcept>
#include <algorithm>
#include <map>
#include <random>

/* ---- Stream helpers ---------------------------------------------------- */

namespace {

template<typename T>
void read_raw(std::istream& s, T& v) {
    if (!s.read(reinterpret_cast<char*>(&v), sizeof(T)))
        throw std::runtime_error("Unexpected end of file");
}

template<typename T>
void write_raw(std::ostream& s, const T& v) {
    s.write(reinterpret_cast<const char*>(&v), sizeof(T));
}

void read_raw_buf(std::istream& s, void* buf, size_t n) {
    if (!s.read(reinterpret_cast<char*>(buf), static_cast<std::streamsize>(n)))
        throw std::runtime_error("Unexpected end of file");
}

void write_raw_buf(std::ostream& s, const void* buf, size_t n) {
    s.write(reinterpret_cast<const char*>(buf), static_cast<std::streamsize>(n));
}

/* ---- Sector table I/O -------------------------------------------------- */

void read_sectors(std::istream& s, SectorCell sectors[][SECTOR_CELLS]) {
    for (int i = 0; i < SECTOR_CELLS; ++i)
        for (int j = 0; j < SECTOR_CELLS; ++j) {
            int16_t count = 0;
            read_raw(s, count);
            if (count < 0) count = 0;
            sectors[i][j].polyCount = count;
            sectors[i][j].polyIndex.resize(count);
            for (int k = 0; k < count; ++k) {
                int16_t idx = 0;
                read_raw(s, idx);
                /* VB6 stores 1-based indices; convert to 0-based. */
                sectors[i][j].polyIndex[k] = idx - 1;
            }
        }
}

void write_sectors_zero(std::ostream& s) {
    /* PolyWorks native saves zero sector table (5202 bytes). */
    int16_t zero = 0;
    for (int i = 0; i < SECTOR_CELLS * SECTOR_CELLS; ++i)
        write_raw(s, zero);
}

void write_sectors(std::ostream& s,
                   const SectorCell sectors[][SECTOR_CELLS]) {
    for (int i = 0; i < SECTOR_CELLS; ++i)
        for (int j = 0; j < SECTOR_CELLS; ++j) {
            int16_t cnt = static_cast<int16_t>(sectors[i][j].polyCount);
            write_raw(s, cnt);
            for (int k = 0; k < cnt; ++k) {
                /* Convert back to 1-based for file. */
                int16_t idx = static_cast<int16_t>(sectors[i][j].polyIndex[k] + 1);
                write_raw(s, idx);
            }
        }
}

/* ---- Core read/write --------------------------------------------------- */

void do_read(std::istream& s, PmsData& d) {
    read_raw(s, d.version);
    if (d.version != PMS_VERSION) {
        std::ostringstream oss;
        oss << "Unsupported PMS version: " << d.version
            << " (expected " << PMS_VERSION << ")";
        throw std::runtime_error(oss.str());
    }

    read_raw(s, d.options);

    int32_t polyCount = 0;
    read_raw(s, polyCount);
    d.polys.resize(static_cast<size_t>(polyCount));
    for (auto& p : d.polys)
        read_raw_buf(s, &p, sizeof(PmsPolyEntry));

    read_raw(s, d.sectorDiv);
    int32_t sectorNumFile = 0;
    read_raw(s, sectorNumFile); /* always 25, ignored */
    read_sectors(s, d.sectors);

    int32_t propCount = 0;
    read_raw(s, propCount);
    d.props.resize(static_cast<size_t>(propCount));
    for (auto& p : d.props)
        read_raw_buf(s, &p, sizeof(PmsProp));

    int32_t scenNameCount = 0;
    read_raw(s, scenNameCount);
    d.sceneryNames.resize(static_cast<size_t>(scenNameCount));
    for (auto& n : d.sceneryNames)
        read_raw_buf(s, &n, sizeof(PmsSceneryName));

    int32_t colliderCount = 0;
    read_raw(s, colliderCount);
    d.colliders.resize(static_cast<size_t>(colliderCount));
    for (auto& c : d.colliders)
        read_raw_buf(s, &c, sizeof(PmsCollider));

    int32_t spawnCount = 0;
    read_raw(s, spawnCount);
    d.spawns.resize(static_cast<size_t>(spawnCount));
    for (auto& sp : d.spawns)
        read_raw_buf(s, &sp, sizeof(PmsSpawnPoint));

    int32_t wpCount = 0;
    read_raw(s, wpCount);
    d.waypoints.resize(static_cast<size_t>(wpCount));
    for (auto& wp : d.waypoints)
        read_raw_buf(s, &wp, sizeof(PmsWaypoint));

    /* PolyWorks extension: lights + sketch */
    if (d.options.mapRandomID < 0) {
        int16_t lightCount = 0;
        read_raw(s, lightCount);
        d.lights.resize(static_cast<size_t>(lightCount));
        for (auto& l : d.lights)
            read_raw_buf(s, &l, sizeof(PmsLight));

        int16_t sketchCount = 0;
        read_raw(s, sketchCount);
        d.sketch.resize(static_cast<size_t>(sketchCount));
        for (auto& sk : d.sketch)
            read_raw_buf(s, &sk, sizeof(PmsSketchLine));
    }
    /* Trailing 8 zero bytes from compiled maps are safely ignored. */
}

void do_write(std::ostream& s, const PmsData& d, bool nativeSave) {
    write_raw(s, d.version);

    PmsOptions opt = d.options;
    if (nativeSave)
        opt.mapRandomID = -1;
    write_raw(s, opt);

    int32_t polyCount = static_cast<int32_t>(d.polys.size());
    write_raw(s, polyCount);
    for (const auto& p : d.polys)
        write_raw_buf(s, &p, sizeof(PmsPolyEntry));

    int32_t sectorDiv = d.sectorDiv ? d.sectorDiv : 1;
    int32_t sectorNumConst = SECTOR_NUM;
    write_raw(s, sectorDiv);
    write_raw(s, sectorNumConst);

    if (nativeSave)
        write_sectors_zero(s);
    else
        write_sectors(s, d.sectors);

    int32_t propCount = static_cast<int32_t>(d.props.size());
    write_raw(s, propCount);
    for (const auto& p : d.props)
        write_raw_buf(s, &p, sizeof(PmsProp));

    int32_t scenNameCount = static_cast<int32_t>(d.sceneryNames.size());
    write_raw(s, scenNameCount);
    for (const auto& n : d.sceneryNames)
        write_raw_buf(s, &n, sizeof(PmsSceneryName));

    int32_t colliderCount = static_cast<int32_t>(d.colliders.size());
    write_raw(s, colliderCount);
    for (const auto& c : d.colliders)
        write_raw_buf(s, &c, sizeof(PmsCollider));

    int32_t spawnCount = static_cast<int32_t>(d.spawns.size());
    write_raw(s, spawnCount);
    for (const auto& sp : d.spawns)
        write_raw_buf(s, &sp, sizeof(PmsSpawnPoint));

    int32_t wpCount = static_cast<int32_t>(d.waypoints.size());
    write_raw(s, wpCount);
    for (const auto& wp : d.waypoints)
        write_raw_buf(s, &wp, sizeof(PmsWaypoint));

    /* PolyWorks extension */
    if (nativeSave) {
        int16_t lightCount = static_cast<int16_t>(d.lights.size());
        write_raw(s, lightCount);
        for (const auto& l : d.lights)
            write_raw_buf(s, &l, sizeof(PmsLight));

        int16_t sketchCount = static_cast<int16_t>(d.sketch.size());
        write_raw(s, sketchCount);
        for (const auto& sk : d.sketch)
            write_raw_buf(s, &sk, sizeof(PmsSketchLine));
    } else {
        /* Compiled: write 4 × int16(0) trailing padding */
        int16_t zero = 0;
        write_raw(s, zero); write_raw(s, zero);
        write_raw(s, zero); write_raw(s, zero);
    }
}

} /* anonymous namespace */

/* ---- Public file I/O --------------------------------------------------- */

PmsLoadResult loadPmsFile(const std::string& path, PmsData& out,
                          std::string& err) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        err = "File not found: " + path;
        return PmsLoadResult::FileNotFound;
    }
    try {
        do_read(f, out);
    } catch (const std::runtime_error& e) {
        err = e.what();
        std::string msg(e.what());
        if (msg.find("version") != std::string::npos)
            return PmsLoadResult::VersionMismatch;
        return PmsLoadResult::Truncated;
    } catch (...) {
        err = "Unknown error loading PMS";
        return PmsLoadResult::Corrupt;
    }
    return PmsLoadResult::OK;
}

bool savePmsFile(const std::string& path, const PmsData& data,
                 std::string& err) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) { err = "Cannot open file for writing: " + path; return false; }
    try { do_write(f, data, true); }
    catch (const std::exception& e) { err = e.what(); return false; }
    return true;
}

/* ---- Compile ----------------------------------------------------------- */

/*
 * Recompute edge normals exactly as VB6 SaveAndCompile does
 * (frmOpenSoldatMapEditor.frm:2661-2686).
 *
 *   xDiff = v[next].X - v[j].X
 *   yDiff = v[j].Y   - v[next].Y     <- note the reversed subtraction
 *   Perp.X = (yDiff / len) * bounciness
 *   Perp.Y = (xDiff / len) * bounciness
 *   Perp.Z = 1                       <- always 1 on disk
 *   vertex.Z = 1
 *
 * Bounciness lives in the *magnitude* of the stored normal (VB6 recomputes
 * Perp.Z = Sqr(X^2 + Y^2) on load), and only applies to POLY_BOUNCY.
 *
 * Verified against the shipped Soldat maps in maps/: 6483 of 6483 sampled
 * edges match this sign convention, and Perp.Z is 1 (or 0) in every map.
 */
static void computePolyNormals(PmsPolyEntry& entry) {
    for (int i = 0; i < 3; ++i) {
        int j = (i + 1) % 3;
        float xDiff = entry.poly.v[j].x - entry.poly.v[i].x;
        float yDiff = entry.poly.v[i].y - entry.poly.v[j].y;
        float len = (xDiff == 0.0f && yDiff == 0.0f)
                        ? 1.0f
                        : std::sqrt(xDiff * xDiff + yDiff * yDiff);

        float bounciness = 1.0f;
        if (entry.polyType == POLY_BOUNCY) {
            const PmsNormal& n = entry.poly.perp.n[i];
            bounciness = std::sqrt(n.x * n.x + n.y * n.y);
            if (bounciness < 1.0f) bounciness = 1.0f;  /* VB6 clamp */
        }

        entry.poly.perp.n[i].x = (yDiff / len) * bounciness;
        entry.poly.perp.n[i].y = (xDiff / len) * bounciness;
        entry.poly.perp.n[i].z = 1.0f;
        entry.poly.v[i].z      = 1.0f;
    }
}

/* ---- VB6 IsInSector (frmOpenSoldatMapEditor.frm:5684) ------------------ */

static bool pointInPoly(const PmsPolygon& p, float x, float y) {
    /* VB6 PointInPoly (frm:9949) — inside test against the three edges using
       the same reversed-subtraction convention as the normals. */
    for (int a = 0; a < 3; ++a) {
        int b = (a + 1) % 3;
        float xDist = x - p.v[a].x;
        float yDist = y - p.v[a].y;
        float xDiff = p.v[b].x - p.v[a].x;
        float yDiff = p.v[a].y - p.v[b].y;
        float len = (xDiff == 0.0f && yDiff == 0.0f)
                        ? 1.0f
                        : std::sqrt(xDiff * xDiff + yDiff * yDiff);
        float d = (yDiff / len) * xDist + (xDiff / len) * yDist;
        if (d < 0) return false;
    }
    return true;
}

static bool isBetween(float p1, float p2, float p3) {
    return (p1 >= p2 && p2 >= p3) || (p3 >= p2 && p2 >= p1);
}

static bool isInSector(const PmsPolygon& p, float x, float y, float div) {
    /* Trivially outside? */
    if (p.v[0].x < x && p.v[1].x < x && p.v[2].x < x) return false;
    if (p.v[0].x > x + div && p.v[1].x > x + div && p.v[2].x > x + div) return false;
    if (p.v[0].y < y && p.v[1].y < y && p.v[2].y < y) return false;
    if (p.v[0].y > y + div && p.v[1].y > y + div && p.v[2].y > y + div) return false;

    /* Any vertex inside the cell? */
    for (int i = 0; i < 3; ++i)
        if (isBetween(x, p.v[i].x, x + div) && isBetween(y, p.v[i].y, y + div))
            return true;

    /* Any cell corner inside the poly? */
    return pointInPoly(p, x,       y) ||
           pointInPoly(p, x + div, y) ||
           pointInPoly(p, x,       y + div) ||
           pointInPoly(p, x + div, y + div);
}

/*
 * Build the collision sector table exactly as VB6 SaveAndCompile does
 * (frm:2689-2727):
 *   - cells outside +/-xSecNum, +/-ySecNum are written empty
 *   - POLY_NO_COLLIDE (type 3) polygons are never indexed
 *   - each cell is capped at 256 polygons
 *   - the cell probe rect is (div*(X-0.5)-1, div*(Y-0.5)-1) sized div+2
 *     (VB6 coerces those origins to Integer, i.e. round-half-to-even)
 */
static void buildSectorTable(PmsData& d, float xSecNum, float ySecNum) {
    for (int i = 0; i < SECTOR_CELLS; ++i)
        for (int j = 0; j < SECTOR_CELLS; ++j) {
            d.sectors[i][j].polyCount = 0;
            d.sectors[i][j].polyIndex.clear();
        }

    const float div = static_cast<float>(d.sectorDiv > 0 ? d.sectorDiv : 1);

    for (int X = -SECTOR_NUM; X <= SECTOR_NUM; ++X) {
        for (int Y = -SECTOR_NUM; Y <= SECTOR_NUM; ++Y) {
            SectorCell& cell = d.sectors[X + SECTOR_NUM][Y + SECTOR_NUM];
            if (X < -xSecNum || X > xSecNum || Y < -ySecNum || Y > ySecNum)
                continue;  /* out of range: stays empty */

            /* VB6 passes these as Integer -> banker's rounding. */
            float ox = std::nearbyint(div * (static_cast<float>(X) - 0.5f) - 1.0f);
            float oy = std::nearbyint(div * (static_cast<float>(Y) - 0.5f) - 1.0f);

            for (int pi = 0; pi < static_cast<int>(d.polys.size()); ++pi) {
                if (d.polys[pi].polyType == POLY_NO_COLLIDE) continue;
                if (!isInSector(d.polys[pi].poly, ox, oy, div + 2.0f)) continue;
                if (static_cast<int>(cell.polyIndex.size()) >= 256) break;
                cell.polyIndex.push_back(pi);
            }
            cell.polyCount = static_cast<int>(cell.polyIndex.size());
        }
    }
}

bool compilePms(const std::string& path, const PmsData& dataIn,
                std::string& err) {
    PmsData data = dataIn;

    /* Find bounding box to centre the map (VB6 SaveAndCompile frm:2606-2612).
       VB6 uses Int(Midpoint(...)), i.e. an *integer* offset. */
    float mapWidth = 0, mapHeight = 0;
    if (!data.polys.empty()) {
        float minX = data.polys[0].poly.v[0].x, maxX = minX;
        float minY = data.polys[0].poly.v[0].y, maxY = minY;
        for (const auto& pe : data.polys)
            for (int i = 0; i < 3; ++i) {
                if (pe.poly.v[i].x < minX) minX = pe.poly.v[i].x;
                if (pe.poly.v[i].x > maxX) maxX = pe.poly.v[i].x;
                if (pe.poly.v[i].y < minY) minY = pe.poly.v[i].y;
                if (pe.poly.v[i].y > maxY) maxY = pe.poly.v[i].y;
            }
        /* SaveAndCompile calls mnuRefreshBG_Click first (frm:2604), which
           seeds min/max with 0, so the origin is always inside the bounds. */
        minX = std::min(minX, 0.0f); minY = std::min(minY, 0.0f);
        maxX = std::max(maxX, 0.0f); maxY = std::max(maxY, 0.0f);

        /* VB6 Int() floors (it is not a truncation toward zero). */
        float offX = std::floor((minX + maxX) * 0.5f);
        float offY = std::floor((minY + maxY) * 0.5f);

        /* VB6: mapWidth = maxX - xOffset (a *half* extent).  mapWidth and
           mapHeight are declared As Integer (frm:2569-2570), so the Single
           result is rounded to nearest-even on assignment -- not truncated.
           Three shipped maps (ctf_Lanubya, ctf_Voland, DesertWind) depend on
           this to reproduce their stored sectorsDivision. */
        mapWidth  = std::nearbyint(maxX - offX);
        mapHeight = std::nearbyint(maxY - offY);

        /* Centre all positions. */
        for (auto& pe : data.polys)
            for (int i = 0; i < 3; ++i) {
                pe.poly.v[i].x -= offX;
                pe.poly.v[i].y -= offY;
            }
        for (auto& prop : data.props) { prop.x -= offX; prop.y -= offY; }
        for (auto& sp   : data.spawns) { sp.x -= static_cast<int32_t>(offX);
                                         sp.y -= static_cast<int32_t>(offY); }
        for (auto& wp   : data.waypoints) { wp.x -= static_cast<int32_t>(offX);
                                            wp.y -= static_cast<int32_t>(offY); }
        for (auto& c    : data.colliders) { c.x -= offX; c.y -= offY; }
    }

    /*
     * Sector division + in-range sector counts (VB6 frm:2634-2641).
     * Verified against the shipped Soldat maps: this formula reproduces the
     * stored sectorsDivision exactly for every map in maps/.
     */
    float xSecNum = SECTOR_NUM, ySecNum = SECTOR_NUM;
    if (mapWidth > mapHeight) {
        data.sectorDiv = static_cast<int32_t>((mapWidth + 100) / 25);
        if (data.sectorDiv > 0) ySecNum = (mapHeight + 100) / data.sectorDiv;
    } else {
        data.sectorDiv = static_cast<int32_t>((mapHeight + 100) / 25);
        if (data.sectorDiv > 0) xSecNum = (mapWidth + 100) / data.sectorDiv;
    }
    if (data.sectorDiv <= 0) data.sectorDiv = 1;

    for (auto& pe : data.polys) computePolyNormals(pe);
    buildSectorTable(data, xSecNum, ySecNum);

    /* Assign a random positive ID, as VB6 does: (Rnd * 999999) + 10000. */
    {
        static std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<int32_t> dist(10000, 1009999);
        data.options.mapRandomID = dist(rng);
    }

    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) { err = "Cannot open file for writing: " + path; return false; }
    try { do_write(f, data, false); }
    catch (const std::exception& e) { err = e.what(); return false; }
    return true;
}

/* ---- In-memory helpers ------------------------------------------------- */

bool pmsDataToBytes(const PmsData& data, std::vector<uint8_t>& out,
                    std::string& err) {
    std::ostringstream ss(std::ios::binary);
    try { do_write(ss, data, true); }
    catch (const std::exception& e) { err = e.what(); return false; }
    const auto& str = ss.str();
    out.assign(str.begin(), str.end());
    return true;
}

bool pmsBytesToData(const std::vector<uint8_t>& bytes, PmsData& out,
                    std::string& err) {
    std::string str(bytes.begin(), bytes.end());
    std::istringstream ss(str, std::ios::binary);
    try { do_read(ss, out); }
    catch (const std::exception& e) { err = e.what(); return false; }
    return true;
}

/* ---- MapDocument ↔ PmsData --------------------------------------------- */

void docToPmsData(const MapDocument& doc, PmsData& out) {
    out = PmsData{};
    out.version = PMS_VERSION;

    /* Options */
    std::memset(&out.options, 0, sizeof(PmsOptions));
    pms_write_string(out.options.mapName, 39, doc.options.mapName);
    pms_write_string(out.options.textureName, 25, doc.options.textureName);
    out.options.bgColor1    = doc.options.bgColor1;
    out.options.bgColor2    = doc.options.bgColor2;
    out.options.startJet    = doc.options.startJet;
    out.options.grenadePacks= doc.options.grenadePacks;
    out.options.medikits    = doc.options.medikits;
    out.options.weather     = doc.options.weather;
    out.options.steps       = doc.options.steps;
    out.options.mapRandomID = doc.options.mapRandomID;

    /* Polys */
    out.polys.reserve(doc.polys.size());
    for (const auto& ep : doc.polys) {
        PmsPolyEntry pe{};
        for (int i = 0; i < 3; ++i) {
            pe.poly.v[i].x     = ep.v[i].world.x;
            pe.poly.v[i].y     = ep.v[i].world.y;
            /* VB6 SaveMap copies the live TPoly and only overwrites X, Y,
               colour and Perp — z/rhw are written through, preserving the
               depthmap and the hidden-polygon flag (frm:5268-5290).
               compilePms() overwrites both with 1.0 separately. */
            pe.poly.v[i].z     = ep.v[i].z;
            pe.poly.v[i].rhw   = ep.v[i].rhw;
            pe.poly.v[i].color = argb(ep.v[i].alpha,
                                      ep.v[i].r, ep.v[i].g, ep.v[i].b);
            pe.poly.v[i].tu    = ep.v[i].tu;
            pe.poly.v[i].tv    = ep.v[i].tv;
        }
        /* Edge normals, exactly as VB6 SaveMap does (frm:5279-5290):
           direction from the geometry, magnitude carrying bounciness,
           Perp.Z always written as 1. */
        for (int i = 0; i < 3; ++i) {
            int j = (i + 1) % 3;
            float xDiff = ep.v[j].world.x - ep.v[i].world.x;
            float yDiff = ep.v[i].world.y - ep.v[j].world.y;
            float len = (xDiff == 0.0f && yDiff == 0.0f)
                            ? 1.0f
                            : std::sqrt(xDiff * xDiff + yDiff * yDiff);
            pe.poly.perp.n[i].x = (yDiff / len) * ep.bounciness[i];
            pe.poly.perp.n[i].y = (xDiff / len) * ep.bounciness[i];
            pe.poly.perp.n[i].z = 1.0f;
        }
        pe.polyType = ep.polyType;
        out.polys.push_back(pe);
    }

    /* VB6 SaveMap (frm:5235-5258) recomputes sectorsDivision from the *full*
       map extents -- unlike SaveAndCompile, which uses half extents measured
       from the map centre (frm:2607-2611).  The two save paths genuinely
       disagree in the original; each is reproduced faithfully here.
       mnuRefreshBG_Click seeds min/max with 0, so the origin is always
       inside the bounds. */
    {
        float minX = 0, minY = 0, maxX = 0, maxY = 0;
        int32_t div = 1;
        if (doc.mapBounds(minX, minY, maxX, maxY)) {
            minX = std::min(minX, 0.0f); minY = std::min(minY, 0.0f);
            maxX = std::max(maxX, 0.0f); maxY = std::max(maxY, 0.0f);
            const float w = maxX - minX, h = maxY - minY;
            div = static_cast<int32_t>(((w > h ? w : h) + 100) / 25);
        }
        out.sectorDiv = div > 0 ? div : 1;
    }

    /* Scenery props */
    out.props.reserve(doc.scenery.size());
    for (const auto& es : doc.scenery) {
        PmsProp p{};
        p.active   = -1; /* VB6 True */
        p.style    = static_cast<int16_t>(es.style);
        p.width    = es.width;
        p.height   = es.height;
        p.x        = es.x;
        p.y        = es.y;
        p.rotation = es.rotation;
        p.scaleX   = es.scaleX;
        p.scaleY   = es.scaleY;
        p.alpha    = es.alpha;
        p.color    = es.color;
        p.level    = es.level;
        out.props.push_back(p);
    }

    /* Scenery names (doc.sceneryNames[0] is sentinel, skip it) */
    for (size_t i = 1; i < doc.sceneryNames.size(); ++i) {
        PmsSceneryName sn{};
        pms_write_string(sn.name, 51, doc.sceneryNames[i]);
        out.sceneryNames.push_back(sn);
    }

    /* Colliders */
    for (const auto& ec : doc.colliders) {
        PmsCollider c{};
        c.active = ec.active ? 1 : 0;
        c.x      = ec.x;
        c.y      = ec.y;
        c.radius = ec.radius;
        out.colliders.push_back(c);
    }

    /* Spawns */
    for (const auto& es : doc.spawns) {
        PmsSpawnPoint sp{};
        sp.active = es.active ? 1 : 0;
        sp.x      = static_cast<int32_t>(es.x);
        sp.y      = static_cast<int32_t>(es.y);
        sp.team   = es.team;
        out.spawns.push_back(sp);
    }

    /* Waypoints */
    for (const auto& ew : doc.waypoints) {
        PmsWaypoint wp{};
        wp.active          = ew.active ? 1 : 0;
        wp.id              = ew.id;
        wp.x               = static_cast<int32_t>(ew.x);
        wp.y               = static_cast<int32_t>(ew.y);
        wp.left            = ew.left;
        wp.right           = ew.right;
        wp.up              = ew.up;
        wp.down            = ew.down;
        wp.m2              = ew.m2;
        wp.pathNum         = static_cast<uint8_t>(ew.pathNum);
        wp.special         = static_cast<uint8_t>(ew.special);
        wp.connectionsNum  = static_cast<int32_t>(ew.connections.size());
        for (int i = 0; i < static_cast<int>(ew.connections.size()) && i < 20; ++i)
            wp.connections[i] = ew.connections[i];
        out.waypoints.push_back(wp);
    }

    /* Lights */
    for (const auto& el : doc.lights) {
        PmsLight l{};
        l.selected  = 0;
        l.color     = {el.r, el.g, el.b};
        l.intensity = el.intensity;
        l.range     = static_cast<int16_t>(el.range);
        l.x         = el.x;
        l.y         = el.y;
        l.z         = el.z;
        out.lights.push_back(l);
    }

    /* Sketch */
    for (const auto& esk : doc.sketch) {
        PmsSketchLine sk{};
        sk.v[0] = {esk.a.x, esk.a.y, 0};
        sk.v[1] = {esk.b.x, esk.b.y, 0};
        out.sketch.push_back(sk);
    }
}

void pmsDataToDoc(const PmsData& data, MapDocument& doc) {
    doc.clear();

    doc.options.mapName     = pms_read_string(data.options.mapName, 38);
    doc.options.textureName = pms_read_string(data.options.textureName, 24);
    doc.options.bgColor1    = data.options.bgColor1;
    doc.options.bgColor2    = data.options.bgColor2;
    doc.options.startJet    = data.options.startJet;
    doc.options.grenadePacks= data.options.grenadePacks;
    doc.options.medikits    = data.options.medikits;
    doc.options.weather     = data.options.weather;
    doc.options.steps       = data.options.steps;
    doc.options.mapRandomID = data.options.mapRandomID;

    doc.polys.reserve(data.polys.size());
    for (const auto& pe : data.polys) {
        EditorPoly ep{};
        for (int i = 0; i < 3; ++i) {
            ep.v[i].world.x = pe.poly.v[i].x;
            ep.v[i].world.y = pe.poly.v[i].y;
            ep.v[i].r       = argb_r(pe.poly.v[i].color);
            ep.v[i].g       = argb_g(pe.poly.v[i].color);
            ep.v[i].b       = argb_b(pe.poly.v[i].color);
            ep.v[i].alpha   = argb_a(pe.poly.v[i].color);
            ep.v[i].tu      = pe.poly.v[i].tu;
            ep.v[i].tv      = pe.poly.v[i].tv;
            /* VB6 reads the whole TPoly record, so the depthmap value and the
               hidden-polygon flag stored in z/rhw survive a load (frm:1980).
               The compiler always writes 1.0, so compiled maps are unaffected. */
            ep.v[i].z       = pe.poly.v[i].z;
            ep.v[i].rhw     = pe.poly.v[i].rhw;
        }
        /* VB6 recovers bounciness from the normal's magnitude on load
           (frm:1988: Perp.vertex(j).Z = Sqr(X^2 + Y^2)). */
        for (int i = 0; i < 3; ++i) {
            const PmsNormal& n = pe.poly.perp.n[i];
            float mag = std::sqrt(n.x * n.x + n.y * n.y);
            ep.bounciness[i] = (mag > 0.0f) ? mag : 1.0f;
        }
        ep.polyType = pe.polyType;
        doc.polys.push_back(ep);
    }

    /* Scenery names (1-based; [0] is sentinel) */
    for (const auto& sn : data.sceneryNames)
        doc.sceneryNames.push_back(pms_read_string(sn.name, 50));

    /* Scenery props.  frm:2019-2050 validates each prop and silently drops the
       broken ones; note the `active` flag is deliberately NOT consulted - the
       original keys validity off Style/coordinates/scale alone, so honouring
       `active` here would discard scenery that PolyWorks itself loads. */
    doc.scenery.reserve(data.props.size());
    for (const auto& p : data.props) {
        if (p.x > 32766.0f || p.x < -32766.0f || p.y > 32766.0f || p.y < -32766.0f) continue;
        if (p.width < 0 || p.height < 0) continue;
        if (static_cast<int>(p.scaleX * 1000.0f) == 0 ||
            static_cast<int>(p.scaleY * 1000.0f) == 0) continue;
        if (p.scaleX < -10000.0f || p.scaleX > 10000.0f ||
            p.scaleY < -10000.0f || p.scaleY > 10000.0f) continue;
        if (p.style < 1) continue;

        EditorScenery es{};
        es.style    = p.style;
        es.x        = p.x;
        es.y        = p.y;
        es.rotation = p.rotation;
        es.scaleX   = p.scaleX;
        es.scaleY   = p.scaleY;
        es.width    = p.width;
        es.height   = p.height;
        /* frm:2037: out-of-range alpha becomes fully opaque. */
        es.alpha    = (p.alpha < 1 || p.alpha > 255) ? 255
                                                     : static_cast<uint8_t>(p.alpha);
        es.color    = p.color;
        es.level    = (p.level >= 0 && p.level <= 255) ? p.level : 0;
        doc.scenery.push_back(es);
    }

    /* Colliders */
    for (const auto& c : data.colliders) {
        EditorCollider ec{};
        ec.active = c.active != 0;
        ec.x = c.x; ec.y = c.y; ec.radius = c.radius;
        doc.colliders.push_back(ec);
    }

    /* Spawns */
    for (const auto& sp : data.spawns) {
        EditorSpawn es{};
        es.active = sp.active != 0;
        es.x = static_cast<float>(sp.x);
        es.y = static_cast<float>(sp.y);
        es.team = sp.team;
        doc.spawns.push_back(es);
    }

    /* Waypoints */
    for (const auto& wp : data.waypoints) {
        EditorWaypoint ew{};
        ew.active  = wp.active != 0;
        ew.id      = wp.id;
        ew.x       = static_cast<float>(wp.x);
        ew.y       = static_cast<float>(wp.y);
        ew.left    = wp.left != 0;
        ew.right   = wp.right != 0;
        ew.up      = wp.up != 0;
        ew.down    = wp.down != 0;
        ew.m2      = wp.m2 != 0;
        ew.pathNum = wp.pathNum;
        ew.special = wp.special;
        for (int i = 0; i < wp.connectionsNum && i < 20; ++i)
            ew.connections.push_back(wp.connections[i]);
        doc.waypoints.push_back(ew);
    }

    /* Lights */
    for (const auto& l : data.lights) {
        EditorLight el{};
        el.r         = l.color.r;
        el.g         = l.color.g;
        el.b         = l.color.b;
        el.intensity = l.intensity;
        el.range     = l.range;
        el.x = l.x; el.y = l.y; el.z = l.z;
        doc.lights.push_back(el);
    }

    /* Sketch */
    for (const auto& sk : data.sketch) {
        EditorSketchLine esk{};
        esk.a = {sk.v[0].x, sk.v[0].y};
        esk.b = {sk.v[1].x, sk.v[1].y};
        doc.sketch.push_back(esk);
    }
}

/* ---- Prefab save / load (VB6-compatible .pwf format) ------------------- */
/*
 * PWF binary format (VB6 SavePrefab / LoadPrefab):
 *   int32   numSelectedPolys
 *   For each selected poly:
 *     PmsPolygon   (with world XY coordinates)
 *     3 × uint8    (vertex selection flags, always 1 in saved output)
 *     uint8        polyType (same as PmsPolyEntry.polyType)
 *   int32   numSelectedScenery
 *   For each selected scenery:
 *     PmsProp      (44 bytes)
 *     uint8[51]    scenery name (length-prefixed byte string, same as PmsSceneryName.data)
 *   int32   numSelColliders
 *   For each selected collider:
 *     PmsCollider  (16 bytes)
 *   int32   numSelSpawns
 *   For each selected spawn:
 *     PmsSpawnPoint (16 bytes)
 *   int32   numSelWaypoints
 *   For each selected waypoint:
 *     PmsWaypoint  (112 bytes)
 *   int32   numSelConnections
 *   For each selected connection:
 *     int16  point1 (local 1-based index among saved waypoints)
 *     int16  point2
 */

bool savePrefab(const std::string& path, const MapDocument& doc,
                std::string& err) {
    std::ofstream f(path, std::ios::binary);
    if (!f) { err = "Cannot open file for writing: " + path; return false; }

    try {
        /* ------ Polygons ------ */
        /* Gather selected polys */
        std::vector<int> selPolyIdx;
        for (int i = 0; i < static_cast<int>(doc.polys.size()); ++i)
            if (doc.polys[i].anySelected()) selPolyIdx.push_back(i);

        int32_t nPolys = static_cast<int32_t>(selPolyIdx.size());
        write_raw(f, nPolys);

        for (int i : selPolyIdx) {
            const EditorPoly& ep = doc.polys[i];
            /* Build a PmsPolygon from the EditorPoly */
            PmsPolygon pg{};
            for (int j = 0; j < 3; ++j) {
                pg.v[j].x   = ep.v[j].world.x;
                pg.v[j].y   = ep.v[j].world.y;
                pg.v[j].z   = 1.0f;
                pg.v[j].rhw = 1.0f;
                pg.v[j].color = (static_cast<uint32_t>(ep.v[j].alpha) << 24) |
                                (static_cast<uint32_t>(ep.v[j].r)     << 16) |
                                (static_cast<uint32_t>(ep.v[j].g)     <<  8) |
                                 static_cast<uint32_t>(ep.v[j].b);
                pg.v[j].tu  = ep.v[j].tu;
                pg.v[j].tv  = ep.v[j].tv;
            }
            write_raw(f, pg);
            /* Vertex-selected flags (always 1 when saving) */
            uint8_t sel = 1;
            write_raw(f, sel); write_raw(f, sel); write_raw(f, sel);
            uint8_t pt = static_cast<uint8_t>(ep.polyType);
            write_raw(f, pt);
        }

        /* ------ Scenery ------ */
        std::vector<int> selScenIdx;
        for (int i = 0; i < static_cast<int>(doc.scenery.size()); ++i)
            if (doc.scenery[i].selected) selScenIdx.push_back(i);

        int32_t nScen = static_cast<int32_t>(selScenIdx.size());
        write_raw(f, nScen);

        for (int i : selScenIdx) {
            const EditorScenery& es = doc.scenery[i];
            PmsProp pp{};
            pp.active   = -1;  /* EditorScenery has no active flag; always true */
            pp.style    = static_cast<int16_t>(es.style);
            pp.width    = es.width;
            pp.height   = es.height;
            pp.x        = es.x;
            pp.y        = es.y;
            pp.rotation = es.rotation;
            pp.scaleX   = es.scaleX;
            pp.scaleY   = es.scaleY;
            pp.alpha    = es.alpha;
            pp.color    = es.color;
            pp.level    = es.level;
            write_raw(f, pp);
            /* Scenery name: 51-byte length-prefixed array */
            const std::string& name = (es.style >= 1 &&
                es.style < static_cast<int>(doc.sceneryNames.size()))
                ? doc.sceneryNames[static_cast<size_t>(es.style)] : "";
            uint8_t nameBuf[51]{};
            uint8_t len = static_cast<uint8_t>(std::min(name.size(), size_t(50)));
            nameBuf[0] = len;
            std::memcpy(nameBuf + 1, name.c_str(), len);
            write_raw_buf(f, nameBuf, 51);
        }

        /* ------ Colliders ------ */
        std::vector<int> selCollIdx;
        for (int i = 0; i < static_cast<int>(doc.colliders.size()); ++i)
            if (doc.colliders[i].selected) selCollIdx.push_back(i);
        int32_t nColl = static_cast<int32_t>(selCollIdx.size());
        write_raw(f, nColl);
        for (int i : selCollIdx) {
            const EditorCollider& ec = doc.colliders[i];
            PmsCollider pc{};
            pc.active = ec.active ? -1 : 0;
            pc.x = ec.x; pc.y = ec.y; pc.radius = ec.radius;
            write_raw(f, pc);
        }

        /* ------ Spawns ------ */
        std::vector<int> selSpawnIdx;
        for (int i = 0; i < static_cast<int>(doc.spawns.size()); ++i)
            if (doc.spawns[i].selected) selSpawnIdx.push_back(i);
        int32_t nSpawn = static_cast<int32_t>(selSpawnIdx.size());
        write_raw(f, nSpawn);
        for (int i : selSpawnIdx) {
            const EditorSpawn& es = doc.spawns[i];
            PmsSpawnPoint ps{};
            ps.active = es.active ? -1 : 0;
            ps.x = static_cast<int32_t>(es.x);
            ps.y = static_cast<int32_t>(es.y);
            ps.team = es.team;
            write_raw(f, ps);
        }

        /* ------ Waypoints ------ */
        /* Assign local 1-based indices to selected waypoints */
        std::vector<int> selWpIdx;
        std::map<int,int> wpLocalId;  /* waypoint.id → local 1-based index */
        int localIdx = 1;
        for (int i = 0; i < static_cast<int>(doc.waypoints.size()); ++i) {
            if (doc.waypoints[i].selected) {
                selWpIdx.push_back(i);
                wpLocalId[doc.waypoints[i].id] = localIdx++;
            }
        }
        int32_t nWp = static_cast<int32_t>(selWpIdx.size());
        write_raw(f, nWp);
        for (int i : selWpIdx) {
            const EditorWaypoint& ew = doc.waypoints[i];
            PmsWaypoint pw{};
            pw.active       = ew.active ? -1 : 0;
            pw.id           = ew.id;
            pw.x            = static_cast<int32_t>(ew.x);
            pw.y            = static_cast<int32_t>(ew.y);
            pw.left         = ew.left ? 1 : 0;
            pw.right        = ew.right ? 1 : 0;
            pw.up           = ew.up ? 1 : 0;
            pw.down         = ew.down ? 1 : 0;
            pw.m2           = ew.m2 ? 1 : 0;
            pw.pathNum      = static_cast<uint8_t>(ew.pathNum);
            pw.special      = static_cast<uint8_t>(ew.special);
            /* crap[5] padding bytes — zero-fill */
            int nc = 0;
            for (int conn : ew.connections) {
                if (nc >= 20) break;
                if (wpLocalId.count(conn))
                    pw.connections[nc++] = wpLocalId[conn];
            }
            pw.connectionsNum = nc;
            write_raw(f, pw);
        }

        /* ------ Connections ------ */
        std::vector<std::pair<int16_t,int16_t>> selConns;
        for (const auto& wp : doc.waypoints) {
            if (!wp.selected) continue;
            for (int connId : wp.connections) {
                if (!wpLocalId.count(connId)) continue;
                /* Only save connection if other endpoint is also selected */
                bool otherSel = false;
                for (const auto& w2 : doc.waypoints)
                    if (w2.id == connId && w2.selected) { otherSel = true; break; }
                if (!otherSel) continue;
                int16_t p1 = static_cast<int16_t>(wpLocalId[wp.id]);
                int16_t p2 = static_cast<int16_t>(wpLocalId[connId]);
                if (p1 < p2)  /* avoid duplicates */
                    selConns.push_back({p1, p2});
            }
        }
        /* Deduplicate */
        std::sort(selConns.begin(), selConns.end());
        selConns.erase(std::unique(selConns.begin(), selConns.end()), selConns.end());

        int32_t nConn = static_cast<int32_t>(selConns.size());
        write_raw(f, nConn);
        for (auto [p1, p2] : selConns) {
            write_raw(f, p1);
            write_raw(f, p2);
        }

    } catch (const std::exception& e) {
        err = e.what(); return false;
    }
    return true;
}

bool loadPrefab(const std::string& path, MapDocument& doc,
                std::string& err) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { err = "Cannot open file: " + path; return false; }

    /* Deselect everything first */
    doc.clearSelection();

    try {
        /* ------ Polygons ------ */
        int32_t nPolys = 0;
        read_raw(f, nPolys);
        for (int32_t i = 0; i < nPolys; ++i) {
            PmsPolygon pg{};
            read_raw(f, pg);
            uint8_t vsel[3]{}, pt = 0;
            read_raw(f, vsel[0]); read_raw(f, vsel[1]); read_raw(f, vsel[2]);
            read_raw(f, pt);

            EditorPoly ep{};
            ep.polyType = pt;
            for (int j = 0; j < 3; ++j) {
                ep.v[j].world.x = pg.v[j].x;
                ep.v[j].world.y = pg.v[j].y;
                ep.v[j].alpha   = static_cast<uint8_t>((pg.v[j].color >> 24) & 0xff);
                ep.v[j].r       = static_cast<uint8_t>((pg.v[j].color >> 16) & 0xff);
                ep.v[j].g       = static_cast<uint8_t>((pg.v[j].color >>  8) & 0xff);
                ep.v[j].b       = static_cast<uint8_t>( pg.v[j].color        & 0xff);
                ep.v[j].tu      = pg.v[j].tu;
                ep.v[j].tv      = pg.v[j].tv;
                ep.v[j].selected = true;
            }
            doc.polys.push_back(ep);
        }

        /* ------ Scenery ------ */
        int32_t nScen = 0;
        read_raw(f, nScen);
        for (int32_t i = 0; i < nScen; ++i) {
            PmsProp pp{};
            read_raw(f, pp);
            uint8_t nameBuf[51]{};
            read_raw_buf(f, nameBuf, 51);
            uint8_t len = nameBuf[0];
            if (len > 50) len = 50;
            std::string name(reinterpret_cast<char*>(nameBuf + 1), len);

            /* Find or add scenery name */
            int nameIdx = -1;
            for (int j = 1; j < static_cast<int>(doc.sceneryNames.size()); ++j)
                if (doc.sceneryNames[j] == name) { nameIdx = j; break; }
            if (nameIdx < 0) {
                nameIdx = static_cast<int>(doc.sceneryNames.size());
                doc.sceneryNames.push_back(name);
            }

            EditorScenery es{};
            /* active: EditorScenery has no active flag, but track for future use */
            es.style    = nameIdx;
            es.width    = pp.width;
            es.height   = pp.height;
            es.x        = pp.x;
            es.y        = pp.y;
            es.rotation = pp.rotation;
            es.scaleX   = pp.scaleX;
            es.scaleY   = pp.scaleY;
            es.alpha    = pp.alpha == 0 ? 255 : static_cast<uint8_t>(pp.alpha);
            es.color    = pp.color;
            es.level    = pp.level;
            es.selected = true;
            doc.scenery.push_back(es);
        }

        /* ------ Colliders ------ */
        int32_t nColl = 0;
        read_raw(f, nColl);
        for (int32_t i = 0; i < nColl; ++i) {
            PmsCollider pc{};
            read_raw(f, pc);
            EditorCollider ec{};
            ec.active   = pc.active != 0;
            ec.x        = pc.x; ec.y = pc.y; ec.radius = pc.radius;
            ec.selected = true;
            doc.colliders.push_back(ec);
        }

        /* ------ Spawns ------ */
        int32_t nSpawn = 0;
        read_raw(f, nSpawn);
        for (int32_t i = 0; i < nSpawn; ++i) {
            PmsSpawnPoint ps{};
            read_raw(f, ps);
            EditorSpawn es{};
            es.active   = ps.active != 0;
            es.x        = static_cast<float>(ps.x);
            es.y        = static_cast<float>(ps.y);
            es.team     = static_cast<uint8_t>(ps.team);
            es.selected = true;
            doc.spawns.push_back(es);
        }

        /* ------ Waypoints ------ */
        /* Determine ID offset: loaded waypoints get IDs starting after existing max */
        int maxId = 0;
        for (const auto& wp : doc.waypoints)
            maxId = std::max(maxId, wp.id);

        int32_t nWp = 0;
        read_raw(f, nWp);
        std::vector<int> loadedWpNewIds;  /* maps local-1-based → new doc id */
        loadedWpNewIds.push_back(0);  /* sentinel for index 0 */
        for (int32_t i = 0; i < nWp; ++i) {
            PmsWaypoint pw{};
            read_raw(f, pw);
            EditorWaypoint ew{};
            ew.id       = ++maxId;
            ew.x        = static_cast<float>(pw.x);
            ew.y        = static_cast<float>(pw.y);
            ew.left     = pw.left != 0;
            ew.right    = pw.right != 0;
            ew.up       = pw.up != 0;
            ew.down     = pw.down != 0;
            ew.m2       = pw.m2 != 0;
            ew.pathNum  = pw.pathNum;
            ew.special  = pw.special;
            ew.active   = pw.active != 0;
            ew.selected = true;
            loadedWpNewIds.push_back(ew.id);
            doc.waypoints.push_back(ew);
        }

        /* ------ Connections ------ */
        int32_t nConn = 0;
        read_raw(f, nConn);
        const int wpStart = static_cast<int>(doc.waypoints.size()) - nWp;
        for (int32_t i = 0; i < nConn; ++i) {
            int16_t p1 = 0, p2 = 0;
            read_raw(f, p1); read_raw(f, p2);
            /* Remap local 1-based to new document IDs */
            if (p1 >= 1 && p1 <= nWp && p2 >= 1 && p2 <= nWp) {
                int newId1 = loadedWpNewIds[static_cast<size_t>(p1)];
                int newId2 = loadedWpNewIds[static_cast<size_t>(p2)];
                /* Add connection both ways (match VB6 bidirectional model) */
                doc.waypoints[wpStart + p1 - 1].connections.push_back(newId2);
                doc.waypoints[wpStart + p2 - 1].connections.push_back(newId1);
            }
        }

    } catch (const std::exception& e) {
        err = e.what(); return false;
    }

    doc.rebuildScreenCache();
    doc.markModified();
    return true;
}
