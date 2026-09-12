#pragma once
/*
 * app.h — the GLFW window, the Dear ImGui context and the frame loop.
 *
 * This is the only file that knows a windowing system exists.  Everything
 * below it (Editor, Interaction, MapDocument, Renderer) is driven through
 * plain calls, which is what lets the interaction machine and the coordinate
 * pipeline be exercised without a display.
 */

#include "editor.h"
#include "theme.h"

#include <array>
#include <string>
#include <vector>

struct GLFWwindow;
struct GLFWcursor;

namespace pw {

/* One popup that the current frame should open.  ImGui popups must be opened
   from inside the window that owns them, but the decision is often made
   elsewhere (a menu item, a key press), so requests are queued here. */
enum class PendingPopup {
    None,
    OpenMap,
    SaveMapAs,
    CompileAs,
    OpenCompiled,
    ExportPrefab,
    ImportPrefab,
    LoadWorkspace,
    SaveWorkspace,
    MapSettings,
    Preferences,
    About,
};

class App {
public:
    bool initialise(int argc, char** argv);
    void run();
    void shutdown();

    Editor& editor() { return m_editor; }

    /* Queue a modal for the next frame. */
    void openPopup(PendingPopup popup) { m_pendingPopup = popup; }
    PendingPopup takePendingPopup();

    /* Skin bitmap sheets, uploaded once.  0 when the skin is unavailable. */
    unsigned int toolSheet() const { return m_toolSheet; }
    int toolSheetCols() const { return m_toolSheetCols; }
    int toolSheetRows() const { return m_toolSheetRows; }

    float uiScale() const { return m_uiScale; }

    /* Resolve a texture by name and return a GL id usable by ImGui::Image,
       uploading it on first use.  Used by the Scenery preview and the Texture
       window, which must show exactly what the renderer shows. */
    unsigned int previewTexture(const std::string& filename, int& w, int& h);

    void requestQuit();

    /* The native file dialogs need a parent window to be modal to. */
    GLFWwindow* window() const { return m_window; }

    /* The viewport context menu currently open, if any.  Held by App because
       the popup is raised by the input handler but drawn by the frame loop,
       and ImGui keeps a popup open only while its Begin is reached. */
    void setContextMenu(const char* id, Vec2 world) {
        m_contextMenuId = id;
        m_contextMenuWorld = world;
    }
    const char* contextMenuId() const { return m_contextMenuId; }
    Vec2 contextMenuWorld() const { return m_contextMenuWorld; }

private:
    void buildFrame();
    void drawViewport();
    void handleViewportInput();
    void handleShortcuts();
    void drawStatusBar(float height);
    void updateCursor();
    void loadSkinGraphics();
    void loadCursors();
    void updateScale();

    GLFWwindow* m_window = nullptr;
    Editor      m_editor;
    SkinColors  m_skin;

    float m_uiScale = 1.0f;          /* logical units per unscaled UI unit */
    float m_fbScale = 1.0f;          /* framebuffer pixels per logical unit */
    float m_appliedUiScale = 0.0f;   /* what the style was last built for */

    unsigned int m_toolSheet = 0;
    int m_toolSheetCols = 0;
    int m_toolSheetRows = 0;

    std::array<GLFWcursor*, TOOL_COUNT> m_cursors{};
    GLFWcursor* m_panCursor = nullptr;

    PendingPopup m_pendingPopup = PendingPopup::None;

    const char* m_contextMenuId = nullptr;
    Vec2  m_contextMenuWorld{};

    float m_wheelAccum = 0.0f;
    bool  m_quitRequested = false;
};

/* Drawn by the other UI translation units. */
void drawMenuBar(App& app);
void drawPanels(App& app);
void drawDialogs(App& app);
/* Raise the popup appropriate to the active tool for a right-click at
   `clickWorld`; draw whichever popup is open. */
void drawViewportContextMenu(App& app, Vec2 clickWorld);
void drawOpenContextMenu(App& app);

}  // namespace pw
