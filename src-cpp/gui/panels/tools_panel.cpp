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
              wxDefaultSize,
              /* frmTools carries a picTitle strip whose MouseDown forwards
                 WM_NCLBUTTONDOWN to the form (frmTools.frm:467), i.e. the strip
                 exists purely so the window can be dragged, and picHide beside
                 it closes it.  A caption and a close box are the portable
                 equivalent.  Without them the window had no title bar at all,
                 which on Windows means it cannot be moved: there is nothing to
                 grab.  wxFRAME_TOOL_WINDOW keeps the slim caption. */
              wxFRAME_TOOL_WINDOW | wxFRAME_NO_TASKBAR | wxCAPTION |
              wxCLOSE_BOX | wxFRAME_FLOAT_ON_PARENT),
      m_mainFrame(mainFrame) {
    SetBackgroundColour(wxColour(0x4A, 0x3C, 0x31));

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
    /* Fit to the buttons rather than forcing a 64x240 client area.  The frame
       used to be sized before its children existed, using the original's pixel
       dimensions; but a wxBitmapButton is not 32x32 on every platform - MSW
       adds its own margins - so the grid could end up wider than the window
       and be clipped, which is what made the panel look truncated on Windows.
       Fitting keeps the original's proportions where the metrics agree and
       stays correct where they do not. */
    SetSizerAndFit(root);
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
