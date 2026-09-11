/*
 * panels.cpp — the seven floating tool windows.
 *
 * The original's palettes are frameless VB6 forms that draw their own title
 * bar into a picture box and drag themselves with ReleaseCapture +
 * WM_NCLBUTTONDOWN (frmTools.frm:300).  That machinery exists only because a
 * borderless window cannot otherwise be moved; under Dear ImGui an ordinary
 * window title bar does the same job, so the hand-rolled title strips are not
 * reproduced.  Everything they contained is.
 *
 * Control inventories come from the forms themselves:
 *   frmTools      14 tool buttons blitted from tool_gfx.bmp
 *   frmDisplay    11 layer toggles, indices 0-10
 *   frmScenery    list, preview, level, rotate/scale, three menu commands
 *   frmWaypoints  5 direction flags, 2 paths, show filter, special, count
 *   frmInfo       6 property sections chosen from a menu
 *   frmPalette    12x6 swatches, RGB/opacity/mode/radius/blend
 *   frmTexture    the map texture with a selectable rectangle
 */

#include "app.h"
#include "editor.h"
#include "geometry.h"
#include "platform.h"

#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>

namespace pw {
namespace {

namespace fs = std::filesystem;

/* A floating panel.  Position and size are only suggestions the first time,
   after which the user's arrangement wins -- the original remembers window
   positions in the workspace file for the same reason. */
bool beginPanel(const char* title, bool* open, float x, float y, float w,
                float h, float scale,
                ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize) {
    /* A negative x means "this far in from the right edge", and a negative y
       "this far up from the status bar", so the right-hand and bottom rows of
       tool windows land on screen whatever the window size is rather than off
       the side or under the status bar of a small one. */
    float px = x * scale;
    if (x < 0.0f) {
        px = ImGui::GetMainViewport()->Size.x + x * scale;
    }
    float py = y * scale;
    ImVec2 pivot(0.0f, 0.0f);
    if (y < 0.0f) {
        /* Pivot on the window's own bottom edge so this works for the
           auto-resizing panels, whose height is not known until they close. */
        py = ImGui::GetIO().DisplaySize.y - ImGui::GetFrameHeight() + y * scale;
        pivot.y = 1.0f;
    }
    ImGui::SetNextWindowPos(ImVec2(px, py), ImGuiCond_FirstUseEver, pivot);
    if (w > 0.0f && h > 0.0f) {
        ImGui::SetNextWindowSize(ImVec2(w * scale, h * scale),
                                 ImGuiCond_FirstUseEver);
    }
    return ImGui::Begin(title, open, flags);
}

/* Text fields in the original are white with black text (TextBoxBack /
   TextBoxText in colors.ini), which is the inverse of the window's own
   colours.  ImGui has one global text colour, so it is pushed per field. */
struct TextFieldColors {
    TextFieldColors() {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1));
    }
    ~TextFieldColors() { ImGui::PopStyleColor(); }
};

bool inputFloatField(const char* label, float* value, float width,
                     const char* fmt = "%.0f") {
    ImGui::SetNextItemWidth(width);
    TextFieldColors colors;
    return ImGui::InputFloat(label, value, 0.0f, 0.0f, fmt,
                             ImGuiInputTextFlags_EnterReturnsTrue);
}

bool inputIntField(const char* label, int* value, float width) {
    ImGui::SetNextItemWidth(width);
    TextFieldColors colors;
    return ImGui::InputInt(label, value, 0, 0,
                           ImGuiInputTextFlags_EnterReturnsTrue);
}

/* ---- Tools (frmTools) --------------------------------------------------- */

