#pragma once
/*
 * editor.h — everything the application is, minus the pixels.
 *
 * The Dear ImGui port has no widget objects to hang state on, so the state the
 * wxWidgets version kept scattered across MainFrame and seven wxFrame
 * subclasses lives here instead, in one place, owned by one object.  The UI
 * files (menus.cpp, panels.cpp, dialogs.cpp) are pure functions of this state:
 * they read it, draw it, and call back into the command methods below.
 *
 * That arrangement is not architectural taste.  The wx implementation kept two
 * copies of several settings -- the Scenery panel's rotate/scale/level, and the
 * paint colour -- and they drifted apart, so panel controls silently had no
 * effect.  With a single owner that class of bug cannot be written.
 *
 * Nothing here includes imgui.h; the state is drawable by anything.
 */

#include "map_document.h"
#include "undo_stack.h"
#include "renderer.h"
#include "texture_manager.h"
#include "viewport_geometry.h"
#include "prefs.h"
#include "message_box.h"
#include "interaction.h"

#include <array>
#include <deque>
#include <string>
#include <vector>

namespace pw {

/* ---- Palette (frmPalette) ---------------------------------------------- */

struct PaletteColor {
    uint8_t r = 0, g = 0, b = 0;
};

struct PaletteState {
    /* 12 columns x 6 rows of swatches, exactly as frmPalette's picPalette. */
    static constexpr int kCols = 12;
    static constexpr int kRows = 6;
    std::array<PaletteColor, kCols * kRows> cells{};
    int selCol = -1;
    int selRow = -1;

    /* The colour the painting tools use. */
    uint8_t r = 255, g = 255, b = 255;
    float   opacity = 1.0f;
    /* 0 Normal, 1 Multiply, 2 Screen, 3 Darken, 4 Lighten, 5 Difference --
       the six names stored in frmPalette.frx at offset 0x16. */
    int     blendMode = 0;
    /* frmPalette's three colour modes (frm:1120): 0 precision, 1 normal,
       2 dynamic.  modConfig.bas:149 defaults to 1. */
    int     colorMode = 1;
    int     radius = 8;

    /* <appPath>/palettes/current.txt, written on exit (modConfig.bas:389). */
    static std::string palettesDir();
    static std::string currentPalettePath();
    /* Where the user's own palette is written; the same directory in a
       portable install, a per-user one when the application is read-only. */
    static std::string userPalettePath();
    bool load(const std::string& path);
    bool save(const std::string& path) const;
    /* Park the selection marker on the swatch holding this colour, or clear it
       when none does (frmPalette.CheckPalette, frm:99-121). */
    void checkPalette(uint8_t r, uint8_t g, uint8_t b);
};

/* ---- Scenery placement (frmScenery) ------------------------------------ */

struct SceneryState {
    std::vector<std::string> available;  /* Scenery-gfx directory listing */
    std::vector<std::string> inUse;      /* names referenced by the map */
    int  selected = -1;                  /* index into `available` */
    int  level    = 1;                   /* 0 back, 1 middle, 2 front */
    bool rotate   = false;
    bool scale    = false;
    std::string dir;                     /* directory `available` came from */
};

/* ---- Waypoint placement (frmWaypoints) --------------------------------- */

struct WaypointState {
    /* Left, Right, Up, Down, Jet -- the order of mnuWayType (frm:636-655). */
    bool  type[5] = {false, false, false, false, false};
    int   pathNum = 0;
    int   special = 0;
};

/* ---- Texture window (frmTexture) --------------------------------------- */

struct TextureWindowState {
    std::string  path;         /* resolved file backing the preview */
    unsigned int texId = 0;    /* GL texture, uploaded lazily */
    int   width = 0, height = 0;
    /* Normalised selection rectangle used by "User Defined X/Y". */
    bool  hasSelection = false;
    float u1 = 0, v1 = 0, u2 = 1, v2 = 1;
    float zoom = 1.0f;
};

/* Which floating tool windows are on screen (VB6 Window menu / workspace). */
struct PanelVisibility {
    bool tools      = true;
    bool display    = true;
    bool scenery    = true;
    bool waypoints  = true;
    bool properties = true;
    bool palette    = true;
    bool texture    = false;
};


class Editor {
public:
    Editor();

    /* ---- owned state --------------------------------------------------- */
    MapDocument doc;
    UndoStack   undo;
    AppPrefs    prefs;
    Renderer    renderer;
    TextureManager texMgr;

    ViewportGeometry viewport;
    Interaction      interaction{*this};

