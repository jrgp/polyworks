/*
 * dialogs.cpp — modal dialogs, message boxes and the file browser.
 *
 * Two things here are different in kind from the wxWidgets implementation and
 * both follow from Dear ImGui having no nested event loop:
 *
 *  - A dialog cannot block.  wxFileDialog::ShowModal() returns an answer;
 *    these draw themselves every frame and report a result when the user
 *    provides one.  Anything that needed to happen "after the dialog" is a
 *    continuation stored in the Editor (see confirmDiscardChanges).
 *
 *  - There is no native file dialog.  GLFW deliberately does not provide one,
 *    and pulling in GTK or the Windows common dialog would reintroduce exactly
 *    the platform toolkit this port removed.  The browser below is therefore
 *    part of the application, which also means it behaves identically on all
 *    three platforms and can be driven in a test.
 *
 * The Map Settings and Preferences dialogs reproduce frmMap and
 * frmPreferences control for control.
 */

#include "app.h"
#include "editor.h"
#include "ini_file.h"
#include "platform.h"
#include "prefs.h"

#include "imgui.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace pw {
namespace {

namespace fs = std::filesystem;

/* ---- file browser ------------------------------------------------------- */

struct FileBrowser {
    bool        open = false;
    bool        saving = false;
    std::string title;
    std::string extension;     /* ".pms", ".pw", ... */
    fs::path    directory;
    std::string filename;
    std::vector<fs::directory_entry> entries;
    bool        needsRescan = true;
    PendingPopup purpose = PendingPopup::None;
};

FileBrowser g_browser;

void rescan(FileBrowser& b) {
    b.entries.clear();
    std::error_code ec;
    if (!fs::is_directory(b.directory, ec)) {
        b.directory = fs::current_path(ec);
    }
    for (const auto& entry : fs::directory_iterator(b.directory, ec)) {
        if (entry.is_directory(ec)) {
            b.entries.push_back(entry);
        } else if (b.extension.empty()) {
            b.entries.push_back(entry);
        } else {
            std::string ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            if (ext == b.extension) {
                b.entries.push_back(entry);
            }
        }
    }
    /* Directories first, then names, case-insensitively -- the order a file
       dialog is expected to use. */
    std::sort(b.entries.begin(), b.entries.end(),
              [](const fs::directory_entry& a, const fs::directory_entry& c) {
                  std::error_code e1, e2;
                  const bool ad = a.is_directory(e1);
                  const bool cd = c.is_directory(e2);
                  if (ad != cd) {
                      return ad;
                  }
                  std::string an = a.path().filename().string();
                  std::string cn = c.path().filename().string();
                  std::transform(an.begin(), an.end(), an.begin(),
                                 [](unsigned char ch) { return std::tolower(ch); });
                  std::transform(cn.begin(), cn.end(), cn.begin(),
                                 [](unsigned char ch) { return std::tolower(ch); });
                  return an < cn;
              });
    b.needsRescan = false;
}

void openBrowser(FileBrowser& b, PendingPopup purpose, const char* title,
                 const char* extension, bool saving, const fs::path& startDir,
                 const std::string& startName) {
    b.open = true;
    b.saving = saving;
    b.title = title;
    b.extension = extension;
    b.directory = startDir;
    b.filename = startName;
    b.purpose = purpose;
    b.needsRescan = true;
}

/* Returns the chosen path once, then resets. */
bool drawBrowser(App& app, FileBrowser& b, std::string& chosen,
                 PendingPopup& purpose) {
    if (!b.open) {
        return false;
    }
    if (b.needsRescan) {
        rescan(b);
    }

    ImGui::OpenPopup(b.title.c_str());
    const ImVec2 centre = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(centre, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(560.0f * app.uiScale(),
                                    420.0f * app.uiScale()),
                             ImGuiCond_Appearing);

    bool result = false;
    if (ImGui::BeginPopupModal(b.title.c_str(), nullptr,
                               ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::TextUnformatted(b.directory.string().c_str());
        if (ImGui::Button("Up")) {
            if (b.directory.has_parent_path() &&
                b.directory.parent_path() != b.directory) {
                b.directory = b.directory.parent_path();
                b.needsRescan = true;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Home")) {
            b.directory = userConfigDir().empty()
                              ? fs::current_path()
                              : fs::path(userConfigDir()).parent_path();
            b.needsRescan = true;
        }

        const float listH = ImGui::GetContentRegionAvail().y -
                            70.0f * app.uiScale();
        if (ImGui::BeginChild("##files", ImVec2(0.0f, listH), true)) {
            std::error_code ec;
            for (const fs::directory_entry& entry : b.entries) {
                const bool dir = entry.is_directory(ec);
                std::string label = entry.path().filename().string();
                if (dir) {
                    label = "[" + label + "]";
                }
                const bool selected =
                    !dir && label == b.filename;
                if (ImGui::Selectable(label.c_str(), selected,
                                      ImGuiSelectableFlags_AllowDoubleClick)) {
                    if (dir) {
                        b.directory = entry.path();
                        b.needsRescan = true;
                    } else {
                        b.filename = entry.path().filename().string();
                        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                            chosen = (b.directory / b.filename).string();
                            purpose = b.purpose;
                            b.open = false;
                            result = true;
                        }
                    }
                }
                if (b.needsRescan) {
                    break;   /* the listing under the iterator just changed */
                }
            }
        }
        ImGui::EndChild();

        char buffer[512];
        std::snprintf(buffer, sizeof(buffer), "%s", b.filename.c_str());
        ImGui::SetNextItemWidth(-120.0f * app.uiScale());
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1));
        if (ImGui::InputText("File", buffer, sizeof(buffer))) {
            b.filename = buffer;
        }
        ImGui::PopStyleColor();

        const bool valid = !b.filename.empty();
        if (!valid) {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button(b.saving ? "Save" : "Open",
                          ImVec2(100.0f * app.uiScale(), 0.0f))) {
            fs::path path = b.directory / b.filename;
            /* A save with no extension gets the dialog's own, so that
               "mymap" produces mymap.pms as the original does. */
            if (b.saving && !b.extension.empty() &&
                path.extension().empty()) {
                path += b.extension;
            }
            chosen = path.string();
            purpose = b.purpose;
            b.open = false;
            result = true;
        }
        if (!valid) {
            ImGui::EndDisabled();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100.0f * app.uiScale(), 0.0f))) {
            b.open = false;
            ImGui::CloseCurrentPopup();
        }
        if (result) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    return result;
}

