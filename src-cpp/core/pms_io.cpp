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

static void computePolyNormals(PmsPolyEntry& entry) {
    /* Recompute edge normals from vertex positions (VB6 SaveAndCompile does this). */
    for (int i = 0; i < 3; ++i) {
        int j = (i + 1) % 3;
        float dx = entry.poly.v[j].x - entry.poly.v[i].x;
        float dy = entry.poly.v[j].y - entry.poly.v[i].y;
        float len = std::sqrt(dx*dx + dy*dy);
        if (len < 1e-6f) len = 1e-6f;
        float bounciness = 1.0f;
        if (entry.polyType == POLY_BOUNCY)
            bounciness = entry.poly.perp.n[i].z; /* preserve existing */
        entry.poly.perp.n[i].x = dy  / len * bounciness;
        entry.poly.perp.n[i].y = -dx / len * bounciness;
        entry.poly.perp.n[i].z = bounciness;
    }
}

static void buildSectorTable(PmsData& d) {
    for (int i = 0; i < SECTOR_CELLS; ++i)
        for (int j = 0; j < SECTOR_CELLS; ++j) {
            d.sectors[i][j].polyCount = 0;
            d.sectors[i][j].polyIndex.clear();
        }

    float div = static_cast<float>(d.sectorDiv ? d.sectorDiv : 1);
    for (int pi = 0; pi < static_cast<int>(d.polys.size()); ++pi) {
        const auto& poly = d.polys[pi].poly;
        float minX = poly.v[0].x, maxX = poly.v[0].x;
        float minY = poly.v[0].y, maxY = poly.v[0].y;
        for (int vi = 1; vi < 3; ++vi) {
            if (poly.v[vi].x < minX) minX = poly.v[vi].x;
            if (poly.v[vi].x > maxX) maxX = poly.v[vi].x;
            if (poly.v[vi].y < minY) minY = poly.v[vi].y;
            if (poly.v[vi].y > maxY) maxY = poly.v[vi].y;
        }
        int secMinX = static_cast<int>(std::floor(minX / div)) + SECTOR_NUM;
        int secMaxX = static_cast<int>(std::ceil (maxX / div)) + SECTOR_NUM;
        int secMinY = static_cast<int>(std::floor(minY / div)) + SECTOR_NUM;
        int secMaxY = static_cast<int>(std::ceil (maxY / div)) + SECTOR_NUM;
        secMinX = std::max(0, std::min(SECTOR_CELLS-1, secMinX));
        secMaxX = std::max(0, std::min(SECTOR_CELLS-1, secMaxX));
        secMinY = std::max(0, std::min(SECTOR_CELLS-1, secMinY));
        secMaxY = std::max(0, std::min(SECTOR_CELLS-1, secMaxY));
        for (int si = secMinX; si <= secMaxX; ++si)
            for (int sj = secMinY; sj <= secMaxY; ++sj) {
                d.sectors[si][sj].polyIndex.push_back(pi);
                d.sectors[si][sj].polyCount++;
            }
    }
}

bool compilePms(const std::string& path, const PmsData& dataIn,
                std::string& err) {
    PmsData data = dataIn;

    /* Find bounding box to centre the map. */
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
        float offX = (minX + maxX) * 0.5f;
        float offY = (minY + maxY) * 0.5f;

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

    /* Compute sector division based on extents. */
    if (data.sectorDiv <= 0) data.sectorDiv = 25;
    for (auto& pe : data.polys) computePolyNormals(pe);
    buildSectorTable(data);

    /* Assign a positive random ID. */
    data.options.mapRandomID = 1;

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
            pe.poly.v[i].z     = 1.0f;
            pe.poly.v[i].rhw   = 1.0f;
            pe.poly.v[i].color = argb(ep.v[i].alpha,
                                      ep.v[i].r, ep.v[i].g, ep.v[i].b);
            pe.poly.v[i].tu    = ep.v[i].tu;
            pe.poly.v[i].tv    = ep.v[i].tv;
        }
        pe.polyType = ep.polyType;
        out.polys.push_back(pe);
    }

    out.sectorDiv = 1;

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
        }
        ep.polyType = pe.polyType;
        doc.polys.push_back(ep);
    }

    /* Scenery names (1-based; [0] is sentinel) */
    for (const auto& sn : data.sceneryNames)
        doc.sceneryNames.push_back(pms_read_string(sn.name, 50));

    /* Scenery props */
    doc.scenery.reserve(data.props.size());
    for (const auto& p : data.props) {
        if (!p.active) continue;
        EditorScenery es{};
        es.style    = p.style;
        es.x        = p.x;
        es.y        = p.y;
        es.rotation = p.rotation;
        es.scaleX   = p.scaleX;
        es.scaleY   = p.scaleY;
        es.width    = p.width;
        es.height   = p.height;
        es.alpha    = static_cast<uint8_t>(p.alpha);
        es.color    = p.color;
        es.level    = p.level;
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
