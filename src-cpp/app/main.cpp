#include "gui/mainframe.h"
#include "gui/panels/tools_panel.h"
#include "gui/panels/display_panel.h"
#include "gui/panels/info_panel.h"
#include "gui/panels/texture_panel.h"
#include "gui/panels/scenery_panel.h"
#include "gui/panels/waypoint_panel.h"
#include "gui/panels/palette_panel.h"

#include <wx/app.h>
#include <wx/image.h>
#include <wx/filename.h>
#include <wx/stdpaths.h>
#include <wx/log.h>
#include <vector>
#include <wx/msgdlg.h>

#include <exception>
#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <cstring>

/* ---- Platform crash signal handler ------------------------------------- */
namespace {

void crashHandler(int sig) {
    const char* sigName = "unknown";
    switch (sig) {
    case SIGSEGV: sigName = "SIGSEGV (segmentation fault)"; break;
    case SIGABRT: sigName = "SIGABRT (abort)"; break;
    case SIGFPE:  sigName = "SIGFPE (floating point exception)"; break;
    case SIGILL:  sigName = "SIGILL (illegal instruction)"; break;
    default: break;
    }
    char msg[256];
    std::snprintf(msg, sizeof(msg),
        "\nPolyWorks fatal crash\n"
        "Signal: %s (%d)\n"
        "Please report this with your map and steps to reproduce.\n",
        sigName, sig);
    std::fputs(msg, stderr);
    /* Restore default handler and re-raise to get OS core dump */
    std::signal(sig, SIG_DFL);
    std::raise(sig);
}

void installCrashHandlers() {
    std::signal(SIGSEGV, crashHandler);
    std::signal(SIGABRT, crashHandler);
    std::signal(SIGFPE,  crashHandler);
    std::signal(SIGILL,  crashHandler);
}

} // namespace

