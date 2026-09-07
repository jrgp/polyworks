#include "gui/mainframe.h"
#include "gui/panels/tools_panel.h"
#include "gui/panels/display_panel.h"
#include "gui/panels/info_panel.h"

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

class PolyWorksApp final : public wxApp {
public:
    bool OnInit() override {
        SetAppName("PolyWorks");
        wxInitAllImageHandlers();

        const wxString skinsPath = getSkinsPath();

        auto* mainFrame = new MainFrame(skinsPath);
        mainFrame->Show(true);

        const wxPoint framePos  = mainFrame->GetPosition();
        const wxSize  frameSize = mainFrame->GetSize();

        /* Tools panel: floating to the right of main window */
        auto* toolsPanel = new ToolsPanel(mainFrame, skinsPath);
        mainFrame->AttachToolsPanel(toolsPanel);
        toolsPanel->SetPosition(wxPoint(framePos.x + frameSize.x + 4, framePos.y));
        toolsPanel->Show(true);

        /* Display panel: visibility toggles */
        auto* displayPanel = new DisplayPanel(mainFrame, mainFrame->m_doc.viewSettings);
        mainFrame->AttachDisplayPanel(displayPanel);
        displayPanel->SetPosition(wxPoint(framePos.x + frameSize.x + 4, framePos.y + 256));
        displayPanel->Show(true);

        /* Info panel: live entity counts */
        auto* infoPanel = new InfoPanel(mainFrame, mainFrame->m_doc);
        mainFrame->AttachInfoPanel(infoPanel);
        infoPanel->SetPosition(wxPoint(framePos.x + frameSize.x + 4, framePos.y + 540));
        infoPanel->Show(true);

        return true;
    }
};

}  // namespace

wxIMPLEMENT_APP(PolyWorksApp);
