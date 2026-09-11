#pragma once
/*
 * theme.h — PolyWorks' appearance, taken from the skin it has always shipped.
 *
 * The original draws its own chrome: every form has BackColor 0x4A3C31 (a dark
 * brown), labels sit on 0x614B3D, text boxes are white-on-black, and frames are
 * the dark green 0x0B3C0D.  Those five values are not hard-coded in the VB6
 * source -- they are read from skins/<skin>/colors.ini at startup
 * (modConfig.bas LoadSkin), which is what makes the shipped skins work.
 *
 * So the theme is loaded from the same file rather than reproduced by eye.  A
 * skin that changes colors.ini changes PolyWorks, exactly as before, and the
 * application never shows Dear ImGui's default blue-grey.
 */

#include <cstdint>
#include <string>

namespace pw {

struct SkinColors {
    /* 0xRRGGBB, matching the file's own notation. */
    uint32_t background  = 0x4A3C31;
    uint32_t labelBack   = 0x614B3D;
    uint32_t labelText   = 0xFFFFFF;
    uint32_t textBoxBack = 0xFFFFFF;
    uint32_t textBoxText = 0x000000;
    uint32_t frame       = 0x0B3C0D;
    std::string font1 = "Arial";
    std::string font2 = "Arial";
};

/* Read <skinsPath>/colors.ini.  Missing keys keep the defaults above, which
   are the values the shipped "default" skin contains. */
SkinColors loadSkinColors(const std::string& skinsPath);

/* Apply the skin to the current ImGui context.  `uiScale` is the DPI scale the
   window reported; sizes are expressed in unscaled units and multiplied here,
   so the UI is the same physical size at every Windows scaling factor. */
void applyTheme(const SkinColors& colors, float uiScale);

}  // namespace pw