namespace {

wxString getSkinsPath() {
    const wxFileName exeName(wxStandardPaths::Get().GetExecutablePath());
    const wxString exeDir = exeName.GetPath();

    /* NB: wxFileName(dir, name) treats `name` as a *file* name and asserts if
       it contains separators, so multi-segment suffixes must be appended as
       directories instead.  Getting this wrong silently yielded an empty skins
       path, which disabled notfound.bmp, every custom cursor and every skin
       bitmap. */
    auto dirBelow = [](const wxString& base,
                       const std::vector<wxString>& segments) {
        wxFileName fn = wxFileName::DirName(base);
        for (const wxString& seg : segments) {
            fn.AppendDir(seg);
        }
        return fn.GetPath();
    };

    const wxString exeRelative = dirBelow(exeDir, {"skins", "default"});
    if (wxFileName::DirExists(exeRelative)) {
        return exeRelative;
    }

    /* Bundled layout: PolyWorks.app/Contents/Resources/skins/default.  A Mac
       application keeps its data in Resources, not beside the executable, so
       a bundle dragged to /Applications finds its skins only through this. */
    const wxString bundleRelative =
        dirBelow(wxStandardPaths::Get().GetResourcesDir(), {"skins", "default"});
    if (wxFileName::DirExists(bundleRelative)) {
        return bundleRelative;
    }

    /* Installed layout: <prefix>/bin/polyworks with skins next to the prefix. */
    const wxString installRelative =
        dirBelow(wxFileName(exeDir, wxEmptyString).GetPath(),
                 {"share", "polyworks", "skins", "default"});
    if (wxFileName::DirExists(installRelative)) {
        return installRelative;
    }

    /* Development checkout: run from the repository root or from build/. */
    const wxString cwd = wxFileName::GetCwd();
    for (const std::vector<wxString>& rel :
         {std::vector<wxString>{"installer", "skins", "default"},
          std::vector<wxString>{"skins", "default"},
          std::vector<wxString>{"..", "installer", "skins", "default"}}) {
        const wxString candidate = dirBelow(cwd, rel);
        if (wxFileName::DirExists(candidate)) {
            return candidate;
        }
    }

    wxLogWarning("Could not locate the skins directory; custom cursors, skin "
                 "bitmaps and the notfound.bmp placeholder will be unavailable.");
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
        installCrashHandlers();
        SetAppName("PolyWorks");
        wxInitAllImageHandlers();

        const wxString skinsPath  = getSkinsPath();
        const wxString soldatPath = getSoldatPath(skinsPath);

        auto* mainFrame = new MainFrame(skinsPath);
        mainFrame->Show(true);

        const wxPoint framePos  = mainFrame->GetPosition();
        const wxSize  frameSize = mainFrame->GetSize();
        const int     rightX    = framePos.x + frameSize.x + 4;
        /* The tool windows form two columns to the right of the editor.  The
           second column has to clear the widest panel in the first one - the
           Display panel, whose two columns of checkboxes are wider under a
           desktop font than the original's 208px form. */
        const int     rightX2   = rightX + 240;

        /* Tools panel — right of main window */
        auto* toolsPanel = new ToolsPanel(mainFrame, skinsPath);
        mainFrame->AttachToolsPanel(toolsPanel);
        PlacePanelOnScreen(toolsPanel, wxPoint(rightX, framePos.y));
        toolsPanel->Show(true);

        /* Display panel — below tools */
        auto* displayPanel = new DisplayPanel(mainFrame, mainFrame->m_doc.viewSettings);
        mainFrame->AttachDisplayPanel(displayPanel);
        PlacePanelOnScreen(displayPanel, wxPoint(rightX, framePos.y + 256));
        displayPanel->Show(true);

        /* Scenery panel */
        auto* sceneryPanel = new SceneryPanel(mainFrame, soldatPath);
        mainFrame->AttachSceneryPanel(sceneryPanel);
        PlacePanelOnScreen(sceneryPanel, wxPoint(rightX, framePos.y + 510));
        sceneryPanel->Show(true);

        /* Waypoint panel */
        auto* waypointPanel = new WaypointPanel(mainFrame);
        mainFrame->AttachWaypointPanel(waypointPanel);
        PlacePanelOnScreen(waypointPanel, wxPoint(rightX, framePos.y + 740));
        waypointPanel->Show(true);

        /* Info panel — below waypoints */
        auto* infoPanel = new InfoPanel(mainFrame, mainFrame->m_doc, &mainFrame->m_undoStack);
        mainFrame->AttachInfoPanel(infoPanel);
        PlacePanelOnScreen(infoPanel, wxPoint(rightX, framePos.y + 920));
        infoPanel->Show(true);

        /* Texture panel (frmTexture) — shown on demand via Window > Texture */
        auto* texturePanel = new TexturePanel(mainFrame);
        mainFrame->AttachTexturePanel(texturePanel);
        PlacePanelOnScreen(texturePanel, wxPoint(rightX2, framePos.y + 300));

        /* Palette panel.  The shipped workspace has [Palette] Visible=True
           (installer/Workspace/current.ini) and Form_Load shows it with the
           other tool windows (frm:10608), so it is up from the start: it is
           where the colour used for filling and vertex painting is chosen. */
        auto* palettePanel = new PalettePanel(mainFrame);
        mainFrame->AttachPalettePanel(palettePanel);
        PlacePanelOnScreen(palettePanel, wxPoint(rightX2, framePos.y));
        palettePanel->Show(true);

        mainFrame->SyncWindowMenu();

        /* VB6 Form_Load opens a map named on the command line, which is how
           the .pms file association works (installer/pw.nsi:185 registers
           `"OpenSoldat PolyWorks.exe" "%1"`). */
        if (argc > 1) {
            mainFrame->OpenCommandLineMap(argv[1]);
        }

        return true;
    }

    bool OnExceptionInMainLoop() override {
        try {
            throw;
        } catch (const std::exception& e) {
            wxString msg = wxString::Format(
                "Unhandled exception in main loop:\n%s\n\n"
                "PolyWorks must close. Save your work first if possible.",
                wxString::FromUTF8(e.what()));
            wxLogError("%s", msg);
            wxMessageBox(msg, "PolyWorks Internal Error", wxOK | wxICON_ERROR);
        } catch (...) {
            wxLogError("Unknown exception in main loop.");
            wxMessageBox("An unknown internal error occurred.\n"
                         "PolyWorks must close.",
                         "PolyWorks Internal Error", wxOK | wxICON_ERROR);
        }
        return false; /* terminate */
    }

    void OnUnhandledException() override {
        try {
            throw;
        } catch (const std::exception& e) {
            std::fprintf(stderr, "PolyWorks: unhandled exception: %s\n", e.what());
        } catch (...) {
            std::fprintf(stderr, "PolyWorks: unknown unhandled exception\n");
        }
        std::abort();
    }
};

}  // namespace

wxIMPLEMENT_APP(PolyWorksApp);