/* Where a browser should start.  The original opens on the configured
   directory for the kind of file being chosen (modConfig.bas MapsDir /
   PrefabsDir / UncompDir), falling back to the current map's folder. */
fs::path startDirFor(Editor& ed, PendingPopup purpose) {
    std::error_code ec;
    auto valid = [&ec](const std::string& p) {
        return !p.empty() && fs::is_directory(p, ec);
    };
    switch (purpose) {
    case PendingPopup::ImportPrefab:
    case PendingPopup::ExportPrefab:
        if (valid(ed.prefs.prefabsDir)) {
            return ed.prefs.prefabsDir;
        }
        break;
    case PendingPopup::OpenMap:
    case PendingPopup::SaveMapAs:
        if (valid(ed.prefs.uncompDir)) {
            return ed.prefs.uncompDir;
        }
        break;
    case PendingPopup::OpenCompiled:
    case PendingPopup::CompileAs:
        if (valid(ed.prefs.soldatDir)) {
            const fs::path maps = fs::path(ed.prefs.soldatDir) / "Maps";
            if (fs::is_directory(maps, ec)) {
                return maps;
            }
            return ed.prefs.soldatDir;
        }
        break;
    default:
        break;
    }
    if (!ed.currentFilePath.empty()) {
        const fs::path parent = fs::path(ed.currentFilePath).parent_path();
        if (fs::is_directory(parent, ec)) {
            return parent;
        }
    }
    return fs::current_path(ec);
}

/* ---- message box -------------------------------------------------------- */

