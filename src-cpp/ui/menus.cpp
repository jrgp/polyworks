/*
 * menus.cpp — the menu bar, the keyboard map and the status bar.
 *
 * The menu tree is the original's, item for item, in its order, with its
 * accelerators.  It is *not* the tree the wxWidgets port built: that one
 * invented top-level Map, Polygon, Arrange and Help menus and moved the
 * display toggles out of the Display window into View.  The authority is
 * frmOpenSoldatMapEditor.frm:531-1400, which has exactly five menus -- File,
 * Edit, Texture, View, Window -- with Arrange and Transform as submenus of
 * Edit, and the polygon types reachable only from the right-click menu.
 *
 * Dear ImGui has no accelerator table, so the shortcuts are handled explicitly
 * in App::handleShortcuts below.  The strings shown beside the menu items and
 * the keys actually tested are kept in the same file so they cannot drift.
 */

#include "app.h"
#include "editor.h"
#include "geometry.h"
#include "platform.h"

#include "imgui.h"

#include <cstdio>

namespace pw {
namespace {

bool item(const char* label, const char* shortcut = nullptr,
          bool enabled = true) {
    return ImGui::MenuItem(label, shortcut, false, enabled);
}

bool check(const char* label, bool* value, const char* shortcut = nullptr) {
    return ImGui::MenuItem(label, shortcut, value);
}

void drawFileMenu(App& app) {
    Editor& ed = app.editor();
    if (!ImGui::BeginMenu("File")) {
        return;
    }
    if (item("New", "Ctrl+N")) {
        ed.confirmDiscardChanges([&ed]() { ed.newMap(); });
    }
    /* mnuOpen_Click (frm:12772) raises the same save prompt mnuNew_Click does
       before it ever shows the file dialog. */
    if (item("Open...", "Ctrl+O")) {
        ed.confirmDiscardChanges(
            [&app]() { app.openPopup(PendingPopup::OpenMap); });
    }
    if (item("Open Compiled...")) {
        ed.confirmDiscardChanges(
            [&app]() { app.openPopup(PendingPopup::OpenCompiled); });
    }
    if (ImGui::BeginMenu("Open Recent", !ed.recentFiles.empty())) {
        int index = 0;
        for (const std::string& path : ed.recentFiles) {
            char label[512];
            std::snprintf(label, sizeof(label), "%d  %s", ++index, path.c_str());
            if (ImGui::MenuItem(label)) {
                const std::string copy = path;
                ed.confirmDiscardChanges([&ed, copy]() { ed.loadMap(copy); });
                break;   /* loading reorders the deque under the iterator */
            }
        }
        ImGui::EndMenu();
    }
    ImGui::Separator();
    if (item("Save...", "Ctrl+S")) {
        if (ed.currentFilePath.empty()) {
            app.openPopup(PendingPopup::SaveMapAs);
        } else {
            ed.saveCurrent();
        }
    }
    if (item("Save As...")) {
        app.openPopup(PendingPopup::SaveMapAs);
    }
    ImGui::Separator();
    if (item("Compile to pms")) {
        if (ed.currentFilePath.empty()) {
            app.openPopup(PendingPopup::CompileAs);
        } else {
            ed.compileTo(ed.currentFilePath);
        }
    }
    if (item("Compile to pms As...", "F9")) {
        app.openPopup(PendingPopup::CompileAs);
    }
    ImGui::Separator();
    if (item("Export...")) {
        app.openPopup(PendingPopup::ExportPrefab);
    }
    if (item("Import...")) {
        app.openPopup(PendingPopup::ImportPrefab);
    }
    ImGui::Separator();
    if (item("Run OpenSoldat", "F8")) {
        ed.runGame(true);
    }
    if (item("Run Soldat", "Shift+F8")) {
        ed.runGame(false);
    }
    ImGui::Separator();
    if (item("Exit")) {
        ed.confirmDiscardChanges([&app]() { app.requestQuit(); });
    }
    ImGui::EndMenu();
}

void drawEditMenu(App& app) {
    Editor& ed = app.editor();
    if (!ImGui::BeginMenu("Edit")) {
        return;
    }
    if (item("Undo", "Ctrl+Z", ed.undo.canUndo())) {
        ed.undoAction();
    }
    if (item("Redo", "Ctrl+Y", ed.undo.canRedo())) {
        ed.redoAction();
    }
    ImGui::Separator();
    const bool sel = ed.doc.anySelected();
    if (item("Duplicate", nullptr, sel)) {
        ed.duplicateSelected();
    }
    if (item("Copy", "Ctrl+C", sel)) {
        ed.copySelection();
    }
    if (item("Paste", "Ctrl+V")) {
        ed.pasteSelection();
    }
    if (item("Clear", "Del", sel)) {
        ed.deleteSelected();
    }
    ImGui::Separator();
    if (item("Select All", "Ctrl+A")) {
        ed.selectAll();
    }
    if (item("Invert Selection", "Ctrl+I")) {
        ed.invertSelection();
    }
    if (item("Deselect", "Ctrl+D")) {
        ed.deselect();
    }
    if (item("Select by Color", "Ctrl+B")) {
        ed.selectByColor();
    }
    ImGui::Separator();
    if (ImGui::BeginMenu("Arrange", sel)) {
        if (item("Bring to Front")) {
            ed.arrangeSelected(0);
        }
        if (item("Bring Forward")) {
            ed.arrangeSelected(2);
        }
        if (item("Send Backward")) {
            ed.arrangeSelected(3);
        }
        if (item("Send to Back")) {
            ed.arrangeSelected(1);
        }
        ImGui::EndMenu();
    }
    ImGui::Separator();
    if (item("Split at Vertex", "Ctrl+L", sel)) {
        ed.polyOperation(0);
    }
    if (item("Join Vertices", "Ctrl+J", sel)) {
        ed.polyOperation(1);
    }
    if (item("Snap Selected Vertices", nullptr, sel)) {
        ed.snapSelectedVertices();
    }
    if (item("Create with Selected", "Ctrl+E", sel)) {
        ed.polyOperation(2);
    }
    if (ImGui::BeginMenu("Transform", sel)) {
        if (item("Rotate 180")) {
            ed.transformSelection(2);
        }
        if (item("Rotate 90 CW")) {
            ed.transformSelection(3);
        }
        if (item("Rotate 90 CCW")) {
            ed.transformSelection(4);
        }
        ImGui::Separator();
        if (item("Flip Horizontal")) {
            ed.transformSelection(0);
        }
        if (item("Flip Vertical")) {
            ed.transformSelection(1);
        }
        ImGui::EndMenu();
    }
    ImGui::Separator();
    if (item("Sever Connections", nullptr, sel)) {
        ed.severConnections();
    }
    ImGui::Separator();
    if (item("Clear sketch")) {
        ed.clearSketch();
    }
    ImGui::Separator();
    if (item("Map Settings...", "Ctrl+M")) {
        app.openPopup(PendingPopup::MapSettings);
    }
    ImGui::Separator();
    if (item("Preferences...", "Ctrl+P")) {
        app.openPopup(PendingPopup::Preferences);
    }
    ImGui::EndMenu();
}

void drawTextureMenu(App& app) {
    Editor& ed = app.editor();
    if (!ImGui::BeginMenu("Texture")) {
        return;
    }
    const bool sel = ed.doc.anySelected();
    if (item("Fix Texture", "Ctrl+F", sel)) {
        ed.polyOperation(3);
    }
    if (item("Untexture", "Ctrl+U", sel)) {
        ed.polyOperation(4);
    }
    if (ImGui::BeginMenu("Transform Texture", sel)) {
        if (item("Rotate 180")) {
            ed.texTransform(2);
        }
        if (item("Rotate 90 CW")) {
            ed.texTransform(3);
        }
        if (item("Rotate 90 CCW")) {
            ed.texTransform(4);
        }
        ImGui::Separator();
        if (item("Flip Horizontal")) {
            ed.texTransform(0);
        }
        if (item("Flip Vertical")) {
            ed.texTransform(1);
        }
        ImGui::EndMenu();
    }
    ImGui::Separator();
    if (item("Average Vertex Colors", "Ctrl+G", sel)) {
        ed.polyOperation(5);
    }
    if (item("Apply Light to Vertices")) {
        ed.applyLightToVertices();
    }
    ImGui::Separator();
    check("Fixed Texture", &ed.doc.viewSettings.fixedTexture);
    check("User Defined X", &ed.customTexX);
    check("User Defined Y", &ed.customTexY);
    ImGui::EndMenu();
}

void drawViewMenu(App& app) {
    Editor& ed = app.editor();
    ViewSettings& vs = ed.doc.viewSettings;
    if (!ImGui::BeginMenu("View")) {
        return;
    }
    if (item("Zoom In")) {
        ed.zoomIn();
    }
    if (item("Zoom Out")) {
        ed.zoomOut();
    }
    if (item("Fit on Screen")) {
        ed.fitOnScreen();
    }
    if (item("Actual Size")) {
        ed.zoomReset();
    }
    if (item("Reset View")) {
        ed.centerAndReset();
    }
    ImGui::Separator();
    check("Show Grid", &vs.showGrid);
    check("Snap to Grid", &vs.snapToGrid);
    check("Snap to Vertices", &vs.snapToVertices);
    ImGui::Separator();
    check("Blend Wireframe", &vs.blendWireframe);
    check("Blend Polys", &vs.blendPolys);
    if (ImGui::BeginMenu("Show Scenery Layers")) {
        check("Back", &vs.showSceneryBack);
        check("Middle", &vs.showSceneryMiddle);
        check("Front", &vs.showSceneryFront);
        ImGui::EndMenu();
    }
    ImGui::Separator();
    if (item("Refresh", "F5")) {
        /* mnuRefreshBG_Click (frm:14057) re-reads the background quad and the
           scenery the map references, which is how a texture edited outside
           PolyWorks is picked up without reopening the map. */
        ed.doc.rebuildScreenCache();
        ed.refreshSceneryInUse();
        ed.needsRedraw = true;
    }
    ImGui::EndMenu();
}

void drawWindowMenu(App& app) {
    Editor& ed = app.editor();
    if (!ImGui::BeginMenu("Window")) {
        return;
    }
    if (ImGui::BeginMenu("Workspace")) {
        if (item("Load Workspace...")) {
            app.openPopup(PendingPopup::LoadWorkspace);
        }
        if (item("Save Workspace...")) {
            app.openPopup(PendingPopup::SaveWorkspace);
        }
        if (item("Reset Window Locations")) {
            ed.panels = PanelVisibility{};
            /* Dropping the stored geometry makes every panel fall back to the
               position panels.cpp gives it when it first appears. */
            ImGui::LoadIniSettingsFromMemory("", 0);
        }
        ImGui::EndMenu();
    }
    if (item("Show All")) {
        ed.panels.tools = true;
        ed.panels.display = true;
        ed.panels.palette = true;
        ed.panels.waypoints = true;
        ed.panels.scenery = true;
        ed.panels.properties = true;
        ed.panels.texture = true;
    }
    if (item("Hide All")) {
        ed.panels.tools = false;
        ed.panels.display = false;
        ed.panels.palette = false;
        ed.panels.waypoints = false;
        ed.panels.scenery = false;
        ed.panels.properties = false;
        ed.panels.texture = false;
    }
    ImGui::Separator();
    check("Tools", &ed.panels.tools);
    check("Display", &ed.panels.display);
    check("Palette", &ed.panels.palette);
    check("Waypoints", &ed.panels.waypoints);
    check("Scenery", &ed.panels.scenery);
    check("Properties", &ed.panels.properties);
    check("Texture", &ed.panels.texture);
    ImGui::EndMenu();
}

}  // namespace

void drawMenuBar(App& app) {
    if (!ImGui::BeginMainMenuBar()) {
        return;
    }
    drawFileMenu(app);
    drawEditMenu(app);
    drawTextureMenu(app);
    drawViewMenu(app);
    drawWindowMenu(app);
    ImGui::EndMainMenuBar();
}

/* ---- status bar --------------------------------------------------------- */

void App::drawStatusBar(float height) {
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0.0f, io.DisplaySize.y - height));
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, height));
    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    if (ImGui::Begin("##statusbar", nullptr, flags)) {
        /* The four panels of StatusBar1 (frm:1690), in its order: cursor
           position, file name, zoom, current tool. */
        ImGui::Text("X: %.0f  Y: %.0f", m_editor.lastMouseWorld.x,
                    m_editor.lastMouseWorld.y);
        ImGui::SameLine(170.0f * m_uiScale);
        ImGui::TextUnformatted(m_editor.documentName().c_str());
        ImGui::SameLine(ImGui::GetWindowWidth() - 300.0f * m_uiScale);
        ImGui::Text("Zoom: %.0f%%", m_editor.doc.zoom * 100.0f);
        ImGui::SameLine(ImGui::GetWindowWidth() - 200.0f * m_uiScale);
        ImGui::TextUnformatted(m_editor.statusToolLabel().c_str());
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}

