#include "gui/mainframe.h"
#include "gui/panels/tools_panel.h"

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

        auto* toolsPanel = new ToolsPanel(mainFrame, skinsPath);
        mainFrame->AttachToolsPanel(toolsPanel);
        toolsPanel->SetActiveTool(mainFrame->GetActiveTool());

        const wxPoint framePos = mainFrame->GetPosition();
        const wxSize frameSize = mainFrame->GetSize();
        toolsPanel->SetPosition(wxPoint(framePos.x + frameSize.x + 8, framePos.y));
        toolsPanel->Show(true);

        return true;
    }
};

}  // namespace

wxIMPLEMENT_APP(PolyWorksApp);