void drawMessageBox(App& app) {
    Editor& ed = app.editor();
    MessageBox& mb = ed.messageBox;
    if (!mb.visible()) {
        return;
    }

    const char* title = mb.title.empty() ? "PolyWorks" : mb.title.c_str();
    ImGui::OpenPopup(title);
    const ImVec2 centre = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(centre, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal(title, nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize |
                                   ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::TextUnformatted(mb.text.c_str());
        ImGui::Separator();

        auto answer = [&](int value) {
            mb.respond(value);
            ImGui::CloseCurrentPopup();
        };

        const float width = 90.0f * app.uiScale();
        switch (mb.kind) {
        case MessageBox::Kind::Confirm:
            if (ImGui::Button("Yes", ImVec2(width, 0.0f))) {
                answer(1);
            }
            ImGui::SameLine();
            if (ImGui::Button("No", ImVec2(width, 0.0f))) {
                answer(0);
            }
            break;
        case MessageBox::Kind::ConfirmCancel:
            /* The save prompt the original raises before discarding a
               modified map (frm:1010): Yes saves, No discards, Cancel
               abandons whatever was about to happen. */
            if (ImGui::Button("Yes", ImVec2(width, 0.0f))) {
                answer(1);
            }
            ImGui::SameLine();
            if (ImGui::Button("No", ImVec2(width, 0.0f))) {
                answer(0);
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(width, 0.0f))) {
                answer(2);
            }
            break;
        default:
            if (ImGui::Button("OK", ImVec2(width, 0.0f))) {
                answer(1);
            }
            break;
        }
        ImGui::EndPopup();
    }
}

/* Act on an answered message box.  Kept out of Editor so that the decision
   about *which* continuation runs stays next to the buttons that produced it. */
void processMessageAnswer(App& app) {
    Editor& ed = app.editor();
    MessageBox& mb = ed.messageBox;
    int answer = -1;
    if (!mb.takeAnswer(answer)) {
        return;   /* no button pressed since the last frame */
    }

    auto continuation = ed.pendingAfterPrompt;
    ed.pendingAfterPrompt = nullptr;

    switch (answer) {
    case 1:
        /* Save, then continue.  With no filename yet the Save As browser has
           to come first, so the continuation is parked again behind it. */
        if (ed.currentFilePath.empty()) {
            ed.pendingAfterPrompt = continuation;
            app.openPopup(PendingPopup::SaveMapAs);
        } else {
            ed.saveCurrent();
            if (continuation) {
                continuation();
            }
        }
        break;
    case 0:
        if (continuation) {
            continuation();
        }
        break;
    default:
        break;   /* Cancel: the pending action is dropped */
    }
}

/* ---- Map Settings (frmMap) ---------------------------------------------- */