/* ---- keyboard ----------------------------------------------------------- */

void App::handleShortcuts() {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureKeyboard || io.WantTextInput) {
        return;   /* a text field owns the keyboard */
    }

    Editor& ed = m_editor;
    const bool ctrl  = io.KeyCtrl || io.KeySuper;   /* Cmd on macOS */
    const bool shift = io.KeyShift;

    auto pressed = [](ImGuiKey key) { return ImGui::IsKeyPressed(key, false); };

    /* Escape and Tab belong to the interaction machine first: Escape abandons
       a polygon being created before it clears the selection (frm:10760). */
    if (pressed(ImGuiKey_Escape)) {
        if (!ed.interaction.onEscape()) {
            ed.deselect();
        }
        return;
    }
    if (pressed(ImGuiKey_Tab)) {
        ed.interaction.onTab(shift);
        return;
    }

    if (ctrl) {
        if (pressed(ImGuiKey_N)) {
            ed.confirmDiscardChanges([&ed]() { ed.newMap(); });
        } else if (pressed(ImGuiKey_O)) {
            ed.confirmDiscardChanges(
                [this]() { openPopup(PendingPopup::OpenMap); });
        } else if (pressed(ImGuiKey_S)) {
            if (ed.currentFilePath.empty()) {
                openPopup(PendingPopup::SaveMapAs);
            } else {
                ed.saveCurrent();
            }
        } else if (pressed(ImGuiKey_Z)) {
            ed.undoAction();
        } else if (pressed(ImGuiKey_Y)) {
            ed.redoAction();
        } else if (pressed(ImGuiKey_C)) {
            ed.copySelection();
        } else if (pressed(ImGuiKey_V)) {
            ed.pasteSelection();
        } else if (pressed(ImGuiKey_A)) {
            ed.selectAll();
        } else if (pressed(ImGuiKey_I)) {
            ed.invertSelection();
        } else if (pressed(ImGuiKey_D)) {
            ed.deselect();
        } else if (pressed(ImGuiKey_B)) {
            ed.selectByColor();
        } else if (pressed(ImGuiKey_L)) {
            ed.polyOperation(0);
        } else if (pressed(ImGuiKey_J)) {
            ed.polyOperation(1);
        } else if (pressed(ImGuiKey_E)) {
            ed.polyOperation(2);
        } else if (pressed(ImGuiKey_F)) {
            ed.polyOperation(3);
        } else if (pressed(ImGuiKey_U)) {
            ed.polyOperation(4);
        } else if (pressed(ImGuiKey_G)) {
            ed.polyOperation(5);
        } else if (pressed(ImGuiKey_M)) {
            openPopup(PendingPopup::MapSettings);
        } else if (pressed(ImGuiKey_P)) {
            openPopup(PendingPopup::Preferences);
        }
        return;
    }

    /* ---- unmodified keys ------------------------------------------------ */
    if (pressed(ImGuiKey_Delete)) {
        ed.deleteSelected();
        return;
    }
    if (pressed(ImGuiKey_F5)) {
        ed.doc.rebuildScreenCache();
        ed.refreshSceneryInUse();
        ed.needsRedraw = true;
        return;
    }
    if (pressed(ImGuiKey_F8)) {
        ed.runGame(!shift);
        return;
    }
    if (pressed(ImGuiKey_F9)) {
        openPopup(PendingPopup::CompileAs);
        return;
    }

    /* Arrow keys nudge the selection; Shift makes the step one grid
       sub-division (frm:11007). */
    static const struct { ImGuiKey key; float dx, dy; } kArrows[] = {
        {ImGuiKey_LeftArrow, -1.0f, 0.0f},
        {ImGuiKey_RightArrow, 1.0f, 0.0f},
        {ImGuiKey_UpArrow, 0.0f, -1.0f},
        {ImGuiKey_DownArrow, 0.0f, 1.0f},
    };
    for (const auto& a : kArrows) {
        if (ImGui::IsKeyPressed(a.key, true)) {
            ed.nudgeSelection(a.dx, a.dy, shift);
            return;
        }
    }

    /* Numpad 1-8 toggle the Display window's layer checkboxes (frm:10897). */
    ViewSettings& vs = ed.doc.viewSettings;
    bool* const kNumpadToggles[8] = {
        &vs.showPolys,   &vs.showWireframe, &vs.showPoints, &vs.showScenery,
        &vs.showObjects, &vs.showWaypoints, &vs.showLights, &vs.showSketch,
    };
    for (int i = 0; i < 8; ++i) {
        if (pressed(static_cast<ImGuiKey>(ImGuiKey_Keypad1 + i))) {
            *kNumpadToggles[i] = !*kNumpadToggles[i];
            return;
        }
    }

    /* Tool hotkeys, in TOOL_* order: A Q S W D E F R G T H Y J U
       (frm:10800 Form_KeyPress).  Checked after everything above so a
       modifier-bearing shortcut always wins. */
    static const ImGuiKey kToolKeys[kSelectableTools] = {
        ImGuiKey_A, ImGuiKey_Q, ImGuiKey_S, ImGuiKey_W, ImGuiKey_D,
        ImGuiKey_E, ImGuiKey_F, ImGuiKey_R, ImGuiKey_G, ImGuiKey_T,
        ImGuiKey_H, ImGuiKey_Y, ImGuiKey_J, ImGuiKey_U,
    };
    for (int i = 0; i < kSelectableTools; ++i) {
        if (pressed(kToolKeys[i])) {
            ed.setActiveTool(i);
            return;
        }
    }

    /* Waypoint direction keys (frm:10830).  J is claimed by the Lights tool
       above, which is the original's behaviour too. */
    static const struct { ImGuiKey key; int type; } kWayKeys[] = {
        {ImGuiKey_N, 0},   /* Left */
        {ImGuiKey_K, 1},   /* Right */
        {ImGuiKey_I, 2},   /* Up */
        {ImGuiKey_M, 3},   /* Down */
    };
    for (const auto& w : kWayKeys) {
        if (pressed(w.key)) {
            ed.toggleWaypointType(w.type);
            return;
        }
    }
}

}  // namespace pw
