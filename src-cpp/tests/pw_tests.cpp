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

/* ---- Main -------------------------------------------------------------- */

int main() {
    for (auto& tc : g_tests) {
        g_currentTest = tc.name;
        tc.fn();
    }
    std::fprintf(stdout, "\n%d passed, %d failed.\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
