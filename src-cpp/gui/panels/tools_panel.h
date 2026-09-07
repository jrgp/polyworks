#pragma once

#include <wx/frame.h>
#include <wx/bitmap.h>

#include <array>

class MainFrame;
class wxBitmapButton;

class ToolsPanel final : public wxFrame {
public:
    ToolsPanel(MainFrame* mainFrame, const wxString& skinsPath);

    void SetActiveTool(int tool);

private:
    void LoadToolBitmaps(const wxString& skinsPath);
    wxBitmap CreatePlaceholderBitmap(int tool, bool selected) const;
    void OnToolClicked(wxCommandEvent& event);

    MainFrame* m_mainFrame = nullptr;
    int m_activeTool = 0;
    std::array<wxBitmapButton*, 14> m_buttons{};
    std::array<wxBitmap, 14> m_normalBitmaps{};
    std::array<wxBitmap, 14> m_selectedBitmaps{};
};