void drawMapSettings(App& app, bool justOpened) {
    Editor& ed = app.editor();
    static char description[64];
    static char textureName[64];
    static int weather = 0, steps = 0, jetCount = 0, grenades = 0, medikits = 0;
    static float backColor1[3] = {0, 0, 0};
    static float backColor2[3] = {0, 0, 0};
    static std::vector<std::string> textures;

    if (justOpened) {
        std::snprintf(description, sizeof(description), "%s",
                      ed.doc.options.mapName.c_str());
        std::snprintf(textureName, sizeof(textureName), "%s",
                      ed.doc.options.textureName.c_str());
        weather = ed.doc.options.weather;
        steps = ed.doc.options.steps;
        jetCount = ed.doc.options.startJet;
        grenades = ed.doc.options.grenadePacks;
        medikits = ed.doc.options.medikits;
        const uint32_t c1 = ed.doc.options.bgColor1;
        const uint32_t c2 = ed.doc.options.bgColor2;
        backColor1[0] = ((c1 >> 16) & 0xFF) / 255.0f;
        backColor1[1] = ((c1 >> 8) & 0xFF) / 255.0f;
        backColor1[2] = (c1 & 0xFF) / 255.0f;
        backColor2[0] = ((c2 >> 16) & 0xFF) / 255.0f;
        backColor2[1] = ((c2 >> 8) & 0xFF) / 255.0f;
        backColor2[2] = (c2 & 0xFF) / 255.0f;

        /* cboTexture lists the game's textures directory (frmMap.frm:430). */
        textures.clear();
        std::error_code ec;
        if (!ed.prefs.soldatDir.empty()) {
            for (const char* sub : {"Textures", "textures"}) {
                const fs::path dir = fs::path(ed.prefs.soldatDir) / sub;
                if (!fs::is_directory(dir, ec)) {
                    continue;
                }
                for (const auto& e : fs::directory_iterator(dir, ec)) {
                    if (e.is_regular_file(ec)) {
                        textures.push_back(e.path().filename().string());
                    }
                }
                break;
            }
            std::sort(textures.begin(), textures.end());
        }
    }

    ImGui::OpenPopup("Map Settings");
    const ImVec2 centre = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(centre, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (!ImGui::BeginPopupModal("Map Settings", nullptr,
                                ImGuiWindowFlags_AlwaysAutoResize |
                                    ImGuiWindowFlags_NoSavedSettings)) {
        return;
    }

    const float scale = app.uiScale();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1));
    ImGui::SetNextItemWidth(260.0f * scale);
    ImGui::InputText("Description", description, sizeof(description));
    ImGui::SetNextItemWidth(260.0f * scale);
    ImGui::InputText("Texture", textureName, sizeof(textureName));
    ImGui::PopStyleColor();

    if (!textures.empty()) {
        ImGui::SetNextItemWidth(260.0f * scale);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1));
        if (ImGui::BeginCombo("##texturelist", "Choose...")) {
            for (const std::string& name : textures) {
                if (ImGui::Selectable(name.c_str())) {
                    std::snprintf(textureName, sizeof(textureName), "%s",
                                  name.c_str());
                }
            }
            ImGui::EndCombo();
        }
        ImGui::PopStyleColor();
    }

    /* picTexture (frmMap.frm:64, assigned in cboTexture_Click at :790): the
       chosen texture is shown straight away, so a wrong pick is obvious
       before the dialog is accepted. */
    {
        int tw = 0, th = 0;
        const unsigned int tex = app.previewTexture(textureName, tw, th);
        if (tex != 0 && tw > 0 && th > 0) {
            const float box = 96.0f * scale;
            const float fit = std::min(box / tw, box / th);
            ImGui::Image(static_cast<ImTextureID>(tex),
                         ImVec2(tw * fit, th * fit));
        } else if (textureName[0] != '\0') {
            ImGui::TextUnformatted("(texture not found)");
        }
    }

    ImGui::ColorEdit3("Background top", backColor1,
                      ImGuiColorEditFlags_NoInputs);
    ImGui::ColorEdit3("Background bottom", backColor2,
                      ImGuiColorEditFlags_NoInputs);

    /* cboWeather / cboSteps: the fixed lists frmMap.frx stores. */
    static const char* kWeather[] = {"None", "Rain", "Sandstorm", "Snow"};
    static const char* kSteps[] = {"Hard Ground", "Soft Ground", "None"};
    ImGui::SetNextItemWidth(160.0f * scale);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1));
    if (ImGui::BeginCombo("Weather", kWeather[std::clamp(weather, 0, 3)])) {
        for (int i = 0; i < 4; ++i) {
            if (ImGui::Selectable(kWeather[i], weather == i)) {
                weather = i;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SetNextItemWidth(160.0f * scale);
    if (ImGui::BeginCombo("Steps", kSteps[std::clamp(steps, 0, 2)])) {
        for (int i = 0; i < 3; ++i) {
            if (ImGui::Selectable(kSteps[i], steps == i)) {
                steps = i;
            }
        }
        ImGui::EndCombo();
    }
    /* cboJet (frmMap.frm:725): named presets write a fixed amount into the
       box, and only "Custom" leaves it editable. */
    struct JetPreset { const char* name; int value; };
    static const JetPreset kJets[] = {
        {"None", 0},     {"Minimal", 12}, {"Very low", 45},
        {"Low", 95},     {"Normal", 190}, {"High", 320},
        {"Maximum", 800},{"Infinite", 32766},
    };
    constexpr int kJetPresetCount = 8;
    int jetPreset = kJetPresetCount;   /* Custom */
    for (int i = 0; i < kJetPresetCount; ++i) {
        if (kJets[i].value == jetCount) {
            jetPreset = i;
            break;
        }
    }
    ImGui::SetNextItemWidth(160.0f * scale);
    if (ImGui::BeginCombo("Jets",
                          jetPreset == kJetPresetCount ? "Custom"
                                                       : kJets[jetPreset].name)) {
        for (int i = 0; i < kJetPresetCount; ++i) {
            if (ImGui::Selectable(kJets[i].name, jetPreset == i)) {
                jetCount = kJets[i].value;
                jetPreset = i;
            }
        }
        if (ImGui::Selectable("Custom", jetPreset == kJetPresetCount)) {
            jetPreset = kJetPresetCount;
        }
        ImGui::EndCombo();
    }
    ImGui::SetNextItemWidth(120.0f * scale);
    ImGui::BeginDisabled(jetPreset != kJetPresetCount);
    if (ImGui::InputInt("Jet Fuel", &jetCount)) {
        jetCount = std::max(0, jetCount);
    }
    ImGui::EndDisabled();
    ImGui::SetNextItemWidth(120.0f * scale);
    ImGui::InputInt("Grenades", &grenades);
    ImGui::SetNextItemWidth(120.0f * scale);
    ImGui::InputInt("Medikits", &medikits);
    ImGui::PopStyleColor();

    ImGui::Separator();
    if (ImGui::Button("OK", ImVec2(100.0f * scale, 0.0f))) {
        ed.undo.push(ed.doc);
        ed.doc.options.mapName = description;
        const bool textureChanged =
            ed.doc.options.textureName != std::string(textureName);
        ed.doc.options.textureName = textureName;
        ed.doc.options.weather = static_cast<uint8_t>(std::clamp(weather, 0, 3));
        ed.doc.options.steps = static_cast<uint8_t>(std::clamp(steps, 0, 2));
        ed.doc.options.startJet = jetCount;
        ed.doc.options.grenadePacks =
            static_cast<uint8_t>(std::clamp(grenades, 0, 255));
        ed.doc.options.medikits =
            static_cast<uint8_t>(std::clamp(medikits, 0, 255));
        auto pack = [](const float c[3]) {
            return 0xFF000000u |
                   (static_cast<uint32_t>(c[0] * 255.0f + 0.5f) << 16) |
                   (static_cast<uint32_t>(c[1] * 255.0f + 0.5f) << 8) |
                   static_cast<uint32_t>(c[2] * 255.0f + 0.5f);
        };
        ed.doc.options.bgColor1 = pack(backColor1);
        ed.doc.options.bgColor2 = pack(backColor2);
        ed.doc.markModified();
        if (textureChanged) {
            ed.refreshTextureWindow();
        }
        ed.needsRedraw = true;
        ImGui::CloseCurrentPopup();
        app.openPopup(PendingPopup::None);
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(100.0f * scale, 0.0f))) {
        ImGui::CloseCurrentPopup();
        app.openPopup(PendingPopup::None);
    }
    ImGui::EndPopup();
}

/* ---- Preferences (frmPreferences) --------------------------------------- */

void drawPreferences(App& app, bool justOpened) {
    Editor& ed = app.editor();
    static AppPrefs working;
    static char soldatDir[512];
    static char prefabsDir[512];
    static char uncompDir[512];

    if (justOpened) {
        working = ed.prefs;
        std::snprintf(soldatDir, sizeof(soldatDir), "%s",
                      working.soldatDir.c_str());
        std::snprintf(prefabsDir, sizeof(prefabsDir), "%s",
                      working.prefabsDir.c_str());
        std::snprintf(uncompDir, sizeof(uncompDir), "%s",
                      working.uncompDir.c_str());
    }

    ImGui::OpenPopup("Preferences");
    const ImVec2 centre = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(centre, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(520.0f * app.uiScale(), 0.0f),
                             ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal("Preferences", nullptr,
                                ImGuiWindowFlags_AlwaysAutoResize |
                                    ImGuiWindowFlags_NoSavedSettings)) {
        return;
    }

    const float scale = app.uiScale();
    const float field = 90.0f * scale;
    auto commit = [&]() {
        working.soldatDir = soldatDir;
        working.prefabsDir = prefabsDir;
        working.uncompDir = uncompDir;
        /* Same validation the ini loader applies, so the dialog cannot store
           a range the loader would only have to repair (modConfig.bas:101). */
        working.sanitiseZoom();
        ed.prefs = working;
        ed.applyPrefs();
        ed.savePrefs();
        ed.refreshSceneryList();
        ed.needsRedraw = true;
    };

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1));

    /* lblDirs "Directories".  The Soldat directory is the one setting the
       editor cannot usefully guess: without it the scenery list is empty and
       map textures resolve only when the map sits inside a game install. */
    ImGui::SeparatorText("Directories");
    ImGui::SetNextItemWidth(-10.0f * scale);
    ImGui::InputText("Soldat / OpenSoldat", soldatDir, sizeof(soldatDir));
    ImGui::SetNextItemWidth(-10.0f * scale);
    ImGui::InputText("Uncompiled maps", uncompDir, sizeof(uncompDir));
    ImGui::SetNextItemWidth(-10.0f * scale);
    ImGui::InputText("Prefabs", prefabsDir, sizeof(prefabsDir));
    ImGui::PopStyleColor();
    {
        std::error_code ec;
        const bool ok = soldatDir[0] != '\0' &&
                        fs::is_directory(soldatDir, ec);
        if (soldatDir[0] == '\0') {
            ImGui::TextDisabled(
                "Not set - scenery and textures will not be found.");
        } else if (!ok) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.4f, 1.0f),
                               "That directory does not exist.");
        } else {
            const bool scenery =
                fs::is_directory(fs::path(soldatDir) / "Scenery-gfx", ec);
            const bool tex = fs::is_directory(fs::path(soldatDir) / "Textures", ec);
            ImGui::TextDisabled("Scenery-gfx: %s   Textures: %s",
                                scenery ? "found" : "missing",
                                tex ? "found" : "missing");
        }
    }

    ImGui::SeparatorText("Zoom");
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1));
    float minZoom = working.minZoom * 100.0f;
    float maxZoom = working.maxZoom * 100.0f;
    float resetZoom = working.resetZoom * 100.0f;
    ImGui::SetNextItemWidth(field);
    if (ImGui::InputFloat("Min %", &minZoom, 0.0f, 0.0f, "%.0f")) {
        working.minZoom = std::max(0.01f, minZoom / 100.0f);
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(field);
    if (ImGui::InputFloat("Max %", &maxZoom, 0.0f, 0.0f, "%.0f")) {
        working.maxZoom = std::max(working.minZoom, maxZoom / 100.0f);
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(field);
    if (ImGui::InputFloat("Reset %", &resetZoom, 0.0f, 0.0f, "%.0f")) {
        working.resetZoom = std::max(0.01f, resetZoom / 100.0f);
    }

    ImGui::SeparatorText("Grid");
    ImGui::SetNextItemWidth(field);
    ImGui::InputInt("Spacing", &working.gridSpacing, 0, 0);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(field);
    ImGui::InputInt("Divisions", &working.gridDivisions, 0, 0);
    ImGui::PopStyleColor();

    auto colorEdit = [&](const char* label, unsigned int* argb) {
        float c[3] = {((*argb >> 16) & 0xFF) / 255.0f,
                      ((*argb >> 8) & 0xFF) / 255.0f, (*argb & 0xFF) / 255.0f};
        if (ImGui::ColorEdit3(label, c, ImGuiColorEditFlags_NoInputs)) {
            *argb = 0xFF000000u |
                    (static_cast<unsigned int>(c[0] * 255.0f + 0.5f) << 16) |
                    (static_cast<unsigned int>(c[1] * 255.0f + 0.5f) << 8) |
                    static_cast<unsigned int>(c[2] * 255.0f + 0.5f);
        }
    };
    colorEdit("Grid color 1", &working.gridColor1);
    ImGui::SameLine();
    colorEdit("Grid color 2", &working.gridColor2);

    ImGui::SeparatorText("Colors");
    colorEdit("Point", &working.pointColor);
    ImGui::SameLine();
    colorEdit("Selection", &working.selectionColor);
    ImGui::SameLine();
    colorEdit("Background", &working.backgroundColor);

    ImGui::SeparatorText("Blending");
    /* cboPolySrc / cboPolyDest / cboWireSrc / cboWireDest.  The list is the
       Direct3D blend factors the original exposes, in its order. */
    static const char* kFactors[] = {
        "Zero",         "One",          "SrcColor",     "InvSrcColor",
        "SrcAlpha",     "InvSrcAlpha",  "DestAlpha",    "InvDestAlpha",
        "DestColor",    "InvDestColor", "SrcAlphaSat",
    };
    constexpr int kFactorCount = static_cast<int>(sizeof(kFactors) /
                                                  sizeof(kFactors[0]));
    auto factorCombo = [&](const char* label, int* value) {
        ImGui::SetNextItemWidth(130.0f * scale);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1));
        if (ImGui::BeginCombo(label,
                              kFactors[std::clamp(*value, 0, kFactorCount - 1)])) {
            for (int i = 0; i < kFactorCount; ++i) {
                if (ImGui::Selectable(kFactors[i], *value == i)) {
                    *value = i;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::PopStyleColor();
    };
    factorCombo("Polygon SRC", &working.polyBlendSrc);
    ImGui::SameLine();
    factorCombo("Polygon DEST", &working.polyBlendDest);
    factorCombo("Wireframe SRC", &working.wireBlendSrc);
    ImGui::SameLine();
    factorCombo("Wireframe DEST", &working.wireBlendDest);

    ImGui::SeparatorText("Other");
    ImGui::Checkbox("Use 4 verts for scenery", &working.sceneryVerts);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1));
    ImGui::SetNextItemWidth(field);
    ImGui::InputInt("Undo depth", &working.undoDepth, 0, 0);
    ImGui::PopStyleColor();

    ImGui::Separator();
    if (ImGui::Button("OK", ImVec2(100.0f * scale, 0.0f))) {
        commit();
        ImGui::CloseCurrentPopup();
        app.openPopup(PendingPopup::None);
    }
    ImGui::SameLine();
    if (ImGui::Button("Apply", ImVec2(100.0f * scale, 0.0f))) {
        commit();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(100.0f * scale, 0.0f))) {
        ImGui::CloseCurrentPopup();
        app.openPopup(PendingPopup::None);
    }
    ImGui::EndPopup();
}

}  // namespace

