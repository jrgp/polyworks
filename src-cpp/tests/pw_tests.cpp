/*
 * pw_tests.cpp — Headless test suite for PolyWorks core.
 *
 * Simple hand-rolled runner — no external test framework required.
 * Each TEST() macro registers a function; main() runs them and reports.
 *
 * Tests cover:
 *   - struct sizes (binary compatibility)
 *   - pms_types helpers
 *   - geometry operations
 *   - PMS round-trips on all maps in maps/
 *   - MapDocument editing
 *   - UndoStack
 */

#include "pms_types.h"
#include "pms_io.h"
#include "map_document.h"
#include "undo_stack.h"
#include "geometry.h"
#include "color_key.h"
#include "texture_manager.h"

#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <functional>
#include <filesystem>
#include <sstream>
#include <algorithm>

namespace fs = std::filesystem;

/* ---- Minimal test harness ---------------------------------------------- */

static int g_pass = 0, g_fail = 0;
static std::string g_currentTest;

static void test_pass(const char* expr, const char* file, int line) {
    (void)expr; (void)file; (void)line;
    ++g_pass;
}
static void test_fail(const char* expr, const char* file, int line,
                      const std::string& extra = "") {
    ++g_fail;
    std::fprintf(stderr, "FAIL [%s] %s:%d  %s%s\n",
                 g_currentTest.c_str(), file, line, expr,
                 extra.empty() ? "" : ("  // " + extra).c_str());
}