void drawToolsPanel(App& app) {
    Editor& ed = app.editor();
    if (!ed.panels.tools) {
        return;
    }
    const float scale = app.uiScale();
    if (beginPanel("Tools", &ed.panels.tools, 8.0f, 40.0f, 0.0f, 0.0f, scale)) {
        const unsigned int sheet = app.toolSheet();
        const float button = 32.0f * scale;

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(1.0f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
        for (int tool = 0; tool < kSelectableTools; ++tool) {
            /* tools.bmp is 64x224: two columns of seven, which is the layout
               frmTools lays its picTools controls out in. */
            if ((tool % 2) != 0) {
                ImGui::SameLine();
            }
            ImGui::PushID(tool);

            const bool selected = (ed.activeTool == tool);
            bool clicked = false;
            if (sheet != 0 && app.toolSheetRows() > tool) {
                /* Column 0 is the normal face and column 2 the pressed one
                   (frmTools.frm:399-405 blits from x=0 and x=64). */
                const int cols = std::max(1, app.toolSheetCols());
                const int rows = std::max(1, app.toolSheetRows());
                const int col = selected ? std::min(2, cols - 1) : 0;
                const ImVec2 uv0(static_cast<float>(col) / cols,
                                 static_cast<float>(tool) / rows);
                const ImVec2 uv1(static_cast<float>(col + 1) / cols,
                                 static_cast<float>(tool + 1) / rows);
                clicked = ImGui::ImageButton(
                    "##tool", static_cast<ImTextureID>(sheet),
                    ImVec2(button, button), uv0, uv1, ImVec4(0, 0, 0, 0),
                    ImVec4(1, 1, 1, 1));
            } else {
                /* Without the skin sheet the buttons still have to work, so
                   they fall back to the tool's hotkey letter. */
                if (selected) {
                    ImGui::PushStyleColor(ImGuiCol_Button,
                                          ImGui::GetStyleColorVec4(
                                              ImGuiCol_ButtonActive));
                }
                clicked = ImGui::Button(toolHotkey(tool), ImVec2(button, button));
                if (selected) {
                    ImGui::PopStyleColor();
                }
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s (%s)", toolName(tool), toolHotkey(tool));
            }
            if (clicked) {
                ed.setActiveTool(tool);
            }
            ImGui::PopID();
        }
        ImGui::PopStyleVar(2);
    }
    ImGui::End();
}

/* ---- Display (frmDisplay) ----------------------------------------------- */

void drawDisplayPanel(App& app) {
    Editor& ed = app.editor();
    if (!ed.panels.display) {
        return;
    }
    ViewSettings& vs = ed.doc.viewSettings;
    const float scale = app.uiScale();
    if (beginPanel("Display", &ed.panels.display, -270.0f, 348.0f, 0.0f, 0.0f,
                   scale)) {
        /* lblLayer(0..10), in the form's own order. */
        struct Layer { const char* label; bool* flag; };
        const Layer layers[] = {
            {"Background", &vs.showBackground},
            {"Polygons",   &vs.showPolys},
            {"Texture",    &vs.showTexture},
            {"Wireframe",  &vs.showWireframe},
            {"Points",     &vs.showPoints},
            {"Scenery",    &vs.showScenery},
            {"Objects",    &vs.showObjects},
            {"Waypoints",  &vs.showWaypoints},
            {"Grid",       &vs.showGrid},
            {"Lights",     &vs.showLights},
            {"Sketch",     &vs.showSketch},
        };
        for (const Layer& layer : layers) {
            ImGui::Checkbox(layer.label, layer.flag);
        }
    }
    ImGui::End();
}

/* ---- Scenery (frmScenery) ----------------------------------------------- */

void drawSceneryPanel(App& app) {
    Editor& ed = app.editor();
    if (!ed.panels.scenery) {
        return;
    }
    SceneryState& st = ed.sceneryState;
    const float scale = app.uiScale();
    if (beginPanel("Scenery", &ed.panels.scenery, -270.0f, -8.0f, 260.0f,
                   230.0f, scale, ImGuiWindowFlags_None)) {
        /* picSceneryMenu opens mnuScenery (frmScenery.frm:1230). */
        if (ImGui::Button("Scenery")) {
            ImGui::OpenPopup("scenery_menu");
        }
        if (ImGui::BeginPopup("scenery_menu")) {
            if (ImGui::MenuItem("Clear Unused")) {
                ed.undo.push(ed.doc);
                ed.doc.clearUnusedScenery();
                ed.doc.markModified();
                ed.refreshSceneryInUse();
            }
            if (ImGui::MenuItem("Reload Scenery List")) {
                st.dir.clear();   /* force a rescan */
                ed.refreshSceneryList();
            }
            if (ImGui::MenuItem("Refresh Scenery")) {
                ed.refreshSceneryInUse();
                ed.needsRedraw = true;
            }
            ImGui::EndPopup();
        }

        /* imgScenery: the sprite that will be placed, at its real size. */
        const float previewH = 96.0f * scale;
        if (st.selected >= 0 &&
            st.selected < static_cast<int>(st.available.size())) {
            int w = 0, h = 0;
            const unsigned int tex =
                app.previewTexture(st.available[static_cast<size_t>(st.selected)],
                                   w, h);
            if (tex != 0 && w > 0 && h > 0) {
                const float fit = std::min(previewH / static_cast<float>(h),
                                           (ImGui::GetContentRegionAvail().x) /
                                               static_cast<float>(w));
                ImGui::Image(static_cast<ImTextureID>(tex),
                             ImVec2(static_cast<float>(w) * std::min(fit, 1.0f),
                                    static_cast<float>(h) * std::min(fit, 1.0f)));
            } else {
                ImGui::TextUnformatted("(image not found)");
            }
        } else {
            ImGui::Dummy(ImVec2(0.0f, previewH * 0.25f));
        }

        ImGui::Separator();
        if (st.available.empty()) {
            /* A silent empty list is the single most confusing failure this
               panel can have: it looks like a broken control rather than a
               missing game directory.  Say which it is. */
            ImGui::TextWrapped(
                "No scenery found.  Set the Soldat directory in "
                "Edit > Preferences.");
        }

        const float listH = ImGui::GetContentRegionAvail().y - 90.0f * scale;
        if (ImGui::BeginListBox("##scenerylist",
                                ImVec2(-FLT_MIN, std::max(60.0f * scale, listH)))) {
            for (int i = 0; i < static_cast<int>(st.available.size()); ++i) {
                const bool selected = (st.selected == i);
                if (ImGui::Selectable(st.available[static_cast<size_t>(i)].c_str(),
                                      selected)) {
                    st.selected = i;
                }
                if (selected && ImGui::IsWindowAppearing()) {
                    ImGui::SetScrollHereY();
                }
            }
            ImGui::EndListBox();
        }

        ImGui::TextUnformatted("Level:");
        ImGui::SameLine();
        if (ImGui::RadioButton("Back", st.level == 0)) {
            st.level = 0;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Middle", st.level == 1)) {
            st.level = 1;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Front", st.level == 2)) {
            st.level = 2;
        }

        /* picRotate / picScale: whether a placement drag also rotates or
           scales the sprite (frmScenery.frm:790). */
        ImGui::Checkbox("Rotate", &st.rotate);
        ImGui::SameLine();
        ImGui::Checkbox("Scale", &st.scale);
    }
    ImGui::End();
}

/* ---- Waypoints (frmWaypoints) ------------------------------------------- */

void drawWaypointsPanel(App& app) {
    Editor& ed = app.editor();
    if (!ed.panels.waypoints) {
        return;
    }
    WaypointState& st = ed.waypointState;
    ViewSettings& vs = ed.doc.viewSettings;
    const float scale = app.uiScale();
    if (beginPanel("Waypoints", &ed.panels.waypoints, 8.0f, -8.0f, 0.0f,
                   0.0f, scale)) {
        /* picType(0..4): the direction flags stamped onto the next waypoint,
           and edited into the selected one. */
        static const char* kTypeLabels[5] = {"Left", "Right", "Up", "Down",
                                             "Fly"};
        bool typeChanged = false;
        for (int i = 0; i < 5; ++i) {
            if (i != 0) {
                ImGui::SameLine();
            }
            if (ImGui::Checkbox(kTypeLabels[i], &st.type[i])) {
                typeChanged = true;
            }
        }

        ImGui::TextUnformatted("Path:");
        ImGui::SameLine();
        bool pathChanged = false;
        if (ImGui::RadioButton("Path 1", st.pathNum == 1)) {
            st.pathNum = 1;
            pathChanged = true;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Path 2", st.pathNum == 2)) {
            st.pathNum = 2;
            pathChanged = true;
        }

        /* cboSpecial: the action code stored in the waypoint's `special`
           field.  The original's list is free-form; the values are what the
           game reads. */
        ImGui::SetNextItemWidth(120.0f * scale);
        bool specialChanged = false;
        {
            TextFieldColors colors;
            specialChanged = ImGui::InputInt("Special", &st.special, 1, 10);
        }

        /* Editing any of the above rewrites the selected waypoints, which is
           what makes these controls an editor rather than a set of defaults
           (frm:11290 SetWaypointProperties). */
        if (typeChanged || pathChanged || specialChanged) {
            bool any = false;
            for (auto& w : ed.doc.waypoints) {
                if (!w.selected) {
                    continue;
                }
                if (!any) {
                    ed.undo.push(ed.doc);
                    any = true;
                }
                w.left  = st.type[0];
                w.right = st.type[1];
                w.up    = st.type[2];
                w.down  = st.type[3];
                w.m2    = st.type[4];
                if (pathChanged) {
                    w.pathNum = st.pathNum;
                }
                if (specialChanged) {
                    w.special = st.special;
                }
            }
            if (any) {
                ed.doc.markModified();
                ed.needsRedraw = true;
            }
        }

        ImGui::Separator();
        ImGui::TextUnformatted("Show:");
        ImGui::SameLine();
        if (ImGui::RadioButton("All", vs.waypointPathFilter == 0)) {
            vs.waypointPathFilter = 0;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Path1", vs.waypointPathFilter == 1)) {
            vs.waypointPathFilter = 1;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Path2", vs.waypointPathFilter == 2)) {
            vs.waypointPathFilter = 2;
        }

        /* lblNumCon: connections on the selected waypoint. */
        int connections = 0;
        int selected = 0;
        for (const auto& w : ed.doc.waypoints) {
            if (w.selected) {
                ++selected;
                connections += static_cast<int>(w.connections.size());
            }
        }
        ImGui::Text("Connections: %d", connections);
        ImGui::Text("Selected: %d of %d", selected,
                    static_cast<int>(ed.doc.waypoints.size()));
    }
    ImGui::End();
}

/* ---- Properties (frmInfo) ----------------------------------------------- */

/* mnuProp(0..5): which of the six property pages is on show. */
enum PropPage {
    PROP_POLYGON = 0,
    PROP_SCENERY,
    PROP_TRANSFORM,
    PROP_TEXTURE,
    PROP_LIGHT,
    PROP_MAPINFO,
    PROP_COUNT
};

void drawPolygonProperties(Editor& ed, float scale) {
    EditorPoly* sel = nullptr;
    int index = -1;
    int count = 0;
    for (size_t i = 0; i < ed.doc.polys.size(); ++i) {
        if (!ed.doc.polys[i].anySelected()) {
            continue;
        }
        ++count;
        if (sel == nullptr) {
            sel = &ed.doc.polys[i];
            index = static_cast<int>(i);
        }
    }
    if (sel == nullptr) {
        ImGui::TextUnformatted("No polygon selected");
        return;
    }
    ImGui::Text("Polygon %d  (%d selected)", index, count);

    int type = sel->polyType;
    ImGui::SetNextItemWidth(180.0f * scale);
    {
        TextFieldColors colors;
        if (ImGui::BeginCombo("Type", polyTypeName(type))) {
            for (int i = 0; i < POLY_TYPE_COUNT; ++i) {
                if (ImGui::Selectable(polyTypeName(i), i == type)) {
                    ed.undo.push(ed.doc);
                    for (auto& p : ed.doc.polys) {
                        if (p.anySelected()) {
                            p.polyType = static_cast<uint8_t>(i);
                        }
                    }
                    ed.doc.markModified();
                    ed.needsRedraw = true;
                }
            }
            ImGui::EndCombo();
        }
    }

    /* txtTexture(0..1): the UV of the first selected vertex, in texture
       pixels, which is how the original presents it. */
    int texW = 0, texH = 0;
    ed.selectedTextureSize(texW, texH);
    const float sw = texW > 0 ? static_cast<float>(texW) : 1.0f;
    const float sh = texH > 0 ? static_cast<float>(texH) : 1.0f;
    const EditorVertex* v = &sel->v[0];
    for (int i = 0; i < 3; ++i) {
        if (sel->v[i].selected) {
            v = &sel->v[i];
            break;
        }
    }
    float tu = v->tu * sw;
    float tv = v->tv * sh;
    const bool tuChanged = inputFloatField("Texture X", &tu, 80.0f * scale);
    const bool tvChanged = inputFloatField("Texture Y", &tv, 80.0f * scale);
    if (tuChanged || tvChanged) {
        ed.undo.push(ed.doc);
        for (auto& p : ed.doc.polys) {
            for (auto& vert : p.v) {
                if (!vert.selected) {
                    continue;
                }
                if (tuChanged) {
                    vert.tu = tu / sw;
                }
                if (tvChanged) {
                    vert.tv = tv / sh;
                }
            }
        }
        ed.doc.markModified();
        ed.needsRedraw = true;
    }

    /* txtVertexAlpha: opacity as a percentage of 255. */
    float opacity = static_cast<float>(v->alpha) * 100.0f / 255.0f;
    if (inputFloatField("Opacity %", &opacity, 80.0f * scale)) {
        const uint8_t alpha = static_cast<uint8_t>(
            std::clamp(opacity, 0.0f, 100.0f) * 255.0f / 100.0f + 0.5f);
        ed.undo.push(ed.doc);
        for (auto& p : ed.doc.polys) {
            for (auto& vert : p.v) {
                if (vert.selected) {
                    vert.alpha = alpha;
                }
            }
        }
        ed.doc.markModified();
        ed.needsRedraw = true;
    }

    /* txtBounciness: only meaningful for POLY_BOUNCY, where 1.0 is 0% extra
       bounce, which is why the field shows a percentage above that. */
    float bounce = (sel->bounciness[0] - 1.0f) * 100.0f;
    if (inputFloatField("Bounciness %", &bounce, 80.0f * scale)) {
        ed.undo.push(ed.doc);
        for (auto& p : ed.doc.polys) {
            if (!p.anySelected()) {
                continue;
            }
            for (float& b : p.bounciness) {
                b = 1.0f + bounce / 100.0f;
            }
        }
        ed.doc.markModified();
        ed.needsRedraw = true;
    }
}

void drawSceneryProperties(Editor& ed, float scale) {
    EditorScenery* sel = nullptr;
    int count = 0;
    for (auto& s : ed.doc.scenery) {
        if (s.selected) {
            ++count;
            if (sel == nullptr) {
                sel = &s;
            }
        }
    }
    if (sel == nullptr) {
        ImGui::TextUnformatted("No scenery selected");
        return;
    }
    ImGui::Text("%d selected", count);

    float scaleX = sel->scaleX * 100.0f;
    float scaleY = sel->scaleY * 100.0f;
    float rotation = sel->rotation * 180.0f / 3.14159265358979f;
    float opacity = static_cast<float>(sel->alpha) * 100.0f / 255.0f;
    int level = sel->level;

    const bool sxChanged = inputFloatField("Scale X %", &scaleX, 80.0f * scale);
    const bool syChanged = inputFloatField("Scale Y %", &scaleY, 80.0f * scale);
    const bool rotChanged = inputFloatField("Rotation", &rotation, 80.0f * scale,
                                            "%.1f");
    const bool opChanged = inputFloatField("Opacity %", &opacity, 80.0f * scale);

    bool levelChanged = false;
    ImGui::SetNextItemWidth(120.0f * scale);
    {
        static const char* kLevels[] = {"Back", "Middle", "Front"};
        TextFieldColors colors;
        if (ImGui::BeginCombo("Level", kLevels[std::clamp(level, 0, 2)])) {
            for (int i = 0; i < 3; ++i) {
                if (ImGui::Selectable(kLevels[i], i == level)) {
                    level = i;
                    levelChanged = true;
                }
            }
            ImGui::EndCombo();
        }
    }

    if (sxChanged || syChanged || rotChanged || opChanged || levelChanged) {
        ed.undo.push(ed.doc);
        for (auto& s : ed.doc.scenery) {
            if (!s.selected) {
                continue;
            }
            if (sxChanged) {
                s.scaleX = scaleX / 100.0f;
            }
            if (syChanged) {
                s.scaleY = scaleY / 100.0f;
            }
            if (rotChanged) {
                s.rotation = rotation * 3.14159265358979f / 180.0f;
            }
            if (opChanged) {
                s.alpha = static_cast<uint8_t>(
                    std::clamp(opacity, 0.0f, 100.0f) * 255.0f / 100.0f + 0.5f);
            }
            if (levelChanged) {
                s.level = level;
            }
        }
        ed.doc.markModified();
        ed.doc.rebuildScreenCache();
        ed.needsRedraw = true;
    }
}

void drawTransformProperties(Editor& ed, float scale) {
    /* picProp(2): txtScale(0/1) and txtRotate apply a one-shot transform to
       the selection about the reference point (frmInfo.frm:300). */
    if (!ed.doc.anySelected()) {
        ImGui::TextUnformatted("Nothing selected");
        return;
    }
    static float scaleX = 100.0f;
    static float scaleY = 100.0f;
    static float rotate = 0.0f;
    inputFloatField("Scale X %", &scaleX, 80.0f * scale);
    inputFloatField("Scale Y %", &scaleY, 80.0f * scale);
    inputFloatField("Rotation", &rotate, 80.0f * scale, "%.1f");
    if (ImGui::Button("Apply")) {
        ed.undo.push(ed.doc);
        MapDocument::TransformSession session;
        ed.doc.beginTransform(session);
        ed.doc.applyTransform(session, scaleX / 100.0f, scaleY / 100.0f,
                              rotate * 3.14159265358979f / 180.0f);
        ed.doc.markModified();
        ed.doc.rebuildScreenCache();
        ed.needsRedraw = true;
        scaleX = 100.0f;
        scaleY = 100.0f;
        rotate = 0.0f;
    }
}

void drawTextureProperties(Editor& ed, float scale) {
    /* picProp(3): txtQuadX/txtQuadY, the UV rectangle a Textured Quad is
       created with when User Defined X/Y are on (frmInfo.frm:220). */
    int texW = 0, texH = 0;
    ed.selectedTextureSize(texW, texH);
    ImGui::Text("Texture: %s", ed.doc.options.textureName.c_str());
    ImGui::Text("Dimensions: %dx%d", texW, texH);
    ImGui::Separator();
    ImGui::Checkbox("User Defined X", &ed.customTexX);
    ImGui::Checkbox("User Defined Y", &ed.customTexY);
    TextureWindowState& tw = ed.textureWindow;
    float x1 = tw.u1 * (texW > 0 ? texW : 1);
    float y1 = tw.v1 * (texH > 0 ? texH : 1);
    float x2 = tw.u2 * (texW > 0 ? texW : 1);
    float y2 = tw.v2 * (texH > 0 ? texH : 1);
    bool changed = inputFloatField("X1", &x1, 70.0f * scale);
    changed |= inputFloatField("Y1", &y1, 70.0f * scale);
    changed |= inputFloatField("X2", &x2, 70.0f * scale);
    changed |= inputFloatField("Y2", &y2, 70.0f * scale);
    if (changed && texW > 0 && texH > 0) {
        tw.u1 = x1 / texW;
        tw.v1 = y1 / texH;
        tw.u2 = x2 / texW;
        tw.v2 = y2 / texH;
        tw.hasSelection = true;
    }
}

void drawLightProperties(Editor& ed, float scale) {
    EditorLight* sel = nullptr;
    int count = 0;
    for (auto& l : ed.doc.lights) {
        if (l.selected) {
            ++count;
            if (sel == nullptr) {
                sel = &l;
            }
        }
    }
    if (sel == nullptr) {
        ImGui::TextUnformatted("No light selected");
        return;
    }
    ImGui::Text("%d selected", count);

    float intensity = sel->intensity * 100.0f;
    float z = sel->z;
    int range = sel->range;
    const bool iChanged = inputFloatField("Intensity %", &intensity, 80.0f * scale);
    const bool zChanged = inputFloatField("Z-coord", &z, 80.0f * scale, "%.1f");
    const bool rChanged = inputIntField("Range", &range, 80.0f * scale);
    if (iChanged || zChanged || rChanged) {
        ed.undo.push(ed.doc);
        for (auto& l : ed.doc.lights) {
            if (!l.selected) {
                continue;
            }
            if (iChanged) {
                l.intensity = intensity / 100.0f;
            }
            if (zChanged) {
                l.z = z;
            }
            if (rChanged) {
                l.range = range;
            }
        }
        ed.doc.markModified();
        ed.needsRedraw = true;
    }
}

void drawMapInfo(Editor& ed) {
    /* lblCount(0..6): the counts the original shows against the engine's own
       limits, which is the point of the page -- a map over a limit will not
       load in Soldat. */
    int vertexCount = static_cast<int>(ed.doc.polys.size()) * 3;
    int connections = 0;
    for (const auto& w : ed.doc.waypoints) {
        connections += static_cast<int>(w.connections.size());
    }
    ImGui::Text("Polygons:    %d/5000", static_cast<int>(ed.doc.polys.size()));
    ImGui::Text("Scenery:     %d/500 (%d)", static_cast<int>(ed.doc.scenery.size()),
                static_cast<int>(ed.doc.sceneryNames.size()) - 1);
    ImGui::Text("Spawns:      %d/128", static_cast<int>(ed.doc.spawns.size()));
    ImGui::Text("Colliders:   %d/128", static_cast<int>(ed.doc.colliders.size()));
    ImGui::Text("Waypoints:   %d/500", static_cast<int>(ed.doc.waypoints.size()));
    ImGui::Text("Connections: %d", connections);
    ImGui::Text("Vertices:    %d", vertexCount);
    ImGui::Separator();
    /* lblCount(6): the bounding box of the geometry, which is what the
       original reports as the map's dimensions (frmInfo.frm:470). */
    float minX = 0.0f, minY = 0.0f, maxX = 0.0f, maxY = 0.0f;
    bool first = true;
    for (const auto& p : ed.doc.polys) {
        for (const auto& v : p.v) {
            if (first) {
                minX = maxX = v.world.x;
                minY = maxY = v.world.y;
                first = false;
            } else {
                minX = std::min(minX, v.world.x);
                maxX = std::max(maxX, v.world.x);
                minY = std::min(minY, v.world.y);
                maxY = std::max(maxY, v.world.y);
            }
        }
    }
    ImGui::Text("Dimensions:  %.0fx%.0f", maxX - minX, maxY - minY);
}

void drawPropertiesPanel(App& app) {
    Editor& ed = app.editor();
    if (!ed.panels.properties) {
        return;
    }
    static int page = PROP_POLYGON;
    const float scale = app.uiScale();
    if (beginPanel("Properties", &ed.panels.properties, 8.0f, 330.0f, 260.0f,
                   300.0f, scale, ImGuiWindowFlags_None)) {
        static const char* kPages[PROP_COUNT] = {
            "Polygon Properties", "Scenery Properties", "Transform",
            "Texture Settings",   "Light Properties",   "Map Info",
        };
        if (ImGui::Button(kPages[page])) {
            ImGui::OpenPopup("prop_menu");
        }
        if (ImGui::BeginPopup("prop_menu")) {
            for (int i = 0; i < PROP_COUNT; ++i) {
                if (ImGui::MenuItem(kPages[i], nullptr, page == i)) {
                    page = i;
                }
            }
            ImGui::EndPopup();
        }
        ImGui::Separator();

        switch (page) {
        case PROP_POLYGON:   drawPolygonProperties(ed, scale); break;
        case PROP_SCENERY:   drawSceneryProperties(ed, scale); break;
        case PROP_TRANSFORM: drawTransformProperties(ed, scale); break;
        case PROP_TEXTURE:   drawTextureProperties(ed, scale); break;
        case PROP_LIGHT:     drawLightProperties(ed, scale); break;
        case PROP_MAPINFO:   drawMapInfo(ed); break;
        default: break;
        }

        ImGui::Separator();
        ImGui::Text("Cursor: %.0f, %.0f", ed.lastMouseWorld.x,
                    ed.lastMouseWorld.y);
    }
    ImGui::End();
}

/* ---- Palette (frmPalette) ----------------------------------------------- */

void drawPalettePanel(App& app) {
    Editor& ed = app.editor();
    if (!ed.panels.palette) {
        return;
    }
    PaletteState& pal = ed.palette;
    const float scale = app.uiScale();
    if (beginPanel("Palette", &ed.panels.palette, -270.0f, 40.0f, 0.0f, 0.0f,
                   scale)) {
        /* picPaletteMenu opens mnuPalette (frmPalette.frm:1150). */
        if (ImGui::Button("Palette")) {
            ImGui::OpenPopup("palette_menu");
        }
        if (ImGui::BeginPopup("palette_menu")) {
            if (ImGui::MenuItem("Load Palette")) {
                pal.load(PaletteState::currentPalettePath());
            }
            if (ImGui::MenuItem("Save Palette")) {
                pal.save(PaletteState::userPalettePath());
            }
            if (ImGui::MenuItem("Clear")) {
                pal.cells.fill(PaletteColor{});
                pal.selCol = -1;
                pal.selRow = -1;
            }
            ImGui::EndPopup();
        }

        /* picPalette: a 12x6 grid of swatches.  Left click selects a colour,
           right click stores the current one (frmPalette.frm:820 / mnuAddToPalette). */
        const float cell = 14.0f * scale;
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(1.0f, 1.0f));
        for (int row = 0; row < PaletteState::kRows; ++row) {
            for (int col = 0; col < PaletteState::kCols; ++col) {
                if (col != 0) {
                    ImGui::SameLine();
                }
                const int index = row * PaletteState::kCols + col;
                const PaletteColor& c = pal.cells[static_cast<size_t>(index)];
                ImGui::PushID(index);
                const ImVec4 colour(c.r / 255.0f, c.g / 255.0f, c.b / 255.0f,
                                    1.0f);
                if (ImGui::ColorButton("##swatch", colour,
                                       ImGuiColorEditFlags_NoTooltip |
                                           ImGuiColorEditFlags_NoDragDrop,
                                       ImVec2(cell, cell))) {
                    pal.r = c.r;
                    pal.g = c.g;
                    pal.b = c.b;
                    pal.selCol = col;
                    pal.selRow = row;
                }
                if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                    pal.cells[static_cast<size_t>(index)] = {pal.r, pal.g, pal.b};
                    pal.selCol = col;
                    pal.selRow = row;
                }
                /* shpSel1/shpSel2: the selected swatch is ringed. */
                if (pal.selCol == col && pal.selRow == row) {
                    const ImVec2 a = ImGui::GetItemRectMin();
                    const ImVec2 b = ImGui::GetItemRectMax();
                    ImGui::GetWindowDrawList()->AddRect(
                        a, b, IM_COL32(255, 255, 255, 255));
                    ImGui::GetWindowDrawList()->AddRect(
                        ImVec2(a.x + 1, a.y + 1), ImVec2(b.x - 1, b.y - 1),
                        IM_COL32(0, 0, 0, 255));
                }
                ImGui::PopID();
            }
        }
        ImGui::PopStyleVar();

        ImGui::Separator();
        /* picColor: the current vertex colour, and the way into the picker. */
        float rgb[3] = {pal.r / 255.0f, pal.g / 255.0f, pal.b / 255.0f};
        if (ImGui::ColorEdit3("Vertex Color", rgb,
                              ImGuiColorEditFlags_NoInputs |
                                  ImGuiColorEditFlags_PickerHueBar)) {
            ed.setPaintColorFromPicker(
                static_cast<uint8_t>(rgb[0] * 255.0f + 0.5f),
                static_cast<uint8_t>(rgb[1] * 255.0f + 0.5f),
                static_cast<uint8_t>(rgb[2] * 255.0f + 0.5f));
        }

        /* txtRGB(0..2): the same colour as numbers, which is how a precise
           value gets entered. */
        int r = pal.r, g = pal.g, b = pal.b;
        bool numeric = false;
        ImGui::SetNextItemWidth(50.0f * scale);
        {
            TextFieldColors colors;
            numeric |= ImGui::InputInt("R", &r, 0, 0);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(50.0f * scale);
            numeric |= ImGui::InputInt("G", &g, 0, 0);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(50.0f * scale);
            numeric |= ImGui::InputInt("B", &b, 0, 0);
        }
        if (numeric) {
            ed.setPaintColorFromPicker(
                static_cast<uint8_t>(std::clamp(r, 0, 255)),
                static_cast<uint8_t>(std::clamp(g, 0, 255)),
                static_cast<uint8_t>(std::clamp(b, 0, 255)));
        }

        float opacity = pal.opacity * 100.0f;
        if (inputFloatField("Opacity %", &opacity, 60.0f * scale)) {
            pal.opacity = std::clamp(opacity, 0.0f, 100.0f) / 100.0f;
        }

        /* cboBlendMode: the six names stored in frmPalette.frx. */
        static const char* kBlendModes[] = {"Normal",  "Multiply", "Screen",
                                            "Darken",  "Lighten",  "Difference"};
        ImGui::SetNextItemWidth(110.0f * scale);
        {
            TextFieldColors colors;
            if (ImGui::BeginCombo("Blend",
                                  kBlendModes[std::clamp(pal.blendMode, 0, 5)])) {
                for (int i = 0; i < 6; ++i) {
                    if (ImGui::Selectable(kBlendModes[i], pal.blendMode == i)) {
                        pal.blendMode = i;
                    }
                }
                ImGui::EndCombo();
            }
        }

        /* picColorMode(0..2): how the painting tools apply the colour. */
        ImGui::TextUnformatted("Mode:");
        if (ImGui::RadioButton("Precision", pal.colorMode == 0)) {
            pal.colorMode = 0;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Normal", pal.colorMode == 1)) {
            pal.colorMode = 1;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Dynamic", pal.colorMode == 2)) {
            pal.colorMode = 2;
        }

        int radius = pal.radius;
        if (inputIntField("Radius", &radius, 60.0f * scale)) {
            pal.radius = std::max(1, radius);
        }

        /* cmdDefault: back to white at full opacity (frmPalette.frm:960). */
        if (ImGui::Button("Default")) {
            ed.setPaintColorFromPicker(255, 255, 255);
            pal.opacity = 1.0f;
            pal.blendMode = 0;
        }
    }
    ImGui::End();
}