void drawDialogs(App& app) {
    Editor& ed = app.editor();

    /* Which modal, if any, is showing.  Held here rather than in App because
       only this file knows when one closes. */
    static PendingPopup active = PendingPopup::None;
    bool justOpened = false;

    const PendingPopup requested = app.takePendingPopup();
    if (requested != PendingPopup::None) {
        switch (requested) {
        case PendingPopup::OpenMap:
            openBrowser(g_browser, requested, "Open Map", ".pms", false,
                        startDirFor(ed, requested), {});
            break;
        case PendingPopup::OpenCompiled:
            openBrowser(g_browser, requested, "Open Compiled Map", ".pms",
                        false, startDirFor(ed, requested), {});
            break;
        case PendingPopup::SaveMapAs:
            openBrowser(g_browser, requested, "Save Map As", ".pms", true,
                        startDirFor(ed, requested),
                        fs::path(ed.currentFilePath).filename().string());
            break;
        case PendingPopup::CompileAs:
            openBrowser(g_browser, requested, "Compile To", ".pms", true,
                        startDirFor(ed, requested),
                        fs::path(ed.currentFilePath).filename().string());
            break;
        case PendingPopup::ExportPrefab:
            openBrowser(g_browser, requested, "Export Prefab", ".pwp", true,
                        startDirFor(ed, requested), {});
            break;
        case PendingPopup::ImportPrefab:
            openBrowser(g_browser, requested, "Import Prefab", ".pwp", false,
                        startDirFor(ed, requested), {});
            break;
        case PendingPopup::LoadWorkspace:
            openBrowser(g_browser, requested, "Load Workspace", ".ini", false,
                        startDirFor(ed, requested), {});
            break;
        case PendingPopup::SaveWorkspace:
            openBrowser(g_browser, requested, "Save Workspace", ".ini", true,
                        startDirFor(ed, requested), {});
            break;
        default:
            active = requested;
            justOpened = true;
            break;
        }
    }

    std::string chosen;
    PendingPopup purpose = PendingPopup::None;
    if (drawBrowser(app, g_browser, chosen, purpose)) {
        switch (purpose) {
        case PendingPopup::OpenMap:
        case PendingPopup::OpenCompiled:
            ed.loadMap(chosen);
            break;
        case PendingPopup::SaveMapAs: {
            /* A Save As that was raised to answer a discard prompt has to run
               the parked continuation once the file is written. */
            auto continuation = ed.pendingAfterPrompt;
            ed.pendingAfterPrompt = nullptr;
            if (ed.saveMap(chosen) && continuation) {
                continuation();
            }
            break;
        }
        case PendingPopup::CompileAs:
            ed.compileTo(chosen);
            break;
        case PendingPopup::ExportPrefab:
            ed.exportPrefab(chosen);
            break;
        case PendingPopup::ImportPrefab:
            ed.importPrefab(chosen);
            break;
        case PendingPopup::SaveWorkspace: {
            /* The workspace is the panel visibility plus ImGui's own window
               geometry, which is exactly what mnuSaveSpace stores. */
            IniFile ini;
            ini.load(chosen);
            ini.write("Windows", "Tools", ed.panels.tools);
            ini.write("Windows", "Display", ed.panels.display);
            ini.write("Windows", "Palette", ed.panels.palette);
            ini.write("Windows", "Waypoints", ed.panels.waypoints);
            ini.write("Windows", "Scenery", ed.panels.scenery);
            ini.write("Windows", "Properties", ed.panels.properties);
            ini.write("Windows", "Texture", ed.panels.texture);
            /* ImGui owns the window geometry, so the workspace stores its
               settings block verbatim.  Newlines would break the flat ini,
               so they are escaped. */
            std::string layout = ImGui::SaveIniSettingsToMemory();
            std::string encoded;
            for (char c : layout) {
                if (c == '\n') {
                    encoded += "\\n";
                } else if (c == '\\') {
                    encoded += "\\\\";
                } else if (c != '\r') {
                    encoded += c;
                }
            }
            ini.write("Windows", "Layout", encoded);
            ini.save();
            break;
        }
        case PendingPopup::LoadWorkspace: {
            IniFile ini;
            ini.load(chosen);
            {
                ed.panels.tools = ini.readBool("Windows", "Tools", true);
                ed.panels.display = ini.readBool("Windows", "Display", true);
                ed.panels.palette = ini.readBool("Windows", "Palette", true);
                ed.panels.waypoints = ini.readBool("Windows", "Waypoints", true);
                ed.panels.scenery = ini.readBool("Windows", "Scenery", true);
                ed.panels.properties =
                    ini.readBool("Windows", "Properties", true);
                ed.panels.texture = ini.readBool("Windows", "Texture", false);
                const std::string encoded =
                    ini.readString("Windows", "Layout");
                std::string layout;
                for (size_t i = 0; i < encoded.size(); ++i) {
                    if (encoded[i] == '\\' && i + 1 < encoded.size()) {
                        layout += (encoded[++i] == 'n') ? '\n' : encoded[i];
                    } else {
                        layout += encoded[i];
                    }
                }
                if (!layout.empty()) {
                    ImGui::LoadIniSettingsFromMemory(layout.c_str(),
                                                     layout.size());
                }
            }
            break;
        }
        default:
            break;
        }
    }

    switch (active) {
    case PendingPopup::About: {
        ImGui::OpenPopup("About PolyWorks");
        const ImVec2 c = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(c, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        if (ImGui::BeginPopupModal("About PolyWorks", nullptr,
                                   ImGuiWindowFlags_AlwaysAutoResize |
                                       ImGuiWindowFlags_NoSavedSettings)) {
            ImGui::TextUnformatted("Soldat PolyWorks");
            ImGui::TextUnformatted("Map editor for Soldat / OpenSoldat.");
            ImGui::Separator();
            ImGui::TextUnformatted("Original by Jeremy 'Ricochet' Ho.");
            ImGui::TextUnformatted("C++ / Dear ImGui port.");
            ImGui::Separator();
            if (ImGui::Button("OK", ImVec2(100.0f * app.uiScale(), 0.0f))) {
                ImGui::CloseCurrentPopup();
                active = PendingPopup::None;
            }
            ImGui::EndPopup();
        } else {
            active = PendingPopup::None;
        }
        break;
    }
    case PendingPopup::MapSettings:
        drawMapSettings(app, justOpened);
        if (!ImGui::IsPopupOpen("Map Settings")) {
            active = PendingPopup::None;
        }
        break;
    case PendingPopup::Preferences:
        drawPreferences(app, justOpened);
        if (!ImGui::IsPopupOpen("Preferences")) {
            active = PendingPopup::None;
        }
        break;
    default:
        break;
    }

    drawMessageBox(app);
    processMessageAnswer(app);
}

}  // namespace pw
