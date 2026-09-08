#include "tools_panel.h"

#include "gui/mainframe.h"

#include <wx/bmpbuttn.h>
#include <wx/filefn.h>
#include <wx/filename.h>
#include <wx/image.h>
#include <wx/dcmemory.h>
#include <wx/sizer.h>
#include <wx/panel.h>

namespace {
constexpr int kToolButtonBaseId = wxID_HIGHEST + 500;
constexpr int kToolCount = 14;
constexpr int kButtonSize = 32;
}

ToolsPanel::ToolsPanel(MainFrame* mainFrame, const wxString& skinsPath)
    : wxFrame(mainFrame,
              wxID_ANY,
              "Tools",
              wxDefaultPosition,
              wxSize(64, 240),
              wxFRAME_TOOL_WINDOW | wxFRAME_NO_TASKBAR | wxBORDER_SIMPLE | wxFRAME_FLOAT_ON_PARENT),
      m_mainFrame(mainFrame) {
    SetBackgroundColour(wxColour(0x4A, 0x3C, 0x31));
    SetClientSize(wxSize(64, 240));

    LoadToolBitmaps(skinsPath);

    auto* root = new wxBoxSizer(wxVERTICAL);
    root->AddSpacer(16);

    auto* grid = new wxGridSizer(7, 2, 0, 0);
    for (int i = 0; i < kToolCount; ++i) {
        auto* button = new wxBitmapButton(this,
                                          kToolButtonBaseId + i,
                                          m_normalBitmaps[i],
                                          wxDefaultPosition,
                                          wxSize(kButtonSize, kButtonSize),
                                          wxBORDER_NONE);
        button->SetBackgroundColour(wxColour(0x4A, 0x3C, 0x31));
        button->SetBitmapPressed(m_selectedBitmaps[i]);
        button->SetToolTip(GetToolName(i) + " (" + GetToolHotkey(i) + ")");
        grid->Add(button, 0, wxEXPAND);
        m_buttons[i] = button;
    }

    root->Add(grid, 1, wxEXPAND);
    SetSizer(root);
    /* The frame was given its final size before the buttons existed, so no
       resize follows SetSizer() to trigger a layout.  GTK lays out anyway;
       Win32 does not, leaving all fourteen buttons stacked at (0,0) with only
       one visible.  Lay out explicitly. */
    Layout();
    Bind(wxEVT_BUTTON, &ToolsPanel::OnToolClicked, this);
}

void ToolsPanel::LoadToolBitmaps(const wxString& skinsPath) {
    const wxString bitmapPath = wxFileName(skinsPath, "tool_gfx.bmp").GetFullPath();
    const bool hasSpriteSheet = !skinsPath.empty() && wxFileExists(bitmapPath);

    if (hasSpriteSheet) {
        wxImage image(bitmapPath, wxBITMAP_TYPE_BMP);
        if (image.IsOk()) {
            for (int i = 0; i < kToolCount; ++i) {
                const int y = i * kButtonSize;
                m_normalBitmaps[i] = wxBitmap(image.GetSubImage(wxRect(0, y, kButtonSize, kButtonSize)));
                m_selectedBitmaps[i] = wxBitmap(image.GetSubImage(wxRect(64, y, kButtonSize, kButtonSize)));
            }
            return;
        }
    }

    for (int i = 0; i < kToolCount; ++i) {
        m_normalBitmaps[i] = CreatePlaceholderBitmap(i, false);
        m_selectedBitmaps[i] = CreatePlaceholderBitmap(i, true);
    }
}

wxBitmap ToolsPanel::CreatePlaceholderBitmap(int tool, bool selected) const {
    wxBitmap bitmap(kButtonSize, kButtonSize);
    wxMemoryDC dc(bitmap);

    const wxColour background = selected ? wxColour(0xC8, 0x8B, 0x46) : wxColour(0x6B, 0x58, 0x46);
    dc.SetBackground(wxBrush(background));
    dc.Clear();

    dc.SetPen(wxPen(selected ? *wxWHITE : *wxBLACK, selected ? 3 : 1));
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    dc.DrawRectangle(1, 1, kButtonSize - 2, kButtonSize - 2);

    dc.SetTextForeground(*wxWHITE);
    dc.DrawLabel(wxString::Format("%d", tool), wxRect(0, 0, kButtonSize, kButtonSize), wxALIGN_CENTER);
    dc.SelectObject(wxNullBitmap);
    return bitmap;
}

void ToolsPanel::OnToolClicked(wxCommandEvent& event) {
    const int tool = event.GetId() - kToolButtonBaseId;
    if (tool < 0 || tool >= kToolCount || m_mainFrame == nullptr) {
        return;
    }

    m_mainFrame->SetActiveTool(tool);
}

void ToolsPanel::SetActiveTool(int tool) {
    if (tool < 0 || tool >= kToolCount) {
        return;
    }

    m_activeTool = tool;
    for (int i = 0; i < kToolCount; ++i) {
        if (m_buttons[i] == nullptr) {
            continue;
        }

        m_buttons[i]->SetBitmapLabel(i == m_activeTool ? m_selectedBitmaps[i] : m_normalBitmaps[i]);
        m_buttons[i]->Refresh();
    }
}