/* ---- Texture (frmTexture) ----------------------------------------------- */

void drawTexturePanel(App& app) {
    Editor& ed = app.editor();
    if (!ed.panels.texture) {
        return;
    }
    TextureWindowState& tw = ed.textureWindow;
    const float scale = app.uiScale();
    if (beginPanel("Texture", &ed.panels.texture, 300.0f, 100.0f, 340.0f,
                   340.0f, scale, ImGuiWindowFlags_HorizontalScrollbar)) {
        int w = 0, h = 0;
        const unsigned int tex =
            app.previewTexture(ed.doc.options.textureName, w, h);
        if (tex == 0 || w <= 0 || h <= 0) {
            ImGui::TextWrapped("No texture loaded (%s).",
                               ed.doc.options.textureName.empty()
                                   ? "none set"
                                   : ed.doc.options.textureName.c_str());
        } else {
            tw.width = w;
            tw.height = h;
            const float drawW = static_cast<float>(w) * tw.zoom * scale;
            const float drawH = static_cast<float>(h) * tw.zoom * scale;
            const ImVec2 origin = ImGui::GetCursorScreenPos();
            ImGui::Image(static_cast<ImTextureID>(tex), ImVec2(drawW, drawH));

            /* Dragging over the image marks the rectangle "User Defined X/Y"
               takes a new Textured Quad's UVs from (frmTexture.frm:190). */
            if (ImGui::IsItemActive() &&
                ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                const ImVec2 start = ImGui::GetIO().MouseClickedPos[0];
                const ImVec2 now = ImGui::GetIO().MousePos;
                tw.u1 = std::clamp((start.x - origin.x) / drawW, 0.0f, 1.0f);
                tw.v1 = std::clamp((start.y - origin.y) / drawH, 0.0f, 1.0f);
                tw.u2 = std::clamp((now.x - origin.x) / drawW, 0.0f, 1.0f);
                tw.v2 = std::clamp((now.y - origin.y) / drawH, 0.0f, 1.0f);
                tw.hasSelection = true;
            }
            if (tw.hasSelection) {
                ImGui::GetWindowDrawList()->AddRect(
                    ImVec2(origin.x + tw.u1 * drawW, origin.y + tw.v1 * drawH),
                    ImVec2(origin.x + tw.u2 * drawW, origin.y + tw.v2 * drawH),
                    IM_COL32(255, 255, 0, 255));
            }
        }

        ImGui::SetNextItemWidth(120.0f * scale);
        ImGui::SliderFloat("Zoom", &tw.zoom, 0.25f, 4.0f, "%.2fx");
        if (tw.hasSelection && w > 0 && h > 0) {
            ImGui::Text("Selection: %.0f,%.0f - %.0f,%.0f", tw.u1 * w,
                        tw.v1 * h, tw.u2 * w, tw.v2 * h);
        }
    }
    ImGui::End();
}

}  // namespace

void drawPanels(App& app) {
    drawToolsPanel(app);
    drawDisplayPanel(app);
    drawSceneryPanel(app);
    drawWaypointsPanel(app);
    drawPropertiesPanel(app);
    drawPalettePanel(app);
    drawTexturePanel(app);
}

}  // namespace pw
