#include "gui/mainframe.h"
#include "gui/panels/tools_panel.h"
#include "gui/panels/display_panel.h"
#include "gui/panels/info_panel.h"
#include "gui/panels/scenery_panel.h"
#include "gui/panels/waypoint_panel.h"
#include "gui/panels/palette_panel.h"

#include <wx/app.h>
#include <wx/image.h>
#include <wx/filename.h>
#include <wx/stdpaths.h>

namespace {

wxString getSkinsPath() {
    const wxFileName exeName(wxStandardPaths::Get().GetExecutablePath());
    const wxString exeDir = exeName.GetPath();

    const wxString exeRelative = wxFileName(exeDir, "skins/default").GetFullPath();
    if (wxFileName::DirExists(exeRelative)) {
        return exeRelative;
    }

    const wxString devRelative = wxFileName(wxFileName::GetCwd(), "installer/skins/default").GetFullPath();
    if (wxFileName::DirExists(devRelative)) {
        return devRelative;
    }

    return {};
}

/* Detect the Soldat game data directory (for scenery/textures). */
wxString getSoldatPath(const wxString& skinsPath) {
    /* installer/skins/default → installer/ → that's a dev checkout. */
    /* Real Soldat path would be found via config; fall back to a sibling dir. */
    if (!skinsPath.empty()) {
        wxFileName fn(skinsPath);
        fn.RemoveLastDir();  /* default */
        fn.RemoveLastDir();  /* skins */
        return fn.GetPath();
    }
    return {};
}

class PolyWorksApp final : public wxApp {
public:
    bool OnInit() override {
        SetAppName("PolyWorks");
        wxInitAllImageHandlers();

        const wxString skinsPath  = getSkinsPath();
        const wxString soldatPath = getSoldatPath(skinsPath);

        auto* mainFrame = new MainFrame(skinsPath);
        mainFrame->Show(true);

        const wxPoint framePos  = mainFrame->GetPosition();
        const wxSize  frameSize = mainFrame->GetSize();
        const int     rightX    = framePos.x + frameSize.x + 4;

        /* Tools panel — right of main window */
        auto* toolsPanel = new ToolsPanel(mainFrame, skinsPath);
        mainFrame->AttachToolsPanel(toolsPanel);
        toolsPanel->SetPosition(wxPoint(rightX, framePos.y));
        toolsPanel->Show(true);

        /* Display panel — below tools */
        auto* displayPanel = new DisplayPanel(mainFrame, mainFrame->m_doc.viewSettings);
        mainFrame->AttachDisplayPanel(displayPanel);
        displayPanel->SetPosition(wxPoint(rightX, framePos.y + 256));
        displayPanel->Show(true);

        /* Scenery panel */
        auto* sceneryPanel = new SceneryPanel(mainFrame, soldatPath);
        mainFrame->AttachSceneryPanel(sceneryPanel);
        sceneryPanel->SetPosition(wxPoint(rightX, framePos.y + 510));
        sceneryPanel->Show(true);

        /* Waypoint panel */
        auto* waypointPanel = new WaypointPanel(mainFrame);
        mainFrame->AttachWaypointPanel(waypointPanel);
        waypointPanel->SetPosition(wxPoint(rightX, framePos.y + 740));
        waypointPanel->Show(true);

        /* Info panel — below waypoints */
        auto* infoPanel = new InfoPanel(mainFrame, mainFrame->m_doc);
        mainFrame->AttachInfoPanel(infoPanel);
        infoPanel->SetPosition(wxPoint(rightX, framePos.y + 920));
        infoPanel->Show(true);

        /* Palette panel — shown on demand via View menu; initially hidden */
        auto* palettePanel = new PalettePanel(mainFrame);
        mainFrame->AttachPalettePanel(palettePanel);
        palettePanel->SetPosition(wxPoint(rightX + 216, framePos.y));
        /* Don't show by default — user opens via View > Color Palette */

        return true;
    }
};

}  // namespace

wxIMPLEMENT_APP(PolyWorksApp);
