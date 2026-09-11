/*
 * context_menu.cpp — the viewport right-click menus.
 *
 * Form_MouseDown (frm:11113-11144) chooses one of five popups from the tool
 * that is active, and does nothing at all for the rest:
 *
 *   currentFunction CREATE or QUAD  -> mnuPolyTypes
 *   currentTool     MOVE            -> mnuMove        (at the click point)
 *   currentTool     PSELECT/VSELECT -> mnuVertexSelect
 *   currentFunction SCENERY         -> the scenery tree, toggled
 *   currentFunction OBJECTS         -> mnuObjects     (at the click point)
 *   currentFunction WAYPOINT        -> mnuWaypoint    (at the click point)
 *
 * Note the deliberate mix of currentTool and currentFunction: Move, Polygon
 * Select and Vertex Select test the *chosen* tool, so their menus still appear
 * when a modifier has temporarily switched the effective function, while the
 * others test the effective one.  That asymmetry is reproduced here.
 */

#include "app.h"
#include "editor.h"

#include "imgui.h"

#include <string>

namespace pw {
namespace {

/* mnuSpawn, in index order (frm:952-1030).  The index *is* the team value
   written into the PMS, so the order matters beyond presentation. */
const char* const kSpawnNames[] = {
    "Player Spawn",  "Alpha Team",     "Bravo Team",    "Charlie Team",
    "Delta Team",    "Alpha Flag",     "Bravo Flag",    "Grenade Kit",
    "Medikit",       "Cluster Grenades", "Vest",        "Flame",
    "Berserker",     "Predator",       "Point Match Flag", "Rambo Bow",
    "Stat Gun",
};
constexpr int kSpawnCount =
    static_cast<int>(sizeof(kSpawnNames) / sizeof(kSpawnNames[0]));

/* mnuPolyType (frm:1036-1146). */
const char* const kPolyTypeNames[] = {
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
    "Blue Bullets Collides",
    "Blue Players Collide",
    "Yellow Bullets Collides",
    "Yellow Players Collide",
    "Green Bullets Collides",
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
constexpr int kPolyTypeCount =
    static_cast<int>(sizeof(kPolyTypeNames) / sizeof(kPolyTypeNames[0]));

/* mnuWayType (frm:1165-1185).  Left/Right/Up/Down/Fly. */
const char* const kWayTypeNames[] = {"Left", "Right", "Up", "Down", "Fly"};

void drawPolyTypesMenu(Editor& ed) {
    /* mnuPolyType_Click (frm:14492) sets the creation type only -- it does
       not restamp anything already on the map. */
    for (int i = 0; i < kPolyTypeCount; ++i) {
        if (ImGui::MenuItem(kPolyTypeNames[i], nullptr,
                            ed.activeTool != TOOL_QUAD &&
                                ed.creationPolyType == i)) {
            ed.creationPolyType = i;
            ed.setActiveTool(TOOL_CREATE);
        }
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Textured Quad", nullptr,
                        ed.activeTool == TOOL_QUAD)) {
        ed.setActiveTool(TOOL_QUAD);
    }
}

void drawMoveMenu(Editor& ed, Vec2 clickWorld) {
    /* mnuSetRCenter pins the pivot where the right-click landed; the original
       stashes mouseCoords before showing the menu for exactly this. */
    if (ImGui::MenuItem("Set Reference Point", nullptr,
                        ed.doc.rCenterMode == MapDocument::RCenterMode::Set)) {
        ed.doc.rCenterMode = MapDocument::RCenterMode::Set;
        ed.doc.rCenter = clickWorld;
        ed.needsRedraw = true;
    }
    if (ImGui::MenuItem("Center Reference Point", nullptr,
                        ed.doc.rCenterMode ==
                            MapDocument::RCenterMode::Center)) {
        ed.doc.rCenterMode = MapDocument::RCenterMode::Center;
        ed.doc.updateRCenter();
        ed.needsRedraw = true;
    }
    if (ImGui::MenuItem("Fixed Reference Point", nullptr,
                        ed.doc.rCenterMode ==
                            MapDocument::RCenterMode::Fixed)) {
        ed.doc.rCenterMode = MapDocument::RCenterMode::Fixed;
        ed.doc.updateRCenter();
        ed.needsRedraw = true;
    }
}

void drawVertexSelectMenu(Editor& ed) {
    const bool any = ed.doc.anySelected();
    if (ImGui::MenuItem("Duplicate", nullptr, false, any)) {
        ed.duplicateSelected();
    }
    if (ImGui::MenuItem("Copy", nullptr, false, any)) {
        ed.copySelection();
    }
    if (ImGui::MenuItem("Paste")) {
        ed.pasteSelection();
    }
    if (ImGui::MenuItem("Clear", nullptr, false, any)) {
        ed.deleteSelected();
    }
    ImGui::Separator();
    if (ImGui::BeginMenu("Arrange", any)) {
        if (ImGui::MenuItem("Bring To Front")) {
            ed.arrangeSelected(0);
        }
        if (ImGui::MenuItem("Bring Forward")) {
            ed.arrangeSelected(2);
        }
        if (ImGui::MenuItem("Send Backward")) {
            ed.arrangeSelected(3);
        }
        if (ImGui::MenuItem("Send To Back")) {
            ed.arrangeSelected(1);
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Transform", any)) {
        if (ImGui::MenuItem("Rotate 180")) {
            ed.transformSelection(2);
        }
        if (ImGui::MenuItem("Rotate 90 CW")) {
            ed.transformSelection(3);
        }
        if (ImGui::MenuItem("Rotate 90 CCW")) {
            ed.transformSelection(4);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Flip Horizontal")) {
            ed.transformSelection(0);
        }
        if (ImGui::MenuItem("Flip Vertical")) {
            ed.transformSelection(1);
        }
        ImGui::EndMenu();
    }
}

void drawObjectsMenu(Editor& ed) {
    for (int i = 0; i < kSpawnCount; ++i) {
        if (ImGui::MenuItem(kSpawnNames[i], nullptr,
                            !ed.doc.showGostek && ed.currentSpawnTeam == i)) {
            ed.currentSpawnTeam = i;
            ed.doc.showGostek = false;
            ed.needsRedraw = true;
        }
    }
    ImGui::Separator();
    /* mnuCollider: the original uses a sentinel rather than a spawn index,
       because a collider is a different record in the PMS. */
    if (ImGui::MenuItem("Collider", nullptr,
                        !ed.doc.showGostek && ed.currentSpawnTeam < 0)) {
        ed.currentSpawnTeam = -1;
        ed.doc.showGostek = false;
        ed.needsRedraw = true;
    }
    ImGui::Separator();
    /* mnuGostek (frm:14461) toggles the scale reference figure, which the
       next click then positions rather than placing an object. */
    if (ImGui::MenuItem("Gostek", nullptr, ed.doc.showGostek)) {
        ed.doc.showGostek = !ed.doc.showGostek;
        ed.needsRedraw = true;
    }
}

void drawWaypointMenu(Editor& ed) {
    /* mnuWayType_Click toggles one flag; several can be set at once, which is
       why these are checkable items and not a radio group. */
    for (int i = 0; i < 5; ++i) {
        if (ImGui::MenuItem(kWayTypeNames[i], nullptr,
                            ed.waypointState.type[i])) {
            ed.toggleWaypointType(i);
        }
    }
}

}  // namespace

void drawViewportContextMenu(App& app, Vec2 clickWorld) {
    Editor& ed = app.editor();
    const int tool = ed.activeTool;
    const int fn = ed.interaction.currentFunction();

    const char* id = nullptr;
    if (fn == TOOL_CREATE || fn == TOOL_QUAD) {
        id = "##ctxPolyTypes";
    } else if (tool == TOOL_MOVE) {
        id = "##ctxMove";
    } else if (tool == TOOL_PSELECT || tool == TOOL_VSELECT) {
        id = "##ctxVertexSelect";
    } else if (fn == TOOL_SCENERY) {
        /* The original pops up a scenery *tree* placed at the cursor.  The
           equivalent here is the Scenery panel, which is always available, so
           right-click toggles it exactly as the original toggles the tree. */
        ed.panels.scenery = !ed.panels.scenery;
        return;
    } else if (fn == TOOL_OBJECTS) {
        id = "##ctxObjects";
    } else if (fn == TOOL_WAYPOINT) {
        id = "##ctxWaypoint";
    } else {
        return;   /* every other tool: right-click does nothing */
    }

    ImGui::OpenPopup(id);
    app.setContextMenu(id, clickWorld);
}

/* Drawn every frame from the frame loop so the popup survives the click that
   opened it; ImGui popups only stay open while their Begin is reached. */
void drawOpenContextMenu(App& app) {
    const char* id = app.contextMenuId();
    if (!id) {
        return;
    }
    Editor& ed = app.editor();
    if (ImGui::BeginPopup(id)) {
        if (std::string(id) == "##ctxPolyTypes") {
            drawPolyTypesMenu(ed);
        } else if (std::string(id) == "##ctxMove") {
            drawMoveMenu(ed, app.contextMenuWorld());
        } else if (std::string(id) == "##ctxVertexSelect") {
            drawVertexSelectMenu(ed);
        } else if (std::string(id) == "##ctxObjects") {
            drawObjectsMenu(ed);
        } else if (std::string(id) == "##ctxWaypoint") {
            drawWaypointMenu(ed);
        }
        ImGui::EndPopup();
    } else {
        app.setContextMenu(nullptr, {});
    }
}

}  // namespace pw