    PaletteState    palette;
    SceneryState    sceneryState;
    WaypointState   waypointState;
    TextureWindowState textureWindow;
    PanelVisibility panels;
    MessageBox      messageBox;

    /* VB6 `currentTool`: the tool chosen in frmTools, before modifiers. */
    int activeTool = 0;
    /* VB6 `polyType`: the type stamped onto newly created polygons. */
    int creationPolyType = 0;
    /* VB6 `currentTeam`: which spawn/collider the Objects tool places. */
    int currentSpawnTeam = 0;
    /* mnuCustomX / mnuCustomY: take new polygons' UVs from the Texture window. */
    bool customTexX = false;
    bool customTexY = false;

    std::string currentFilePath;
    std::string skinsPath;
    Vec2 lastMouseWorld{};
    bool running = true;
    /* Set by any command that changes what should be on screen.  The frame
       loop redraws unconditionally, so this only drives cheap bookkeeping. */
    bool needsRedraw = true;
    /* Set when the view could not be centred because the viewport had no size
       yet; the frame loop clears it once it has. */
    bool viewResetPending = false;

    /* ---- lifecycle ----------------------------------------------------- */
    void initialise(const std::string& skinsPath);
    void shutdown();

    /* ---- file ---------------------------------------------------------- */
    void newMap();
    bool loadMap(const std::string& path);
    bool saveMap(const std::string& path);
    bool saveCurrent();
    bool compileTo(const std::string& path);
    bool exportPrefab(const std::string& path);
    bool importPrefab(const std::string& path);
    void copySelection();
    void pasteSelection();
    bool openCommandLineMap(const std::string& arg);
    void runGame(bool openSoldat);
    /* True when it is safe to discard the document.  When it is not, a save
       prompt is raised and `pendingAfterPrompt` is run once answered. */
    bool confirmDiscardChanges(std::function<void()> continuation);

    /* ---- edit ---------------------------------------------------------- */
    void undoAction();
    void redoAction();
    void deleteSelected();
    void duplicateSelected();
    void selectAll();
    void deselect();
    void invertSelection();
    void selectByColor();
    void severConnections();
    void clearSketch();
    void transformSelection(int which);   /* see kTransform* in editor.cpp */
    void polyOperation(int which);
    void texTransform(int which);
    void applyLightToVertices();
    void snapSelectedVertices();
    void arrangeSelected(int which);
    void nudgeSelection(float dirX, float dirY, bool fine);

    /* ---- view ---------------------------------------------------------- */
    void zoomIn();
    void zoomOut();
    void zoomReset();
    void centerAndReset();
    void fitOnScreen();

    /* ---- tools --------------------------------------------------------- */
    void setActiveTool(int tool);
    void toggleWaypointType(int idx);

    /* ---- assets -------------------------------------------------------- */
    void registerAppAssetPaths();
    void registerAssetPathsForMap(const std::string& mapPath);
    void applyPrefs();
    void loadPrefs();
    void savePrefs();
    void refreshSceneryList();
    void refreshSceneryInUse();
    void refreshTextureWindow();
    /* Size of the texture used by the first selected polygon, or {0,0}. */
    void selectedTextureSize(int& w, int& h);
    /* Index in doc.sceneryNames for the scenery chosen in the panel, adding it
       when the map does not reference it yet.  0 means "nothing chosen". */
    int  orAddSelectedSceneryIndex();
    /* Paint colour handed to the colouring tools. */
    void setPaintColorFromPicker(uint8_t r, uint8_t g, uint8_t b);

    /* ---- recent files -------------------------------------------------- */
    static constexpr int kMaxRecentFiles = 10;
    std::deque<std::string> recentFiles;
    void addToRecentFiles(const std::string& path);

    /* ---- status -------------------------------------------------------- */
    std::string windowTitle() const;
    std::string statusToolLabel() const;
    std::string documentName() const;

    /* Raise a modal message box on the next frame. */
    void showMessage(MessageBox::Kind kind, const std::string& title,
                     const std::string& text);

    /* Continuation run when a save prompt is answered, so that File>Open can
       resume after the user chooses. */
    std::function<void()> pendingAfterPrompt;

public:
    void resetViewForLoadedMap();

private:
    std::string clipboardPrefabPath() const;
    /* The game directory the texture search paths were last built from. */
    std::string m_appliedGameDir;
};

/* Names shown in frmTools and in the status bar.  Index is the VB6 TOOL_*
   constant. */
const char* toolName(int tool);
const char* toolHotkey(int tool);
/* The caption lblCurrentTool shows for an *effective* function, i.e. after
   modifier keys have been applied (frm:1655-1683). */
const char* functionName(int fn);

}  // namespace pw
