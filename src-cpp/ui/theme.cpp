#include "theme.h"

#include "ini_file.h"

#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>

namespace pw {
namespace {

/* colors.ini stores a VB6 OLE_COLOR, which is &HBBGGRR: the value goes through
   HexToLong (modOSME.bas:833) straight into a control's BackColor.  Swap the
   outer bytes to get the RGB the rest of this file works in. */
uint32_t parseHex(const std::string& s, uint32_t fallback) {
    if (s.empty()) {
        return fallback;
    }
    try {
        const uint32_t bgr =
            static_cast<uint32_t>(std::stoul(s, nullptr, 16)) & 0xFFFFFFu;
        return ((bgr & 0xFFu) << 16) | (bgr & 0xFF00u) | ((bgr >> 16) & 0xFFu);
    } catch (const std::exception&) {
        return fallback;
    }
}

ImVec4 rgb(uint32_t c, float alpha = 1.0f) {
    return ImVec4(static_cast<float>((c >> 16) & 0xFF) / 255.0f,
                  static_cast<float>((c >> 8) & 0xFF) / 255.0f,
                  static_cast<float>(c & 0xFF) / 255.0f, alpha);
}

/* Mix towards white (t > 0) or black (t < 0).  The skin only names six
   colours, but a widget set needs hover and active variants of each; deriving
   them keeps every state recognisably the skin's colour. */
ImVec4 shade(uint32_t c, float t, float alpha = 1.0f) {
    const float target = t >= 0.0f ? 1.0f : 0.0f;
    const float k = std::fabs(t);
    ImVec4 v = rgb(c, alpha);
    v.x += (target - v.x) * k;
    v.y += (target - v.y) * k;
    v.z += (target - v.z) * k;
    return v;
}

}  // namespace

SkinColors loadSkinColors(const std::string& skinsPath) {
    SkinColors out;
    if (skinsPath.empty()) {
        return out;
    }
    IniFile ini;
    ini.load((std::filesystem::path(skinsPath) / "colors.ini").string());

    out.background  = parseHex(ini.readString("GUIColors", "Background"), out.background);
    out.labelBack   = parseHex(ini.readString("GUIColors", "LabelBack"), out.labelBack);
    out.labelText   = parseHex(ini.readString("GUIColors", "LabelText"), out.labelText);
    out.textBoxBack = parseHex(ini.readString("GUIColors", "TextBoxBack"), out.textBoxBack);
    out.textBoxText = parseHex(ini.readString("GUIColors", "TextBoxText"), out.textBoxText);
    out.frame       = parseHex(ini.readString("GUIColors", "Frame"), out.frame);
    out.font1 = ini.readString("GUIColors", "Font1", out.font1);
    out.font2 = ini.readString("GUIColors", "Font2", out.font2);
    return out;
}

void applyTheme(const SkinColors& c, float uiScale) {
    ImGuiStyle style;   /* start from the defaults, then overwrite */

    /* The original's forms are flat rectangles with square corners and a thin
       border -- VB6 picture boxes, essentially.  Rounded, translucent panels
       would read as a different application. */
    style.WindowRounding    = 0.0f;
    style.ChildRounding     = 0.0f;
    style.FrameRounding     = 0.0f;
    style.PopupRounding     = 0.0f;
    style.ScrollbarRounding = 0.0f;
    style.GrabRounding      = 0.0f;
    style.TabRounding       = 0.0f;
    style.WindowBorderSize  = 1.0f;
    style.FrameBorderSize   = 1.0f;
    style.PopupBorderSize   = 1.0f;
    style.WindowPadding     = ImVec2(6, 6);
    style.FramePadding      = ImVec2(4, 2);
    style.ItemSpacing       = ImVec2(6, 4);
    style.ItemInnerSpacing  = ImVec2(4, 4);
    style.IndentSpacing     = 16.0f;
    style.ScrollbarSize     = 13.0f;
    style.GrabMinSize       = 10.0f;
    style.WindowTitleAlign  = ImVec2(0.0f, 0.5f);

    ImVec4* col = style.Colors;
    col[ImGuiCol_Text]                 = rgb(c.labelText);
    col[ImGuiCol_TextDisabled]         = shade(c.labelText, -0.45f);
    col[ImGuiCol_WindowBg]             = rgb(c.background);
    col[ImGuiCol_ChildBg]              = rgb(c.background);
    col[ImGuiCol_PopupBg]              = rgb(c.background);
    col[ImGuiCol_Border]               = rgb(c.frame);
    col[ImGuiCol_BorderShadow]         = ImVec4(0, 0, 0, 0);
    col[ImGuiCol_FrameBg]              = rgb(c.textBoxBack);
    col[ImGuiCol_FrameBgHovered]       = shade(c.textBoxBack, -0.08f);
    col[ImGuiCol_FrameBgActive]        = shade(c.textBoxBack, -0.16f);
    col[ImGuiCol_TitleBg]              = rgb(c.labelBack);
    col[ImGuiCol_TitleBgActive]        = shade(c.labelBack, 0.12f);
    col[ImGuiCol_TitleBgCollapsed]     = shade(c.labelBack, -0.25f);
    col[ImGuiCol_MenuBarBg]            = rgb(c.labelBack);
    col[ImGuiCol_ScrollbarBg]          = shade(c.background, -0.2f);
    col[ImGuiCol_ScrollbarGrab]        = rgb(c.labelBack);
    col[ImGuiCol_ScrollbarGrabHovered] = shade(c.labelBack, 0.15f);
    col[ImGuiCol_ScrollbarGrabActive]  = shade(c.labelBack, 0.3f);
    /* The tick sits on the text-box background, not on the panel, so it has
       to take the text-box foreground colour: the skin's label text is light
       and would be invisible against white. */
    col[ImGuiCol_CheckMark]            = rgb(c.textBoxText);
    col[ImGuiCol_SliderGrab]           = rgb(c.labelBack);
    col[ImGuiCol_SliderGrabActive]     = shade(c.labelBack, 0.25f);
    col[ImGuiCol_Button]               = rgb(c.labelBack);
    col[ImGuiCol_ButtonHovered]        = shade(c.labelBack, 0.18f);
    col[ImGuiCol_ButtonActive]         = shade(c.labelBack, -0.18f);
    col[ImGuiCol_Header]               = shade(c.labelBack, 0.1f);
    col[ImGuiCol_HeaderHovered]        = shade(c.labelBack, 0.25f);
    col[ImGuiCol_HeaderActive]         = shade(c.labelBack, -0.1f);
    col[ImGuiCol_Separator]            = rgb(c.frame);
    col[ImGuiCol_SeparatorHovered]     = shade(c.frame, 0.3f);
    col[ImGuiCol_SeparatorActive]      = shade(c.frame, 0.5f);
    col[ImGuiCol_ResizeGrip]           = shade(c.labelBack, 0.0f, 0.5f);
    col[ImGuiCol_ResizeGripHovered]    = shade(c.labelBack, 0.2f);
    col[ImGuiCol_ResizeGripActive]     = shade(c.labelBack, 0.4f);
    col[ImGuiCol_Tab]                  = rgb(c.labelBack);
    col[ImGuiCol_TabHovered]           = shade(c.labelBack, 0.25f);
    col[ImGuiCol_TabSelected]          = shade(c.labelBack, 0.12f);
    col[ImGuiCol_TabDimmed]            = shade(c.labelBack, -0.3f);
    col[ImGuiCol_TabDimmedSelected]    = shade(c.labelBack, -0.15f);
    col[ImGuiCol_TableHeaderBg]        = rgb(c.labelBack);
    col[ImGuiCol_TableBorderStrong]    = rgb(c.frame);
    col[ImGuiCol_TableBorderLight]     = shade(c.frame, 0.2f);
    col[ImGuiCol_TableRowBg]           = ImVec4(0, 0, 0, 0);
    col[ImGuiCol_TableRowBgAlt]        = shade(c.background, 0.05f);
    col[ImGuiCol_TextSelectedBg]       = shade(c.labelBack, 0.3f, 0.7f);
    col[ImGuiCol_DragDropTarget]       = rgb(c.labelText);
    col[ImGuiCol_NavCursor]            = rgb(c.labelText);
    col[ImGuiCol_ModalWindowDimBg]     = ImVec4(0, 0, 0, 0.45f);

    /* Text boxes are white with black text in the original, which means the
       text colour has to change inside them; ImGui has one global text colour,
       so input fields push ImGuiCol_Text themselves (see panels.cpp). */

    style.ScaleAllSizes(uiScale);
    ImGui::GetStyle() = style;
}

}  // namespace pw
