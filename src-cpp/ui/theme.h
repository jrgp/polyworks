#pragma once
/*
 * theme.h — PolyWorks' appearance, taken from the skin it has always shipped.
 *
 * The original draws its own chrome, reading the colours from
 * skins/<skin>/colors.ini at startup (frm:4648) rather than hard-coding them,
 * which is what makes the shipped skins work.  So the theme is loaded from the
 * same file rather than reproduced by eye, and the application never shows
 * Dear ImGui's default blue-grey.
 *
 * The file's notation is NOT RGB.  HexToLong (modOSME.bas:833) turns the hex
 * into a VB6 Long that is then assigned to a control's BackColor, and a VB6
 * OLE_COLOR is &HBBGGRR -- blue in the high byte.  The shipped skin's
 * Background=4A3C31 is therefore RGB(0x31, 0x3C, 0x4A), the dark slate blue of
 * the screenshot in the README, and reading it as RGB gives brown instead.
 *
 * The render colours in polyworks.ini go through the same HexToLong but are
 * *not* byte-swapped: ARGB() (modUtils.bas:81) only prefixes an alpha byte and
 * hands the result to Direct3D, which reads it as A,R,G,B.  PointColor=CE4D4A
 * really is red.  The two families differ, so only this one is swapped.
 */

#include <cstdint>
#include <string>

namespace pw {

struct SkinColors {
    /* 0xRRGGBB, already converted from the file's BGR notation. */
    uint32_t background  = 0x313C4A;   /* file: 4A3C31 */
    uint32_t labelBack   = 0x3D4B61;   /* file: 614B3D */
    uint32_t labelText   = 0xFFFFFF;
    uint32_t textBoxBack = 0xFFFFFF;
    uint32_t textBoxText = 0x000000;
    uint32_t frame       = 0x0D3C0B;   /* file: 0B3C0D */
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