#define EXPECT(cond) \
    do { if (cond) test_pass(#cond,__FILE__,__LINE__); \
         else      test_fail(#cond,__FILE__,__LINE__); } while(0)

#define EXPECT_EQ(a,b) \
    do { auto _a=(a); auto _b=(b); \
         if (_a==_b) test_pass(#a "==" #b,__FILE__,__LINE__); \
         else { std::ostringstream _ss; _ss<<#a<<"="<<_a<<" "#b<<"="<<_b; \
                test_fail(#a "==" #b,__FILE__,__LINE__,_ss.str()); } } while(0)

#define EXPECT_NEAR(a,b,eps) \
    do { float _a=(a), _b=(b), _e=(eps); \
         if (std::fabs(_a-_b)<=_e) test_pass(#a "~=" #b,__FILE__,__LINE__); \
         else { std::ostringstream _ss; _ss<<_a<<" vs "<<_b<<" eps "<<_e; \
                test_fail(#a "~=" #b,__FILE__,__LINE__,_ss.str()); } } while(0)

using TestFn = std::function<void()>;
struct TestCase { std::string name; TestFn fn; };
static std::vector<TestCase> g_tests;

static void register_test(const char* name, TestFn fn) {
    g_tests.push_back({name, fn});
}

#define TEST(name) \
    static void test_##name(); \
    static bool _reg_##name = (register_test(#name, test_##name), true); \
    static void test_##name()

/* ---- Struct size assertions -------------------------------------------- */

TEST(struct_sizes) {
    EXPECT_EQ(sizeof(PmsVertex),     28u);
    EXPECT_EQ(sizeof(PmsNormal),     12u);
    EXPECT_EQ(sizeof(PmsPolyNormals),36u);
    EXPECT_EQ(sizeof(PmsPolygon),   120u);
    EXPECT_EQ(sizeof(PmsPolyEntry), 121u);
    EXPECT_EQ(sizeof(PmsProp),       44u);
    EXPECT_EQ(sizeof(PmsSceneryName),55u);
    EXPECT_EQ(sizeof(PmsCollider),   16u);
    EXPECT_EQ(sizeof(PmsSpawnPoint), 16u);
    EXPECT_EQ(sizeof(PmsWaypoint),  112u);
    EXPECT_EQ(sizeof(Color3),         3u);
    EXPECT_EQ(sizeof(PmsLight),      22u);
    EXPECT_EQ(sizeof(PmsSketchLine), 24u);
    EXPECT_EQ(sizeof(PmsOptions),    84u);
}

/* ---- ARGB helpers ------------------------------------------------------- */

TEST(argb_helpers) {
    uint32_t c = argb(0xAA, 0x11, 0x22, 0x33);
    EXPECT_EQ(argb_a(c), 0xAAu);
    EXPECT_EQ(argb_r(c), 0x11u);
    EXPECT_EQ(argb_g(c), 0x22u);
    EXPECT_EQ(argb_b(c), 0x33u);

    EXPECT_EQ(argb(255,255,255,255), 0xFFFFFFFFu);
    EXPECT_EQ(argb(0,0,0,0),         0x00000000u);
}

/* ---- String helpers ----------------------------------------------------- */

TEST(pms_string_roundtrip) {
    uint8_t buf[39] = {};
    pms_write_string(buf, 39, "TestMap");
    std::string s = pms_read_string(buf, 38);
    EXPECT_EQ(s, std::string("TestMap"));

    pms_write_string(buf, 39, "");
    s = pms_read_string(buf, 38);
    EXPECT_EQ(s, std::string(""));

    /* Truncation: write a 50-char string into a 10-char buffer */
    uint8_t buf2[10] = {};
    pms_write_string(buf2, 10, "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    EXPECT_EQ(buf2[0], 9u);  /* max len = bufSize-1 = 9 */
}

/* ---- Geometry: snapToGrid ---------------------------------------------- */

TEST(snap_to_grid) {
    EXPECT_NEAR(snapToGrid(14.0f, 10.0f), 10.0f, 1e-4f);
    EXPECT_NEAR(snapToGrid(16.0f, 10.0f), 20.0f, 1e-4f);
    EXPECT_NEAR(snapToGrid(15.0f, 10.0f), 20.0f, 1e-4f);  /* round-half-up */
    EXPECT_NEAR(snapToGrid(-6.0f, 10.0f), -10.0f, 1e-4f);
    EXPECT_NEAR(snapToGrid( 0.0f, 10.0f),   0.0f, 1e-4f);
}

/* ---- Geometry: pointInPoly -------------------------------------------- */

TEST(point_in_poly) {
    /* CW triangle: (0,0), (100,0), (50,100) */
    PmsVertex cw[3];
    cw[0] = {0,0,1,1,0xFFFFFFFF,0,0};
    cw[1] = {100,0,1,1,0xFFFFFFFF,0,0};
    cw[2] = {50,100,1,1,0xFFFFFFFF,0,0};

    EXPECT( pointInPoly(50, 50, cw));   /* centroid-ish */
    EXPECT(!pointInPoly(200, 50, cw));  /* outside */
    EXPECT(!pointInPoly(-1, -1, cw));   /* outside */
}

/* ---- Geometry: isPolyClockwise ---------------------------------------- */

TEST(poly_winding) {
    PmsVertex cw[3], ccw[3];
    /* CW in Y-down space: going right then down */
    cw[0] = {0,0,1,1,0,0,0};
    cw[1] = {100,0,1,1,0,0,0};
    cw[2] = {50,100,1,1,0,0,0};

    /* CCW: reverse order */
    ccw[0] = {50,100,1,1,0,0,0};
    ccw[1] = {100,0,1,1,0,0,0};
    ccw[2] = {0,0,1,1,0,0,0};

    EXPECT( isPolyClockwise(cw));
    EXPECT(!isPolyClockwise(ccw));
}

/* ---- Geometry: segmentsIntersect --------------------------------------- */

TEST(segments_intersect) {
    /* Crossing: (0,0)-(1,1) and (0,1)-(1,0) */
    EXPECT( segmentsIntersect(0,0,1,1, 0,1,1,0));
    /* Parallel: not intersecting */
    EXPECT(!segmentsIntersect(0,0,1,0, 0,1,1,1));
    /* T-intersection */
    EXPECT( segmentsIntersect(0,0,2,0, 1,-1,1,1));
    /* Non-intersecting diagonal */
    EXPECT(!segmentsIntersect(0,0,1,0, 2,0,3,1));
}

/* ---- Geometry: snapZoom ------------------------------------------------ */

TEST(snap_zoom) {
    /* From 1.0, zoom in one step → 2.0 */
    float z = snapZoom(1.0f, 1);
    EXPECT_NEAR(z, 2.0f, 1e-4f);

    /* From 1.0, zoom out one step → 0.5 */
    z = snapZoom(1.0f, -1);
    EXPECT_NEAR(z, 0.5f, 1e-4f);

    /* Already at max, zoom in → still max */
    z = snapZoom(512.0f, 1);
    EXPECT_NEAR(z, 512.0f, 1e-4f);

    /* Already at min, zoom out → still min */
    z = snapZoom(0.03125f, -1);
    EXPECT_NEAR(z, 0.03125f, 1e-4f);
}

/* ---- Geometry: worldToScreen / screenToWorld --------------------------- */

TEST(coord_conversion) {
    float sx, sy, wx, wy;
    worldToScreen(100, 200, 50, 80, 2.0f, sx, sy);
    EXPECT_NEAR(sx, (100-50)*2.0f, 1e-4f);
    EXPECT_NEAR(sy, (200-80)*2.0f, 1e-4f);

    screenToWorld(sx, sy, 50, 80, 2.0f, wx, wy);
    EXPECT_NEAR(wx, 100.0f, 1e-4f);
    EXPECT_NEAR(wy, 200.0f, 1e-4f);
}

/* ---- Geometry: inSelRect ---------------------------------------------- */

TEST(sel_rect) {
    EXPECT( inSelRect(5, 5, 0, 0, 10, 10));   /* inside */
    EXPECT(!inSelRect(0, 5, 0, 0, 10, 10));   /* on left edge — exclusive */
    EXPECT(!inSelRect(10, 5, 0, 0, 10, 10));  /* on right edge */
    EXPECT(!inSelRect(-1, 5, 0, 0, 10, 10));  /* outside */
    /* inverted rect coords — should still work */
    EXPECT( inSelRect(5, 5, 10, 10, 0, 0));
}

/* ---- MapDocument basics ----------------------------------------------- */

TEST(map_document_clear) {
    MapDocument doc;
    EXPECT(doc.polys.empty());
    EXPECT(doc.scenery.empty());
    EXPECT(doc.spawns.empty());
    EXPECT(doc.colliders.empty());
    EXPECT(doc.waypoints.empty());
    EXPECT(doc.lights.empty());
    EXPECT(doc.sketch.empty());
    EXPECT_NEAR(doc.zoom, 1.0f, 1e-6f);
    EXPECT(!doc.modified);
    /* sceneryNames[0] is sentinel */
    EXPECT_EQ(doc.sceneryNames.size(), 1u);
    EXPECT_EQ(doc.sceneryNames[0], std::string(""));
}

TEST(map_document_add_poly) {
    MapDocument doc;
    EditorPoly p{};
    p.v[0].world = {0, 0};
    p.v[1].world = {100, 0};
    p.v[2].world = {50, 100};
    p.polyType = POLY_NORMAL;
    int idx = doc.addPoly(p);
    EXPECT_EQ(idx, 0);
    EXPECT_EQ(doc.polys.size(), 1u);
    EXPECT(doc.modified);
}

TEST(map_document_delete_selected) {
    MapDocument doc;
    EditorPoly p{};
    p.v[0].world = {0, 0};
    p.v[1].world = {100, 0};
    p.v[2].world = {50, 100};
    p.v[0].selected = p.v[1].selected = p.v[2].selected = true;
    doc.addPoly(p);
    doc.deleteSelected();
    EXPECT_EQ(doc.polys.size(), 0u);
}

TEST(map_document_partial_delete) {
    /* Only 2 of 3 vertices selected → poly NOT deleted (VB6 behavior) */
    MapDocument doc;
    EditorPoly p{};
    p.v[0].world = {0, 0};
    p.v[1].world = {100, 0};
    p.v[2].world = {50, 100};
    p.v[0].selected = p.v[1].selected = true;
    p.v[2].selected = false;
    doc.addPoly(p);
    doc.deleteSelected();
    EXPECT_EQ(doc.polys.size(), 1u);  /* poly survives */
}

TEST(map_document_move_selected) {
    MapDocument doc;
    EditorPoly p{};
    p.v[0].world = {10, 20};
    p.v[1].world = {110, 20};
    p.v[2].world = {60, 120};
    p.v[0].selected = p.v[1].selected = p.v[2].selected = true;
    doc.addPoly(p);
    doc.moveSelected(5.0f, -3.0f);
    EXPECT_NEAR(doc.polys[0].v[0].world.x, 15.0f, 1e-4f);
    EXPECT_NEAR(doc.polys[0].v[0].world.y, 17.0f, 1e-4f);
}

TEST(map_document_screen_cache) {
    MapDocument doc;
    doc.scrollX = 10; doc.scrollY = 20; doc.zoom = 2.0f;
    EditorPoly p{};
    p.v[0].world = {50, 70};
    p.v[1].world = {150, 70};
    p.v[2].world = {100, 170};
    doc.addPoly(p);
    doc.rebuildScreenCache();
    EXPECT_NEAR(doc.polys[0].v[0].screen.x, (50-10)*2.0f, 1e-4f);
    EXPECT_NEAR(doc.polys[0].v[0].screen.y, (70-20)*2.0f, 1e-4f);
}

/* ---- PMS round-trip on all maps in maps/ ------------------------------- */

/* The count-only check below is much weaker than its name suggests: it passed
   throughout every edge-normal, sector-table and bounciness bug found by the
   forensic audit.  This test does a strict *editor* round-trip instead —
   file -> MapDocument -> file -> MapDocument — and compares every field the
   editor is responsible for preserving, across all shipped maps. */
TEST(editor_roundtrip_preserves_all_maps) {
    const std::string mapsDir = "maps";
    if (!fs::exists(mapsDir)) {
        std::fprintf(stderr, "  [SKIP] maps/ directory not found\n");
        return;
    }

    int checked = 0, bad = 0;
    for (const auto& entry : fs::directory_iterator(mapsDir)) {
        auto ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext != ".pms") continue;

        const std::string path = entry.path().string();
        PmsData in;
        std::string err;
        if (loadPmsFile(path, in, err) != PmsLoadResult::OK) continue;

        MapDocument a;
        pmsDataToDoc(in, a);

        PmsData mid;
        docToPmsData(a, mid);

        std::vector<uint8_t> bytes;
        if (!pmsDataToBytes(mid, bytes, err)) {
            ++bad;
            test_fail(("serialize: " + path).c_str(), __FILE__, __LINE__, err);
            continue;
        }

        PmsData back;
        if (!pmsBytesToData(bytes, back, err)) {
            ++bad;
            test_fail(("reload: " + path).c_str(), __FILE__, __LINE__, err);
            continue;
        }

        MapDocument b;
        pmsDataToDoc(back, b);

        bool ok = true;
        auto note = [&](const char* what) {
            if (!ok) return;
            ok = false;
            ++bad;
            test_fail((path + ": " + what).c_str(), __FILE__, __LINE__, "");
        };

        if (a.polys.size() != b.polys.size())         note("polygon count");
        else if (a.scenery.size() != b.scenery.size()) note("scenery count");
        else if (a.spawns.size() != b.spawns.size())   note("spawn count");
        else if (a.colliders.size() != b.colliders.size()) note("collider count");
        else if (a.waypoints.size() != b.waypoints.size()) note("waypoint count");
        else {
            for (size_t i = 0; ok && i < a.polys.size(); ++i) {
                const EditorPoly& p = a.polys[i];
                const EditorPoly& q = b.polys[i];
                if (p.polyType != q.polyType) { note("polyType"); break; }
                for (int j = 0; j < 3; ++j) {
                    if (std::fabs(p.v[j].world.x - q.v[j].world.x) > 1e-3f ||
                        std::fabs(p.v[j].world.y - q.v[j].world.y) > 1e-3f) {
                        note("vertex position"); break;
                    }
                    if (std::fabs(p.v[j].tu - q.v[j].tu) > 1e-4f ||
                        std::fabs(p.v[j].tv - q.v[j].tv) > 1e-4f) {
                        note("vertex UV"); break;
                    }
                    if (p.v[j].r != q.v[j].r || p.v[j].g != q.v[j].g ||
                        p.v[j].b != q.v[j].b || p.v[j].alpha != q.v[j].alpha) {
                        note("vertex colour"); break;
                    }
                    /* Bounciness survives only via the normal's magnitude. */
                    if (std::fabs(p.bounciness[j] - q.bounciness[j]) > 1e-3f) {
                        note("bounciness"); break;
                    }
                }
            }
            for (size_t i = 0; ok && i < a.scenery.size(); ++i) {
                const EditorScenery& p = a.scenery[i];
                const EditorScenery& q = b.scenery[i];
                if (p.style != q.style) { note("scenery style"); break; }
                if (p.level != q.level) { note("scenery level"); break; }
                if (p.alpha != q.alpha) { note("scenery alpha"); break; }
                if (std::fabs(p.x - q.x) > 1e-3f ||
                    std::fabs(p.y - q.y) > 1e-3f) { note("scenery pos"); break; }
                if (std::fabs(p.rotation - q.rotation) > 1e-4f) { note("scenery rotation"); break; }
                if (std::fabs(p.scaleX - q.scaleX) > 1e-4f ||
                    std::fabs(p.scaleY - q.scaleY) > 1e-4f) { note("scenery scale"); break; }
            }
            for (size_t i = 0; ok && i < a.spawns.size(); ++i) {
                if (a.spawns[i].team != b.spawns[i].team ||
                    std::fabs(a.spawns[i].x - b.spawns[i].x) > 1e-3f ||
                    std::fabs(a.spawns[i].y - b.spawns[i].y) > 1e-3f) {
                    note("spawn"); break;
                }
            }
            for (size_t i = 0; ok && i < a.waypoints.size(); ++i) {
                if (a.waypoints[i].connections != b.waypoints[i].connections) {
                    note("waypoint connections"); break;
                }
            }
            if (ok && a.options.mapName != b.options.mapName) note("map name");
            if (ok && a.options.textureName != b.options.textureName) note("texture name");
            if (ok && a.options.steps != b.options.steps) note("steps");
            if (ok && a.options.weather != b.options.weather) note("weather");
        }
        ++checked;
    }

    std::fprintf(stderr, "  editor round-trip: %d maps checked, %d failed\n",
                 checked, bad);
    EXPECT(checked > 50);
    EXPECT_EQ(bad, 0);
}

TEST(pms_roundtrip_all_maps) {
    const std::string mapsDir = "maps";
    if (!fs::exists(mapsDir)) {
        std::fprintf(stderr, "  [SKIP] maps/ directory not found (run from project root)\n");
        return;
    }

    int loaded = 0, failed = 0;
    for (const auto& entry : fs::directory_iterator(mapsDir)) {
        auto ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext != ".pms" && ext != ".pfb") continue;

        std::string path = entry.path().string();
        PmsData data;
        std::string err;
        PmsLoadResult res = loadPmsFile(path, data, err);
        if (res != PmsLoadResult::OK) {
            ++failed;
            test_fail(("loadPmsFile: " + path).c_str(), __FILE__, __LINE__, err);
            continue;
        }

        /* Serialize to bytes and read back */
        std::vector<uint8_t> bytes;
        if (!pmsDataToBytes(data, bytes, err)) {
            ++failed;
            test_fail(("pmsDataToBytes: " + path).c_str(), __FILE__, __LINE__, err);
            continue;
        }
        PmsData data2;
        if (!pmsBytesToData(bytes, data2, err)) {
            ++failed;
            test_fail(("pmsBytesToData: " + path).c_str(), __FILE__, __LINE__, err);
            continue;
        }

        /* Check basic counts are preserved */
        bool ok = data2.polys.size() == data.polys.size()
               && data2.props.size() == data.props.size()
               && data2.spawns.size() == data.spawns.size()
               && data2.waypoints.size() == data.waypoints.size()
               && data2.lights.size() == data.lights.size()
               && data2.sketch.size() == data.sketch.size();
        if (!ok) {
            ++failed;
            test_fail(("round-trip counts: " + path).c_str(), __FILE__, __LINE__);
        } else {
            ++loaded;
        }
    }

    std::fprintf(stdout, "  PMS round-trip: %d maps passed, %d failed\n",
                 loaded, failed);
    EXPECT_EQ(failed, 0);
}

/* ---- Compile fidelity against the shipped Soldat maps ------------------ */
/*
 * Every map in maps/ was produced by the original VB6 SaveAndCompile.  Loading
 * one and recompiling it must reproduce the same collision data, because the
 * maps are already centred on their own bounding box.  This pins down the four
 * compile bugs found during the source-port audit:
 *   - inverted edge-normal sign
 *   - sectorsDivision never recomputed
 *   - Perp.Z written as bounciness instead of 1
 *   - POLY_NO_COLLIDE (type 3) polygons indexed into the sector table
 */
TEST(compile_matches_original_soldat_maps) {
    const std::string mapsDir = "maps";
    if (!fs::exists(mapsDir)) {
        std::fprintf(stderr, "  [SKIP] maps/ directory not found\n");
        return;
    }

    /* A representative spread of shipped maps. */
    const char* names[] = { "Arena2.pms", "Bunker.pms", "ctf_Ash.pms",
                            "Bridge.pms", "ctf_Run.pms" };

    int checked = 0;
    for (const char* name : names) {
        std::string path = mapsDir + "/" + name;
        if (!fs::exists(path)) continue;

        PmsData orig;
        std::string err;
        if (loadPmsFile(path, orig, err) != PmsLoadResult::OK) continue;
        if (orig.polys.empty()) continue;

        std::string tmp = (fs::temp_directory_path() /
                           ("pw_compile_" + std::string(name))).string();
        EXPECT(compilePms(tmp, orig, err));

        PmsData redone;
        EXPECT(loadPmsFile(tmp, redone, err) == PmsLoadResult::OK);
        fs::remove(tmp);

        if (redone.polys.size() != orig.polys.size()) {
            test_fail("recompiled poly count", __FILE__, __LINE__, name);
            continue;
        }

        /* sectorsDivision must match the original file exactly. */
        EXPECT_EQ(redone.sectorDiv, orig.sectorDiv);

        /* mapRandomID must be a plausible VB6-style positive ID. */
        EXPECT(redone.options.mapRandomID >= 10000 &&
               redone.options.mapRandomID <= 1009999);

        /* Edge normals must match the original direction and magnitude. */
        int normalMismatch = 0, zMismatch = 0;
        for (size_t p = 0; p < orig.polys.size(); ++p)
            for (int i = 0; i < 3; ++i) {
                const PmsNormal& a = orig.polys[p].poly.perp.n[i];
                const PmsNormal& b = redone.polys[p].poly.perp.n[i];
                if (std::fabs(a.x - b.x) > 1e-3f ||
                    std::fabs(a.y - b.y) > 1e-3f)
                    ++normalMismatch;
                if (std::fabs(b.z - 1.0f) > 1e-6f)
                    ++zMismatch;
            }
        if (normalMismatch != 0)
            test_fail("edge normals differ from original", __FILE__, __LINE__,
                      std::string(name) + ": " + std::to_string(normalMismatch));
        else
            test_pass("edge normals", __FILE__, __LINE__);
        EXPECT_EQ(zMismatch, 0);

        /* POLY_NO_COLLIDE must never appear in the sector table. */
        int badType = 0, overCap = 0;
        for (int i = 0; i < SECTOR_CELLS; ++i)
            for (int j = 0; j < SECTOR_CELLS; ++j) {
                if (redone.sectors[i][j].polyCount > 256) ++overCap;
                for (int idx : redone.sectors[i][j].polyIndex)
                    if (idx >= 0 && idx < static_cast<int>(redone.polys.size()) &&
                        redone.polys[idx].polyType == POLY_NO_COLLIDE)
                        ++badType;
            }
        EXPECT_EQ(badType, 0);
        EXPECT_EQ(overCap, 0);

        /* The regenerated sector table should closely track the original's.
           (Exact equality is impossible: VB6 derived the centring offset from
           editor-tracked extents that also covered scenery and waypoints.) */
        int cellsEqual = 0;
        for (int i = 0; i < SECTOR_CELLS; ++i)
            for (int j = 0; j < SECTOR_CELLS; ++j)
                if (redone.sectors[i][j].polyIndex ==
                    orig.sectors[i][j].polyIndex) ++cellsEqual;
        if (cellsEqual < (SECTOR_CELLS * SECTOR_CELLS * 85) / 100)
            test_fail("sector table diverges from original", __FILE__, __LINE__,
                      std::string(name) + ": " + std::to_string(cellsEqual) +
                      "/" + std::to_string(SECTOR_CELLS * SECTOR_CELLS));
        else
            test_pass("sector table", __FILE__, __LINE__);

        ++checked;
    }
    EXPECT(checked > 0);
}

/* ---- Bounciness survives an editor round-trip -------------------------- */
TEST(bounciness_roundtrip) {
    MapDocument doc;
    EditorPoly p{};
    p.v[0].world = {0, 0};
    p.v[1].world = {100, 0};
    p.v[2].world = {50, 100};
    p.polyType = POLY_BOUNCY;
    p.bounciness[0] = p.bounciness[1] = p.bounciness[2] = 2.5f;
    doc.addPoly(p);

    PmsData data;
    docToPmsData(doc, data);

    /* VB6 writes Perp.Z as 1 and carries bounciness in the vector magnitude. */
    for (int i = 0; i < 3; ++i) {
        EXPECT_NEAR(data.polys[0].poly.perp.n[i].z, 1.0f, 1e-6f);
        float mag = std::sqrt(data.polys[0].poly.perp.n[i].x *
                              data.polys[0].poly.perp.n[i].x +
                              data.polys[0].poly.perp.n[i].y *
                              data.polys[0].poly.perp.n[i].y);
        EXPECT_NEAR(mag, 2.5f, 1e-3f);
    }

    MapDocument doc2;
    pmsDataToDoc(data, doc2);
    EXPECT_EQ(doc2.polys.size(), 1u);
    for (int i = 0; i < 3; ++i)
        EXPECT_NEAR(doc2.polys[0].bounciness[i], 2.5f, 1e-3f);
}

/* ---- Edge-normal convention -------------------------------------------- */
/*
 * VB6 (frm:2670-2683) builds the edge normal as
 *     n = ( (v[i].y - v[j].y) , (v[j].x - v[i].x) ) / len
 * i.e. the edge direction rotated 90° clockwise on screen (Y down).
 * This is the opposite sign to the naive (dy, -dx) form, and it is the
 * convention every shipped Soldat map uses.
 */
TEST(compile_normal_orientation) {
    PmsData d;
    d.version = PMS_VERSION;
    PmsPolyEntry pe{};
    pe.poly.v[0].x = 0;   pe.poly.v[0].y = 0;
    pe.poly.v[1].x = 100; pe.poly.v[1].y = 0;
    pe.poly.v[2].x = 50;  pe.poly.v[2].y = 100;
    pe.polyType = POLY_NORMAL;
    d.polys.push_back(pe);

    std::string tmp = (fs::temp_directory_path() / "pw_normal_test.pms").string();
    std::string err;
    EXPECT(compilePms(tmp, d, err));
    PmsData back;
    EXPECT(loadPmsFile(tmp, back, err) == PmsLoadResult::OK);
    fs::remove(tmp);

    /* Edge 0 runs +X along the top edge -> normal is +Y. */
    EXPECT_NEAR(back.polys[0].poly.perp.n[0].x, 0.0f, 1e-3f);
    EXPECT_NEAR(back.polys[0].poly.perp.n[0].y, 1.0f, 1e-3f);

    /* All normals must be unit length and perpendicular to their edge. */
    for (int i = 0; i < 3; ++i) {
        int j = (i + 1) % 3;
        const PmsNormal& n = back.polys[0].poly.perp.n[i];
        EXPECT_NEAR(std::sqrt(n.x * n.x + n.y * n.y), 1.0f, 1e-3f);
        float ex = back.polys[0].poly.v[j].x - back.polys[0].poly.v[i].x;
        float ey = back.polys[0].poly.v[j].y - back.polys[0].poly.v[i].y;
        float elen = std::sqrt(ex * ex + ey * ey);
        EXPECT_NEAR((ex * n.x + ey * n.y) / elen, 0.0f, 1e-3f);
    }
}

/* ---- Texture colour key (modGlobals.bas COLOR_KEY = &HFF00FF00) -------- */
TEST(color_key_makes_pure_green_transparent) {
    /* 2x2: pure green, near-green, green with alpha 0 already, and red. */
    unsigned char px[16] = {
          0, 255,   0, 255,   /* exact key      -> transparent */
          1, 255,   0, 255,   /* off by one     -> untouched   */
          0, 255,   0,   0,   /* already alpha0 -> untouched   */
        255,   0,   0, 255,   /* red            -> untouched   */
    };
    applyColorKey(px, 2, 2);

    EXPECT_EQ((int)px[3], 0);      /* keyed out */
    EXPECT_EQ((int)px[1], 0);      /* green channel zeroed to stop bleeding */

    EXPECT_EQ((int)px[4], 1);      /* near-green untouched */
    EXPECT_EQ((int)px[5], 255);
    EXPECT_EQ((int)px[7], 255);

    EXPECT_EQ((int)px[9], 255);    /* alpha already 0: not the key colour */
    EXPECT_EQ((int)px[11], 0);

    EXPECT_EQ((int)px[12], 255);   /* red fully untouched */
    EXPECT_EQ((int)px[13], 0);
    EXPECT_EQ((int)px[15], 255);
}

TEST(color_key_handles_degenerate_input) {
    /* Must not crash or read out of bounds. */
    applyColorKey(nullptr, 4, 4);
    unsigned char one[4] = {0, 255, 0, 255};
    applyColorKey(one, 0, 0);
    EXPECT_EQ((int)one[3], 255);   /* untouched when the size is empty */
    applyColorKey(one, 1, 1);
    EXPECT_EQ((int)one[3], 0);
}

/* ---- Polygon type colours (modConfig.bas gPolyTypeColors) -------------- */
TEST(poly_type_colors_match_original_defaults) {
    /* Index 0 (Normal) is the user's selection colour. */
    EXPECT_EQ((int)polyTypeColor(POLY_NORMAL), (int)0xCE4D4Au);
    EXPECT_EQ((int)polyTypeColor(POLY_NORMAL, 0x123456u), (int)0x123456u);

    EXPECT_EQ((int)polyTypeColor(1),  (int)0x7ACC29u);  /* OnlyBullets   */
    EXPECT_EQ((int)polyTypeColor(4),  (int)0x29CCCCu);  /* Ice           */
    EXPECT_EQ((int)polyTypeColor(9),  (int)0xCC7A29u);  /* Lava          */
    EXPECT_EQ((int)polyTypeColor(18), (int)0x297ACCu);  /* Bouncy        */
    EXPECT_EQ((int)polyTypeColor(24), (int)0x292929u);  /* Back          */
    EXPECT_EQ((int)polyTypeColor(25), (int)0x7A7A7Au);  /* BackTransition*/

    /* 12..17 alias 10/11 in the original. */
    EXPECT_EQ((int)polyTypeColor(12), (int)polyTypeColor(10));
    EXPECT_EQ((int)polyTypeColor(13), (int)polyTypeColor(11));
    EXPECT_EQ((int)polyTypeColor(16), (int)polyTypeColor(10));
    EXPECT_EQ((int)polyTypeColor(17), (int)polyTypeColor(11));

    /* Out-of-range falls back to the selection colour, never garbage. */
    EXPECT_EQ((int)polyTypeColor(99, 0xAABBCCu), (int)0xAABBCCu);
    EXPECT_EQ((int)polyTypeColor(-3, 0xAABBCCu), (int)0xAABBCCu);
}

/* ---- Interactive transform sessions ------------------------------------ */
TEST(transform_session_scale_and_rotate) {
    MapDocument doc;
    EditorPoly p{};
    p.v[0].world = {0, 0};
    p.v[1].world = {100, 0};
    p.v[2].world = {100, 100};
    p.v[0].selected = p.v[1].selected = p.v[2].selected = true;
    doc.addPoly(p);

    /* Bounding rect is (0,0)-(100,100) so the centre is (50,50). */
    Vec2 c = doc.selectionCenter();
    EXPECT_NEAR(c.x, 50.0f, 1e-4f);
    EXPECT_NEAR(c.y, 50.0f, 1e-4f);

    MapDocument::TransformSession s;
    doc.beginTransform(s);
    EXPECT(!s.empty());

    /* Scale ×2 about the centre. */
    doc.applyTransform(s, 2.0f, 2.0f, 0.0f);
    EXPECT_NEAR(doc.polys[0].v[0].world.x, -50.0f, 1e-3f);
    EXPECT_NEAR(doc.polys[0].v[0].world.y, -50.0f, 1e-3f);

    /* Re-applying from the same session must not accumulate. */
    doc.applyTransform(s, 2.0f, 2.0f, 0.0f);
    EXPECT_NEAR(doc.polys[0].v[0].world.x, -50.0f, 1e-3f);

    /* Rotate 90° about the centre: (0,0) -> (100,0). */
    doc.applyTransform(s, 1.0f, 1.0f, 3.14159265358979f / 2.0f);
    EXPECT_NEAR(doc.polys[0].v[0].world.x, 100.0f, 1e-3f);
    EXPECT_NEAR(doc.polys[0].v[0].world.y,   0.0f, 1e-3f);

    /* Identity transform restores the original geometry. */
    doc.applyTransform(s, 1.0f, 1.0f, 0.0f);
    EXPECT_NEAR(doc.polys[0].v[0].world.x, 0.0f, 1e-3f);
    EXPECT_NEAR(doc.polys[0].v[0].world.y, 0.0f, 1e-3f);
}

TEST(transform_session_moves_all_entity_kinds) {
    MapDocument doc;
    EditorPoly p{};
    p.v[0].world = {0, 0};
    p.v[1].world = {100, 0};
    p.v[2].world = {100, 100};
    p.v[0].selected = p.v[1].selected = p.v[2].selected = true;
    doc.addPoly(p);
    doc.addSpawn(0, 0, SPAWN_ALPHA);
    doc.spawns.back().selected = true;
    doc.polys[0].v[0].selected = true;  /* addSpawn cleared the selection */
    doc.polys[0].v[1].selected = true;
    doc.polys[0].v[2].selected = true;

    MapDocument::TransformSession s;
    doc.beginTransform(s);
    doc.applyTransform(s, 2.0f, 2.0f, 0.0f);

    /* VB6 scales spawns along with polygons; centre is (50,50). */
    EXPECT_NEAR(doc.spawns[0].x, -50.0f, 1e-3f);
    EXPECT_NEAR(doc.spawns[0].y, -50.0f, 1e-3f);
}

/* ---- Snapping (previously dead menu toggles) --------------------------- */
TEST(snap_selected_to_grid) {
    MapDocument doc;
    EditorPoly p{};
    p.v[0].world = {13, 27};
    p.v[1].world = {113, 27};
    p.v[2].world = {113, 127};
    doc.addPoly(p);
    doc.polys[0].v[0].selected = true;

    doc.viewSettings.showGrid   = true;
    doc.viewSettings.snapToGrid = true;
    doc.viewSettings.gridSize   = 10.0f;

    EXPECT(doc.snapSelected(8.0f));
    /* 13 -> 10, 27 -> 30; only the selected vertex moves. */
    EXPECT_NEAR(doc.polys[0].v[0].world.x, 10.0f, 1e-3f);
    EXPECT_NEAR(doc.polys[0].v[0].world.y, 30.0f, 1e-3f);
    EXPECT_NEAR(doc.polys[0].v[1].world.x, 113.0f, 1e-3f);
}

TEST(snap_selected_to_vertices) {
    MapDocument doc;
    EditorPoly a{};
    a.v[0].world = {0, 0};
    a.v[1].world = {100, 0};
    a.v[2].world = {100, 100};
    doc.addPoly(a);

    EditorPoly b{};
    b.v[0].world = {103, 2};   /* within 8 units of a.v[1] = (100,0) */
    b.v[1].world = {200, 0};
    b.v[2].world = {200, 100};
    doc.addPoly(b);

    doc.polys[1].v[0].selected = true;
    doc.viewSettings.snapToVertices = true;
    doc.viewSettings.snapToGrid = false;

    EXPECT(doc.snapSelected(8.0f));
    EXPECT_NEAR(doc.polys[1].v[0].world.x, 100.0f, 1e-3f);
    EXPECT_NEAR(doc.polys[1].v[0].world.y,   0.0f, 1e-3f);

    /* Nothing in range -> no movement, no false positive. */
    doc.clearSelection();
    doc.polys[1].v[1].selected = true;
    EXPECT(!doc.snapSelected(8.0f));
    EXPECT_NEAR(doc.polys[1].v[1].world.x, 200.0f, 1e-3f);
}

/* ---- PMS ↔ MapDocument round-trip ------------------------------------- */
TEST(pms_doc_roundtrip) {
    const std::string mapsDir = "maps";
    if (!fs::exists(mapsDir)) return;

    /* Pick the first .pms we find */
    std::string testMap;
    for (const auto& e : fs::directory_iterator(mapsDir)) {
        auto ext = e.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".pms") { testMap = e.path().string(); break; }
    }
    if (testMap.empty()) return;

    PmsData data1;
    std::string err;
    EXPECT(loadPmsFile(testMap, data1, err) == PmsLoadResult::OK);

    MapDocument doc;
    pmsDataToDoc(data1, doc);

    PmsData data2;
    docToPmsData(doc, data2);

    EXPECT_EQ(data1.polys.size(),    data2.polys.size());
    EXPECT_EQ(data1.spawns.size(),   data2.spawns.size());
    EXPECT_EQ(data1.waypoints.size(),data2.waypoints.size());

    if (!data1.polys.empty()) {
        EXPECT_NEAR(data1.polys[0].poly.v[0].x,
                    data2.polys[0].poly.v[0].x, 1e-3f);
        EXPECT_NEAR(data1.polys[0].poly.v[0].y,
                    data2.polys[0].poly.v[0].y, 1e-3f);
    }
}

/* ---- UndoStack --------------------------------------------------------- */

TEST(undo_basic) {
    MapDocument doc;
    UndoStack stack(8);

    EditorPoly p{};
    p.v[0].world = {0,0};  p.v[1].world = {100,0};  p.v[2].world = {50,100};
    p.v[0].selected = p.v[1].selected = p.v[2].selected = true;

    stack.push(doc);  /* push empty state */
    doc.addPoly(p);
    EXPECT_EQ(doc.polys.size(), 1u);

    EXPECT(stack.canUndo());
    EXPECT(!stack.canRedo());
    bool ok = stack.undo(doc);
    EXPECT(ok);
    EXPECT_EQ(doc.polys.size(), 0u);  /* restored to empty */
    EXPECT(stack.canRedo());
}

TEST(undo_redo) {
    MapDocument doc;
    UndoStack stack(8);

    EditorPoly p{};
    p.v[0].world = {0,0}; p.v[1].world={100,0}; p.v[2].world={50,100};
    p.v[0].selected=p.v[1].selected=p.v[2].selected=true;

    stack.push(doc);   /* state: 0 polys */
    doc.addPoly(p);    /* state: 1 poly  */

    stack.undo(doc);
    EXPECT_EQ(doc.polys.size(), 0u);

    stack.redo(doc);
    EXPECT_EQ(doc.polys.size(), 1u);
}

TEST(undo_push_clears_redo) {
    MapDocument doc;
    UndoStack stack(8);

    EditorPoly p{};
    p.v[0].world={0,0}; p.v[1].world={100,0}; p.v[2].world={50,100};
    p.v[0].selected=p.v[1].selected=p.v[2].selected=true;

    stack.push(doc);
    doc.addPoly(p);
    stack.undo(doc);
    EXPECT(stack.canRedo());

    /* Push a new state — redo should be gone */
    stack.push(doc);
    EXPECT(!stack.canRedo());
}

TEST(undo_depth_limit) {
    MapDocument doc;
    UndoStack stack(3);  /* keep only 3 snapshots */

    for (int i = 0; i < 5; ++i) {
        stack.push(doc);
        EditorPoly p{};
        p.v[0].world={(float)i,0};p.v[1].world={(float)(i+100),0};
        p.v[2].world={(float)(i+50),100};
        doc.addPoly(p);
    }

    /* Should be able to undo at most 3 times */
    int count = 0;
    while (stack.canUndo()) { stack.undo(doc); ++count; }
    EXPECT(count <= 3);
}

TEST(undo_empty_does_not_crash) {
    MapDocument doc;
    UndoStack stack(8);
    EXPECT(!stack.canUndo());
    EXPECT(!stack.canRedo());
    bool ok = stack.undo(doc);
    EXPECT(!ok);
    ok = stack.redo(doc);
    EXPECT(!ok);
}

/* ---- SelectMode tests -------------------------------------------------- */

TEST(select_mode_replace) {
    MapDocument doc;
    EditorPoly p{};
    p.v[0].world = {0, 0}; p.v[1].world = {100, 0}; p.v[2].world = {50, 100};
    doc.addPoly(p);
    /* Select vertex 0 */
    doc.selectVertexAt({10, 10}, 50.0f, MapDocument::SelectMode::Replace);
    EXPECT( doc.polys[0].v[0].selected);
    /* Replace should deselect previous when selecting a different vertex */
    doc.polys[0].v[1].selected = true;
    doc.selectVertexAt({90, 10}, 50.0f, MapDocument::SelectMode::Replace);
    EXPECT(!doc.polys[0].v[0].selected);
}

TEST(select_mode_add) {
    MapDocument doc;
    /* Two separate polygons */
    EditorPoly a{};
    a.v[0].world = {0,0}; a.v[1].world = {50,0}; a.v[2].world = {25,50};
    doc.addPoly(a);
    EditorPoly b{};
    b.v[0].world = {200,0}; b.v[1].world = {250,0}; b.v[2].world = {225,50};
    doc.addPoly(b);

    /* Select vertex in poly A */
    doc.selectVertexAt({25, 25}, 60.0f, MapDocument::SelectMode::Replace);
    /* Add vertex in poly B */
    doc.selectVertexAt({225, 25}, 60.0f, MapDocument::SelectMode::Add);
    /* Both polys should have a selected vertex */
    EXPECT(doc.polys[0].anySelected());
    EXPECT(doc.polys[1].anySelected());
}

TEST(select_mode_subtract) {
    MapDocument doc;
    EditorPoly p{};
    p.v[0].world = {0,0}; p.v[1].world = {100,0}; p.v[2].world = {50,100};
    p.v[0].selected = true; p.v[1].selected = true; p.v[2].selected = true;
    doc.addPoly(p);
    /* Subtract vertex nearest to {10,10} */
    doc.selectVertexAt({10, 10}, 50.0f, MapDocument::SelectMode::Subtract);
    /* v0 should be deselected; v1,v2 remain */
    EXPECT(!doc.polys[0].v[0].selected);
    EXPECT( doc.polys[0].v[1].selected);
    EXPECT( doc.polys[0].v[2].selected);
}

TEST(select_vertices_rect_subtract) {
    MapDocument doc;
    EditorPoly p{};
    p.v[0].world = {5,5}; p.v[1].world = {50,5}; p.v[2].world = {25,50};
    p.v[0].selected = true; p.v[1].selected = true; p.v[2].selected = true;
    doc.addPoly(p);
    /* Subtract vertices within rect that covers v0 only */
    doc.selectVerticesInRect({0,0}, {10,10}, MapDocument::SelectMode::Subtract);
    EXPECT(!doc.polys[0].v[0].selected);
    EXPECT( doc.polys[0].v[1].selected);
    EXPECT( doc.polys[0].v[2].selected);
}

TEST(select_vertices_rect_add_multi_poly) {
    MapDocument doc;
    EditorPoly a{};
    a.v[0].world = {0,0}; a.v[1].world = {50,0}; a.v[2].world = {25,50};
    doc.addPoly(a);
    EditorPoly b{};
    b.v[0].world = {200,0}; b.v[1].world = {250,0}; b.v[2].world = {225,50};
    doc.addPoly(b);

    /* Select poly A vertices */
    doc.selectVerticesInRect({-10,-10}, {100,100}, MapDocument::SelectMode::Replace);
    EXPECT(doc.polys[0].anySelected());
    EXPECT(!doc.polys[1].anySelected());

    /* Add poly B vertices */
    doc.selectVerticesInRect({150,-10}, {300,100}, MapDocument::SelectMode::Add);
    EXPECT(doc.polys[0].anySelected());
    EXPECT(doc.polys[1].anySelected());
}

/* ---- Color painting tests ---------------------------------------------- */

TEST(blend_color_normal) {
    /* Normal mode, opacity=1: destination should become source */
    uint8_t r = 100, g = 150, b = 200;
    MapDocument::blendColor(r, g, b, 255, 0, 0, 1.0f, 0);
    EXPECT_EQ(r, 255);
    EXPECT_EQ(g, 0);
    EXPECT_EQ(b, 0);
}

TEST(blend_color_half_opacity) {
    /* Normal mode, opacity=0.5: mix 50/50 */
    uint8_t r = 0, g = 0, b = 0;
    MapDocument::blendColor(r, g, b, 100, 100, 100, 0.5f, 0);
    EXPECT_NEAR((float)r, 50.0f, 1.0f);
    EXPECT_NEAR((float)g, 50.0f, 1.0f);
    EXPECT_NEAR((float)b, 50.0f, 1.0f);
}

TEST(apply_color_to_selected) {
    MapDocument doc;
    EditorPoly p{};
    p.v[0].world = {0,0}; p.v[1].world = {100,0}; p.v[2].world = {50,100};
    p.v[0].r = p.v[0].g = p.v[0].b = 100;
    p.v[1].r = p.v[1].g = p.v[1].b = 100;
    p.v[2].r = p.v[2].g = p.v[2].b = 100;
    p.v[0].selected = true;
    p.v[1].selected = false;
    doc.addPoly(p);

    bool ok = doc.applyColorToSelected(255, 0, 0, 1.0f, 0);
    EXPECT(ok);
    EXPECT_EQ(doc.polys[0].v[0].r, 255);
    EXPECT_EQ(doc.polys[0].v[0].g, 0);
    /* v1 not selected: unchanged */
    EXPECT_EQ(doc.polys[0].v[1].r, 100);
    EXPECT_EQ(doc.polys[0].v[1].g, 100);
}

TEST(apply_color_to_selected_nothing_selected) {
    MapDocument doc;
    EditorPoly p{};
    p.v[0].world = {0,0}; p.v[1].world = {100,0}; p.v[2].world = {50,100};
    doc.addPoly(p);
    /* No selection: should return false */
    bool ok = doc.applyColorToSelected(255, 0, 0, 1.0f, 0);
    EXPECT(!ok);
}

TEST(apply_color_to_poly_at) {
    MapDocument doc;
    EditorPoly p{};
    p.v[0].world = {0,0}; p.v[1].world = {100,0}; p.v[2].world = {50,100};
    p.v[0].r = p.v[0].g = p.v[0].b = 50;
    p.v[1].r = p.v[1].g = p.v[1].b = 50;
    p.v[2].r = p.v[2].g = p.v[2].b = 50;
    doc.addPoly(p);

    /* Click inside the triangle (centroid ≈ 50, 33) */
    bool ok = doc.applyColorToPolyAt({50, 33}, 0, 255, 0, 1.0f, 0);
    EXPECT(ok);
    EXPECT_EQ(doc.polys[0].v[0].r, 0);
    EXPECT_EQ(doc.polys[0].v[0].g, 255);
    EXPECT_EQ(doc.polys[0].v[0].b, 0);
}

TEST(apply_color_to_vertices_near) {
    MapDocument doc;
    EditorPoly p{};
    p.v[0].world = {0,0}; p.v[1].world = {100,0}; p.v[2].world = {50,100};
    p.v[0].r = p.v[0].g = p.v[0].b = 50;
    p.v[1].r = p.v[1].g = p.v[1].b = 50;
    p.v[2].r = p.v[2].g = p.v[2].b = 50;
    doc.addPoly(p);

    /* Paint within radius 30 of vertex 0 — should only reach v0 */
    bool ok = doc.applyColorToVerticesNear({0,0}, 30.0f, 255, 0, 0, 1.0f, 0);
    EXPECT(ok);
    EXPECT_EQ(doc.polys[0].v[0].r, 255);
    EXPECT_EQ(doc.polys[0].v[0].g, 0);
    /* v1 at (100,0) is outside radius 30 */
    EXPECT_EQ(doc.polys[0].v[1].r, 50);
    EXPECT_EQ(doc.polys[0].v[1].g, 50);
}

TEST(apply_color_vertices_near_respects_selection) {
    MapDocument doc;
    EditorPoly p{};
    p.v[0].world = {0,0}; p.v[1].world = {10,0}; p.v[2].world = {5,10};
    p.v[0].r = p.v[0].g = p.v[0].b = 50;
    p.v[1].r = p.v[1].g = p.v[1].b = 50;
    p.v[2].r = p.v[2].g = p.v[2].b = 50;
    /* Mark v1 as selected; v0 and v2 unselected */
    p.v[1].selected = true;
    doc.addPoly(p);

    /* Paint within big radius that covers all vertices:
       selection exists → only selected v1 should be painted */
    bool ok = doc.applyColorToVerticesNear({5, 5}, 100.0f, 0, 0, 255, 1.0f, 0);
    EXPECT(ok);
    EXPECT_EQ(doc.polys[0].v[0].b, 50);   /* unselected: not painted */
    EXPECT_EQ(doc.polys[0].v[1].b, 255);  /* selected: painted */
    EXPECT_EQ(doc.polys[0].v[2].b, 50);   /* unselected: not painted */
}

/* ---- Polygon operations ------------------------------------------------ */

TEST(split_at_vertex_creates_new_poly) {
    MapDocument doc;
    EditorPoly p{};
    p.v[0].world = {0, 0}; p.v[0].selected = true;
    p.v[1].world = {100, 0};
    p.v[2].world = {50, 100};
    doc.addPoly(p);

    const size_t before = doc.polys.size();
    doc.splitAtVertex();
    EXPECT_EQ((int)doc.polys.size(), (int)(before + 1));
}

TEST(split_at_vertex_moves_original_vertex) {
    MapDocument doc;
    EditorPoly p{};
    p.v[0].world = {0, 0}; p.v[0].selected = true;
    p.v[1].world = {100, 0};
    p.v[2].world = {50, 100};
    doc.addPoly(p);

    doc.splitAtVertex();
    /* Original poly's v1 (left of v0) should have moved to midpoint of (100,0)-(50,100) */
    const Vec2& moved = doc.polys[0].v[1].world;
    EXPECT(std::abs(moved.x - 75.0f) < 0.01f);
    EXPECT(std::abs(moved.y - 50.0f) < 0.01f);
}

TEST(split_at_vertex_no_selection_is_noop) {
    MapDocument doc;
    EditorPoly p{};
    p.v[0].world = {0, 0}; p.v[1].world = {100, 0}; p.v[2].world = {50, 100};
    doc.addPoly(p);
    const size_t before = doc.polys.size();
    doc.splitAtVertex();
    EXPECT_EQ((int)doc.polys.size(), (int)before);
}

TEST(join_vertices_snaps_to_first) {
    MapDocument doc;
    EditorPoly p{};
    p.v[0].world = {0, 0}; p.v[0].selected = true;
    p.v[1].world = {100, 0}; p.v[1].selected = true;
    p.v[2].world = {50, 100}; p.v[2].selected = true;
    doc.addPoly(p);

    doc.joinSelectedVertices();
    for (int j = 0; j < 3; ++j) {
        EXPECT(std::abs(doc.polys[0].v[j].world.x) < 0.01f);
        EXPECT(std::abs(doc.polys[0].v[j].world.y) < 0.01f);
    }
}

TEST(create_poly_from_selected_adds_poly) {
    MapDocument doc;
    EditorPoly p{};
    p.v[0].world = {0, 0}; p.v[0].selected = true;
    p.v[1].world = {100, 0}; p.v[1].selected = true;
    p.v[2].world = {50, 100}; p.v[2].selected = true;
    doc.addPoly(p);

    const size_t before = doc.polys.size();
    doc.createPolyFromSelected();
    EXPECT_EQ((int)doc.polys.size(), (int)(before + 1));
}

TEST(create_poly_from_selected_needs_three_verts) {
    MapDocument doc;
    EditorPoly p{};
    p.v[0].world = {0, 0}; p.v[0].selected = true;
    p.v[1].world = {100, 0}; p.v[1].selected = true;
    p.v[2].world = {50, 100};
    doc.addPoly(p);

    const size_t before = doc.polys.size();
    doc.createPolyFromSelected();
    EXPECT_EQ((int)doc.polys.size(), (int)before);
}

TEST(untexture_selected_sets_tu_tv_one) {
    MapDocument doc;
    EditorPoly p{};
    p.v[0].world = {0, 0}; p.v[0].selected = true; p.v[0].tu = 0.3f; p.v[0].tv = 0.7f;
    p.v[1].world = {100, 0}; p.v[1].tu = 0.5f; p.v[1].tv = 0.5f;
    p.v[2].world = {50, 100};
    doc.addPoly(p);

    doc.untextureSelected();
    EXPECT(std::abs(doc.polys[0].v[0].tu - 1.0f) < 0.001f);
    EXPECT(std::abs(doc.polys[0].v[0].tv - 1.0f) < 0.001f);
    EXPECT(std::abs(doc.polys[0].v[1].tu - 0.5f) < 0.001f);
}

TEST(fix_texture_sets_tu_tv_from_world) {
    MapDocument doc;
    EditorPoly p{};
    p.v[0].world = {64, 128}; p.v[0].selected = true;
    p.v[1].world = {0, 0};
    p.v[2].world = {100, 0};
    doc.addPoly(p);

    doc.fixTextureOnSelected(256.0f, 512.0f);
    EXPECT(std::abs(doc.polys[0].v[0].tu - 0.25f) < 0.001f);
    EXPECT(std::abs(doc.polys[0].v[0].tv - 0.25f) < 0.001f);
    EXPECT(std::abs(doc.polys[0].v[1].tu) < 0.001f);
}

TEST(average_vertex_colors_groups_coincident) {
    MapDocument doc;
    EditorPoly p1{};
    p1.v[0].world = {0, 0}; p1.v[0].r = 100; p1.v[0].g = 0; p1.v[0].b = 0;
    p1.v[1].world = {100, 0}; p1.v[2].world = {50, 100};
    doc.addPoly(p1);

    EditorPoly p2{};
    p2.v[0].world = {0, 0}; p2.v[0].r = 0; p2.v[0].g = 100; p2.v[0].b = 0;
    p2.v[1].world = {200, 0}; p2.v[2].world = {100, 100};
    doc.addPoly(p2);

    doc.averageVertexColors();

    EXPECT_EQ((int)doc.polys[0].v[0].r, 50);
    EXPECT_EQ((int)doc.polys[0].v[0].g, 50);
    EXPECT_EQ((int)doc.polys[1].v[0].r, 50);
    EXPECT_EQ((int)doc.polys[1].v[0].g, 50);
}

TEST(average_vertex_colors_no_coincident_unchanged) {
    MapDocument doc;
    EditorPoly p{};
    p.v[0].world = {0, 0};   p.v[0].r = 200;
    p.v[1].world = {100, 0}; p.v[1].r = 100;
    p.v[2].world = {50, 100}; p.v[2].r = 50;
    doc.addPoly(p);

    doc.averageVertexColors();
    EXPECT_EQ((int)doc.polys[0].v[0].r, 200);
    EXPECT_EQ((int)doc.polys[0].v[1].r, 100);
    EXPECT_EQ((int)doc.polys[0].v[2].r, 50);
}

/* ---- Tools ported in the completion pass -------------------------------- */

TEST(poly_type_names_match_original_captions) {
    EXPECT_EQ(POLY_TYPE_COUNT, 26);
    EXPECT(std::string(polyTypeName(0)) == "Normal");
    EXPECT(std::string(polyTypeName(4)) == "Ice");
    EXPECT(std::string(polyTypeName(20)) == "Hurts Flaggers");
    EXPECT(std::string(polyTypeName(25)) == "Background Transition");
}

TEST(vertex_z_and_rhw_survive_native_roundtrip) {
    MapDocument doc;
    EditorPoly p{};
    p.v[0].world = {0, 0};    p.v[0].z = 77.0f;  p.v[0].rhw = -10.0f;
    p.v[1].world = {100, 0};  p.v[1].z = 12.5f;
    p.v[2].world = {50, 100}; p.v[2].z = -1.0f;
    doc.addPoly(p);

    PmsData data;
    docToPmsData(doc, data);
    MapDocument back;
    pmsDataToDoc(data, back);

    EXPECT_NEAR(back.polys[0].v[0].z, 77.0f, 0.001f);
    EXPECT_NEAR(back.polys[0].v[0].rhw, -10.0f, 0.001f);
    EXPECT_NEAR(back.polys[0].v[1].z, 12.5f, 0.001f);
    EXPECT_NEAR(back.polys[0].v[2].z, -1.0f, 0.001f);
}

TEST(toggle_selected_visibility_flips_z_and_rhw) {
    MapDocument doc;
    EditorPoly p{};
    for (int i = 0; i < 3; ++i) p.v[i].selected = true;
    doc.addPoly(p);

    doc.toggleSelectedVisibility();
    EXPECT_NEAR(doc.polys[0].v[0].z, -1.0f, 0.001f);
    EXPECT_NEAR(doc.polys[0].v[0].rhw, -10.0f, 0.001f);
    doc.toggleSelectedVisibility();
    EXPECT_NEAR(doc.polys[0].v[0].z, 1.0f, 0.001f);
    EXPECT_NEAR(doc.polys[0].v[0].rhw, 1.0f, 0.001f);
}

TEST(erase_sketch_uses_swap_delete_like_original) {
    MapDocument doc;
    for (int i = 0; i < 4; ++i)
        doc.addSketchLine({static_cast<float>(i) * 100, 0},
                          {static_cast<float>(i) * 100 + 10, 0});

    /* VB6 EraseSketch overwrites the hit entry with the last one and shrinks
       the array, so removing index 1 leaves the old last element there. */
    EXPECT(doc.eraseSketchAt({100, 0}, 16.0f));
    EXPECT_EQ((int)doc.sketch.size(), 3);
    EXPECT_NEAR(doc.sketch[1].a.x, 300.0f, 0.001f);
}

TEST(smudge_sketch_falls_off_with_distance) {
    MapDocument doc;
    doc.addSketchLine({0, 0}, {200, 0});

    doc.smudgeSketchAt({0, 0}, 10.0f, 0.0f, 100.0f);
    /* The endpoint at the cursor moves the full delta; the far one is out of
       range and must not move at all. */
    EXPECT_NEAR(doc.sketch[0].a.x, 10.0f, 0.001f);
    EXPECT_NEAR(doc.sketch[0].b.x, 200.0f, 0.001f);
}

TEST(apply_depth_prefers_selected_vertices) {
    MapDocument doc;
    EditorPoly p{};
    p.v[0].world = {0, 0};  p.v[0].selected = true;
    p.v[1].world = {5, 0};
    p.v[2].world = {0, 5};
    doc.addPoly(p);

    doc.applyDepthNear({0, 0}, 50.0f, 200.0f, 1.0f);
    EXPECT_NEAR(doc.polys[0].v[0].z, 200.0f, 0.001f);
    /* With a selection present the unselected vertices are left alone. */
    EXPECT_NEAR(doc.polys[0].v[1].z, 1.0f, 0.001f);
}

TEST(connect_waypoint_caps_connections_at_twenty) {
    MapDocument doc;
    doc.addWaypoint(0, 0);
    for (int i = 0; i < 25; ++i)
        doc.addWaypoint(static_cast<float>(i + 1) * 10.0f, 0);

    doc.currentWaypoint = -1;
    doc.connectWaypointAt({0, 0}, 4.0f);  /* anchor */
    for (int i = 0; i < 25; ++i)
        doc.connectWaypointAt({static_cast<float>(i + 1) * 10.0f, 0}, 4.0f);

    EXPECT(doc.waypoints[0].connections.size() <= 20);
}

TEST(clear_unused_scenery_renumbers_surviving_styles) {
    MapDocument doc;
    doc.sceneryNames.push_back("a.bmp");   /* index 1 — unused */
    doc.sceneryNames.push_back("b.bmp");   /* index 2 — used   */
    doc.addSceneryInstance(2, 10, 10, SCENERY_MIDDLE);

    EXPECT_EQ(doc.clearUnusedScenery(), 1);
    EXPECT_EQ((int)doc.sceneryNames.size(), 2);
    EXPECT(doc.sceneryNames[1] == "b.bmp");
    EXPECT_EQ(doc.scenery[0].style, 1);
}

TEST(snap_point_uses_grid_before_vertices) {
    MapDocument doc;
    EditorPoly p{};
    p.v[0].world = {103, 97};
    doc.addPoly(p);

    doc.viewSettings.snapToVertices = true;
    Vec2 pt{100, 100};
    EXPECT(doc.snapPoint(pt, 16.0f));
    EXPECT_NEAR(pt.x, 103.0f, 0.001f);

    doc.viewSettings.showGrid = true;
    doc.viewSettings.snapToGrid = true;
    doc.viewSettings.gridSize = 50.0f;
    Vec2 pt2{104, 96};
    EXPECT(doc.snapPoint(pt2, 16.0f));
    EXPECT_NEAR(pt2.x, 100.0f, 0.001f);
    EXPECT_NEAR(pt2.y, 100.0f, 0.001f);
}

TEST(extend_sketch_stroke_splits_after_sixteen_units) {
    MapDocument doc;
    doc.beginSketchStroke({0, 0});
    EXPECT(!doc.extendSketchStroke({5, 0}));
    EXPECT_EQ((int)doc.sketch.size(), 1);
    EXPECT(doc.extendSketchStroke({40, 0}));
    EXPECT_EQ((int)doc.sketch.size(), 2);
    /* Finishing drops the trailing stub that never grew. */
    doc.extendSketchStroke({41, 0}, true);
    EXPECT_EQ((int)doc.sketch.size(), 1);
}

TEST(rotate_selected_moves_non_polygon_entities) {
    MapDocument doc;
    EditorPoly p{};
    p.v[0].world = {-100, -100};
    p.v[1].world = {100, -100};
    p.v[2].world = {0, 100};
    doc.addPoly(p);
    /* addSpawn clears the selection, so select everything afterwards. */
    doc.addSpawn(100, 0, SPAWN_GENERAL);
    for (int i = 0; i < 3; ++i) doc.polys[0].v[i].selected = true;
    doc.spawns.back().selected = true;

    doc.rotateSelected(180.0f);
    EXPECT_NEAR(doc.spawns.back().x, -100.0f, 0.5f);
}


/* ---- Tab cycling (frm:6070 TabPressed) ---------------------------------- */

TEST(tab_rotates_vertex_selection) {
    MapDocument doc;
    EditorPoly p{};
    p.v[0].world = {0, 0}; p.v[1].world = {10, 0}; p.v[2].world = {0, 10};
    doc.addPoly(p);
    doc.polys[0].v[0].selected = true;

    EXPECT(doc.cycleSelection(false));
    EXPECT(!doc.polys[0].v[0].selected);
    EXPECT(doc.polys[0].v[2].selected);
}

TEST(tab_next_polygon) {
    MapDocument doc;
    for (int i = 0; i < 3; ++i) {
        EditorPoly p{};
        p.v[0].world = {static_cast<float>(i) * 20, 0};
        p.v[1].world = {static_cast<float>(i) * 20 + 10, 0};
        p.v[2].world = {static_cast<float>(i) * 20, 10};
        doc.addPoly(p);
    }
    for (int i = 0; i < 3; ++i) doc.polys[0].v[i].selected = true;

    EXPECT(doc.cycleSelection(false));
    EXPECT(!doc.polys[0].anySelected());
    EXPECT(doc.polys[1].allSelected());

    /* Shift+Tab wraps backwards past the first polygon. */
    EXPECT(doc.cycleSelection(true));
    EXPECT(doc.polys[0].allSelected());
    EXPECT(doc.cycleSelection(true));
    EXPECT(doc.polys[2].allSelected());
}

TEST(tab_cycles_scenery) {
    MapDocument doc;
    doc.sceneryNames.push_back("a.bmp");
    for (int i = 0; i < 3; ++i) {
        EditorScenery sc{};
        sc.x = static_cast<float>(i) * 10;
        doc.scenery.push_back(sc);
    }
    doc.scenery[0].selected = true;
    EXPECT(doc.cycleSelection(false));
    EXPECT(!doc.scenery[0].selected);
    EXPECT(doc.scenery[1].selected);
    EXPECT(doc.cycleSelection(true));
    EXPECT(doc.scenery[0].selected);
}

TEST(tab_noop_multi_selection) {
    MapDocument doc;
    EditorPoly p{};
    p.v[0].world = {0, 0}; p.v[1].world = {10, 0}; p.v[2].world = {0, 10};
    doc.addPoly(p);
    doc.addPoly(p);
    for (int i = 0; i < 3; ++i) {
        doc.polys[0].v[i].selected = true;
        doc.polys[1].v[i].selected = true;
    }
    EXPECT(!doc.cycleSelection(false));
}


TEST(compile_reproduces_shipped_sector_division) {
    /* Every map in maps/ was produced by the original SaveAndCompile, which
       derives sectorsDivision from the *half* map extents (frm:2607-2611).
       Recompiling an untouched map must reproduce the shipped value.
       (VB6's native SaveMap deliberately uses the *full* extents instead --
       frm:5235 -- so the two paths legitimately disagree.) */
    const std::string mapsDir = "maps";
    if (!fs::exists(mapsDir)) {
        std::fprintf(stderr, "  [SKIP] maps/ directory not found\n");
        return;
    }
    int checked = 0, failed = 0;
    for (const auto& entry : fs::directory_iterator(mapsDir)) {
        auto ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext != ".pms") continue;
        const std::string path = entry.path().string();
        PmsData orig;
        std::string err;
        if (loadPmsFile(path, orig, err) != PmsLoadResult::OK) continue;

        MapDocument doc;
        pmsDataToDoc(orig, doc);
        PmsData mid;
        docToPmsData(doc, mid);

        const std::string tmp = (fs::temp_directory_path() /
                                 "pw_sectordiv_test.pms").string();
        if (!compilePms(tmp, mid, err)) continue;
        PmsData out;
        if (loadPmsFile(tmp, out, err) != PmsLoadResult::OK) continue;
        fs::remove(tmp);

        /* DesertWind.pms stores 31 but its own geometry yields 30 under the
           original formula, so it was not produced by this version of
           SaveAndCompile.  It is the only such map in maps/. */
        if (entry.path().filename() == "DesertWind.pms") continue;

        ++checked;
        if (out.sectorDiv != orig.sectorDiv) {
            ++failed;
            if (failed <= 3)
                std::fprintf(stderr, "  %s: sectorDiv %d -> %d\n",
                             path.c_str(), orig.sectorDiv, out.sectorDiv);
        }
    }
    std::fprintf(stdout, "  sectorDiv: %d maps checked, %d failed\n",
                 checked, failed);
    EXPECT(checked > 90);
    EXPECT_EQ(failed, 0);
}

/* ---- Coordinate / viewport math ---------------------------------------- */

TEST(screen_world_roundtrip_at_all_zooms) {
    MapDocument doc;
    const float zooms[]   = {0.0625f, 0.25f, 0.5f, 1.0f, 2.0f, 8.0f};
    const float scrolls[] = {-1500.0f, 0.0f, 731.5f};
    for (float z : zooms) {
        for (float s : scrolls) {
            doc.zoom = z;
            doc.scrollX = s;
            doc.scrollY = -s;
            for (float sx = 0.0f; sx <= 1200.0f; sx += 137.0f) {
                for (float sy = 0.0f; sy <= 800.0f; sy += 91.0f) {
                    Vec2 w = doc.screenToWorld({sx, sy});
                    Vec2 b = doc.worldToScreen(w);
                    EXPECT_NEAR(b.x, sx, 0.01f);
                    EXPECT_NEAR(b.y, sy, 0.01f);
                }
            }
        }
    }
}

TEST(screen_to_world_honours_zoom) {
    MapDocument doc;
    doc.scrollX = 0.0f;
    doc.scrollY = 0.0f;

    doc.zoom = 1.0f;
    Vec2 w1 = doc.screenToWorld({400.0f, 300.0f});
    EXPECT_NEAR(w1.x, 400.0f, 0.001f);
    EXPECT_NEAR(w1.y, 300.0f, 0.001f);

    /* At 25% zoom the same pixel must map four times further into the world. */
    doc.zoom = 0.25f;
    Vec2 w2 = doc.screenToWorld({400.0f, 300.0f});
    EXPECT_NEAR(w2.x, 1600.0f, 0.001f);
    EXPECT_NEAR(w2.y, 1200.0f, 0.001f);

    /* ...and scroll must not be scaled by zoom. */
    doc.scrollX = -1000.0f;
    Vec2 w3 = doc.screenToWorld({400.0f, 0.0f});
    EXPECT_NEAR(w3.x, 600.0f, 0.001f);
}

TEST(setzoom_keeps_world_point_under_cursor) {
    MapDocument doc;
    doc.zoom = 1.0f;
    doc.scrollX = 250.0f;
    doc.scrollY = -125.0f;
    const float cx = 512.0f, cy = 384.0f;
    Vec2 before = doc.screenToWorld({cx, cy});
    doc.setZoom(0.25f, cx, cy);
    Vec2 after = doc.screenToWorld({cx, cy});
    EXPECT_NEAR(after.x, before.x, 0.01f);
    EXPECT_NEAR(after.y, before.y, 0.01f);
    EXPECT_NEAR(doc.zoom, 0.25f, 0.0001f);
}

TEST(zoomscroll_in_anchors_cursor) {
    /* VB6 frm:4129 — zooming in keeps the world point under the cursor. */
    MapDocument doc;
    doc.zoom = 1.0f;
    doc.scrollX = 100.0f;
    doc.scrollY = 200.0f;
    const float cx = 300.0f, cy = 150.0f;
    Vec2 before = doc.screenToWorld({cx, cy});
    EXPECT(doc.zoomScroll(1.25f, cx, cy, 800.0f, 600.0f, 0.03125f, 512.0f));
    EXPECT_NEAR(doc.zoom, 1.25f, 0.0001f);
    Vec2 after = doc.screenToWorld({cx, cy});
    EXPECT_NEAR(after.x, before.x, 0.01f);
    EXPECT_NEAR(after.y, before.y, 0.01f);
}

TEST(zoomscroll_out_anchors_viewport_centre) {
    /* VB6 frm:4132 — zooming out ignores the cursor and anchors the centre. */
    MapDocument doc;
    doc.zoom = 1.0f;
    doc.scrollX = 100.0f;
    doc.scrollY = 200.0f;
    const float viewW = 800.0f, viewH = 600.0f;
    Vec2 centreBefore = doc.screenToWorld({viewW * 0.5f, viewH * 0.5f});
    EXPECT(doc.zoomScroll(0.8f, 12.0f, 7.0f, viewW, viewH, 0.03125f, 512.0f));
    EXPECT_NEAR(doc.zoom, 0.8f, 0.0001f);
    Vec2 centreAfter = doc.screenToWorld({viewW * 0.5f, viewH * 0.5f});
    EXPECT_NEAR(centreAfter.x, centreBefore.x, 0.01f);
    EXPECT_NEAR(centreAfter.y, centreBefore.y, 0.01f);
}

TEST(zoomscroll_clamps_to_limits) {
    MapDocument doc;
    doc.zoom = 1.1f;
    /* Step would overshoot the maximum, so it is shortened to land on it. */
    EXPECT(doc.zoomScroll(1.25f, 0.0f, 0.0f, 800.0f, 600.0f, 0.5f, 1.25f));
    EXPECT_NEAR(doc.zoom, 1.25f, 0.0001f);
    /* Already at the maximum: the gesture is discarded entirely. */
    EXPECT(!doc.zoomScroll(1.25f, 0.0f, 0.0f, 800.0f, 600.0f, 0.5f, 1.25f));
    EXPECT_NEAR(doc.zoom, 1.25f, 0.0001f);

    doc.zoom = 0.55f;
    EXPECT(doc.zoomScroll(0.8f, 0.0f, 0.0f, 800.0f, 600.0f, 0.5f, 1.25f));
    EXPECT_NEAR(doc.zoom, 0.5f, 0.0001f);
    EXPECT(!doc.zoomScroll(0.8f, 0.0f, 0.0f, 800.0f, 600.0f, 0.5f, 1.25f));
    EXPECT_NEAR(doc.zoom, 0.5f, 0.0001f);
}

TEST(zoomscroll_step_is_gentle) {
    /* Regression for the macOS trackpad report: one wheel notch must change
       zoom by 25%, not by a whole power-of-two zoom level. */
    MapDocument doc;
    doc.zoom = 1.0f;
    doc.zoomScroll(1.25f, 400.0f, 300.0f, 800.0f, 600.0f, 0.03125f, 512.0f);
    EXPECT(doc.zoom < 1.5f);
    EXPECT(doc.zoom > 1.0f);
}

TEST(screen_cache_matches_transform_after_zoom) {
    MapDocument doc;
    EditorPoly p{};
    p.v[0].world = Vec2{-100.0f, -50.0f};
    p.v[1].world = Vec2{100.0f, -50.0f};
    p.v[2].world = Vec2{0.0f, 80.0f};
    doc.addPoly(p);
    doc.zoomScroll(0.8f, 400.0f, 300.0f, 800.0f, 600.0f, 0.03125f, 512.0f);
    for (const auto& p : doc.polys)
        for (int i = 0; i < 3; ++i) {
            Vec2 expect = doc.worldToScreen(p.v[i].world);
            EXPECT_NEAR(p.v[i].screen.x, expect.x, 0.001f);
            EXPECT_NEAR(p.v[i].screen.y, expect.y, 0.001f);
        }
}


/* ---- Asset resolution (portable distribution layout) -------------------- */

/* Builds the directory layout of an extracted portable release:
 *
 *   <root>/PolyWorks/skins/default/notfound.bmp
 *   <root>/PolyWorks/Textures/riverbed.bmp
 *   <root>/PolyWorks/Scenery-gfx/Crate.bmp
 *   <root>/elsewhere/mymap.pms
 *   <root>/elsewhere/Textures/riverbed.bmp     (map-relative override)
 */
namespace {
struct AssetTree {
    fs::path root;
    fs::path app;
    fs::path elsewhere;

    AssetTree() {
        std::error_code ec;
        root = fs::temp_directory_path(ec) / "pw_assets_test";
        fs::remove_all(root, ec);
        app = root / "PolyWorks";
        elsewhere = root / "elsewhere";
        fs::create_directories(app / "skins" / "default", ec);
        fs::create_directories(app / "Textures", ec);
        fs::create_directories(app / "Scenery-gfx", ec);
        fs::create_directories(elsewhere / "Textures", ec);
        write(app / "skins" / "default" / "notfound.bmp");
        write(app / "Textures" / "riverbed.bmp");
        write(app / "Scenery-gfx" / "Crate.bmp");
        write(elsewhere / "Textures" / "riverbed.bmp");
    }
    ~AssetTree() { std::error_code ec; fs::remove_all(root, ec); }

    static void write(const fs::path& p) {
        FILE* f = std::fopen(p.string().c_str(), "wb");
        if (f != nullptr) { std::fputs("BM", f); std::fclose(f); }
    }
};
}  // namespace

/* setBasePath() replaces the search list rather than adding to it: the skin
   directory and its parent must be the only entries afterwards.  MainFrame
   relies on this ordering when it re-registers the application asset paths. */
TEST(texman_setbasepath_replaces_search_paths) {
    AssetTree t;
    TextureManager tm;
    tm.addSearchPath((t.root / "stale").string());
    tm.setBasePath((t.app / "skins" / "default").string());
    EXPECT(!tm.resolvePath("notfound.bmp").empty());
    tm.setBasePath((t.app / "Textures").string());
    EXPECT(tm.resolvePath("notfound.bmp").empty());
}

/* An extracted portable release must find its bundled artwork with nothing
   configured: the executable's own directory is the whole story. */
TEST(texman_resolves_from_app_relative_dirs) {
    AssetTree t;
    TextureManager tm;
    tm.setBasePath((t.app / "skins" / "default").string());
    tm.addSearchPath((t.app / "Textures").string());
    tm.addSearchPath((t.app / "Scenery-gfx").string());

    EXPECT(!tm.resolvePath("riverbed.bmp").empty());
    EXPECT(!tm.resolvePath("Crate.bmp").empty());
    EXPECT(!tm.resolvePath("notfound.bmp").empty());
    EXPECT(tm.resolvePath("no_such_asset.bmp").empty());
}

/* Maps are authored on Windows, where "Crate.bmp" and "crate.bmp" name the
   same file.  Resolution must stay case-insensitive on case-sensitive
   filesystems or most real maps lose their scenery on Linux and macOS. */
TEST(texman_resolution_is_case_insensitive) {
    AssetTree t;
    TextureManager tm;
    tm.setBasePath((t.app / "skins" / "default").string());
    tm.addSearchPath((t.app / "Scenery-gfx").string());
    EXPECT(!tm.resolvePath("crate.bmp").empty());
    EXPECT(!tm.resolvePath("CRATE.BMP").empty());
    EXPECT(!tm.resolvePath("Crate.BMP").empty());
}

/* addSearchPath() prepends, so the most recently registered directory is
   searched first.  MainFrame registers the application directories once at
   startup and the map-relative ones on every open, which is what makes a map
   kept beside its own Textures folder use that copy rather than whatever
   happens to be bundled with the editor. */
TEST(texman_map_relative_paths_win_over_app_paths) {
    AssetTree t;
    TextureManager tm;
    tm.setBasePath((t.app / "skins" / "default").string());
    tm.addSearchPath((t.app / "Textures").string());       /* at startup */
    tm.addSearchPath((t.elsewhere / "Textures").string()); /* on map open */

    const std::string resolved = tm.resolvePath("riverbed.bmp");
    EXPECT(!resolved.empty());
    EXPECT(resolved.find("elsewhere") != std::string::npos);
}

/* A path that already names a real file is used as given, so maps that store
   an absolute or already-rooted texture path keep working. */
TEST(texman_absolute_path_used_verbatim) {
    AssetTree t;
    TextureManager tm;
    tm.setBasePath((t.app / "skins" / "default").string());
    const std::string direct = (t.elsewhere / "Textures" / "riverbed.bmp").string();
    EXPECT(tm.resolvePath(direct) == direct);
}

/* Duplicate registrations are collapsed, so reopening maps in the same
   directory cannot grow the search list without bound. */
TEST(texman_search_paths_are_deduplicated) {
    AssetTree t;
    TextureManager tm;
    tm.setBasePath((t.app / "skins" / "default").string());
    for (int i = 0; i < 100; ++i) {
        tm.addSearchPath((t.app / "Textures").string());
    }
    EXPECT(!tm.resolvePath("riverbed.bmp").empty());
    EXPECT(tm.searchPathCount() <= 3);
}

/* ---- Main -------------------------------------------------------------- */

int main() {
    for (auto& tc : g_tests) {
        g_currentTest = tc.name;
        tc.fn();
    }
    std::fprintf(stdout, "\n%d passed, %d failed.\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
